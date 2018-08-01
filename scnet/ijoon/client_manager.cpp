#include "client_manager.h"
#include "registry.h"

ijoon::ClientManager::ClientManager()
: onAttaching(nullptr), onAttachFailed(nullptr), onAttached(nullptr), onDetached(nullptr), onDetach(nullptr) {
}

ijoon::ClientManager::~ClientManager() {
}

int ijoon::ClientManager::Attach(std::string ip, int port)
{
    for(int i = 0 ; i < MAX_CONNECTION ; i++)
    {
        if(clientMap.count(i) == 0)
        {
            ClientSession *sess = new ClientSession(i, ip, port, this);
            clientMap[i] = sess;
            return i;
        }
    }

    return -1;
}

bool ijoon::ClientManager::Detach(int serverIndex)
{
    if(clientMap.count(serverIndex) == 0)
    {
        return false;
    }

    ClientSession *sess = clientMap[serverIndex];

    clientMap.erase(sess->getServerIndex()); // The calling sequence must be followed. (erase() before delete())
    delete sess;

    return true;
}

bool ijoon::ClientManager::Control(int serverIndex, google::protobuf::Message *request)
{
    if(clientMap.count(serverIndex) == 0)
    {
        return false;
    }

    ClientSession *sess = clientMap[serverIndex];

    sess->eventQueue->put(request);
    return true;
}

inline ijoon::MessageHeader makeHeader(char *buf)
{
    ijoon::MessageHeader header;
    google::protobuf::io::ArrayInputStream ais(buf, PACKET_HEADER_SIZE);
    google::protobuf::io::CodedInputStream coded_input(&ais);
    coded_input.ReadVarint32(&header.dataSize); // Decode the HDR and get the size
    coded_input.ReadVarint32(&header.packetType); // Decode the HDR and get the packet type
    coded_input.ReadVarint32(&header.cryptType); // Decode the Crypt
    
    return header;
}

inline void deSerializeResponse(google::protobuf::Message *response, char *buffer, int bufferSize, ijoon::MessageHeader header) {
    // Assign ArrayInputStream with enough memory
    google::protobuf::io::ArrayInputStream ais(buffer, bufferSize);
    google::protobuf::io::CodedInputStream coded_input(&ais);

    // Read an unsigned integer with Varint encoding, truncating to 32 bits.
    coded_input.ReadVarint32(&header.dataSize);
    coded_input.ReadVarint32(&header.packetType);
    coded_input.ReadVarint32(&header.cryptType);

    // After the message's length is read, PushLimit() is used to prevent the CodedInputStream
    // from reading beyond that length.Limits are used when parsing length-delimited
    // embedded messages
    google::protobuf::io::CodedInputStream::Limit msgLimit = coded_input.PushLimit(header.dataSize);

    // De-Serialize
    response->ParseFromCodedStream(&coded_input);

    // Once the embedded message has been parsed, PopLimit() is called to undo the limit
    coded_input.PopLimit(msgLimit);

    //Print the message
//    cout<<"Message: "<<pbm->DebugString();
}

void *ijoon::clientThread(void *arg)
{
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::ClientSession *sess = (ijoon::ClientSession *)thread->getParam();
    ijoon::ClientManager *manager = sess->manager;

    sess->clntSock = new ijoon::JClientSocket(ijoon::tcp);
    while(!sess->mainThread->isInterrupted())
    {
        // CALLBACK::ATTACHING
        if(manager->onAttaching != nullptr) {
            manager->onAttaching(sess->getServerIndex());
        }

        char portStr[20];
        sprintf(portStr, "%d", sess->getServerPort());

        bool connected = sess->clntSock->connect(sess->getServerIp().c_str(), portStr, 3000);

        if(!connected)
        {
            if(errno != EINPROGRESS)
            {
                // CALLBACK::ATTACH_FAILED
                if(manager->onAttachFailed != nullptr) {
                    manager->onAttachFailed(sess->getServerIndex());
                }

                sess->eventQueue->clear();
                continue;
            }
        }

        sess->sendThread = new ijoon::Thread(ijoon::sendRequestThread, "sendThread");
        sess->sendThread->start((void *)sess);
        sess->recvThread = new ijoon::Thread(ijoon::recvResponseThread, "recvThread");
        sess->recvThread->start((void *)sess);

        // CALLBACK::ATTACHED
        if(manager->onAttached != nullptr) {
            manager->onAttached(sess->getServerIndex());
        }

        if(sess->recvThread != nullptr) {
            sess->recvThread->join();
        }
        if(sess->sendThread != nullptr) {
            sess->sendThread->join();
        }

        // CALLBACK::DETACHED
        if(manager->onDetached != nullptr) {
            manager->onDetached(sess->getServerIndex());
        }
        
        sess->eventQueue->clear(); // User may think that his request is ignored when received DISCONNECTED callback.
    }

    // CALLBACK::DETACH
    if(manager->onDetach != nullptr) {
        manager->onDetach(sess->getServerIndex());
    }
    
    return NULL;
}

void *ijoon::recvResponseThread(void *arg)
{
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::ClientSession *session = (ijoon::ClientSession *)thread->getParam();

    while(!thread->isInterrupted())
    {
        int fd_num = session->clntSock->event(5000);

        if(fd_num == -1)
        {
            break; // select error
        }

        if(fd_num == 0)
        {
            ijn_print(DP_INFO, "Response timeout");
            continue;
        }

        if(!session->recvResponse()) {
            break;
        }
    }

    session->recvThread = nullptr;
    
    if(session->sendThread != nullptr) {
        session->sendThread->interrupt();
        session->eventQueue->interrupt();
    }
    
    session->clntSock->close(ijoon::read);
    
    ijn_print(DP_INFO, "recvResponseThread Finished.");

    return NULL;
}

void *ijoon::sendRequestThread(void *arg)
{
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::ClientSession *session = (ijoon::ClientSession *)thread->getParam();

    while(!thread->isInterrupted())
    {
        google::protobuf::Message *request= (google::protobuf::Message *)session->eventQueue->get(50*1000);
        if(request == NULL)
        {
            continue; // timeout
        }

        if(!session->sendRequest(request))
        {
            ijn_print(DP_DEBUG, "send failed");
            break;
        }
    }

    session->sendThread = nullptr;
    
    if(session->recvThread != nullptr) {
        session->recvThread->interrupt();
    }
    
    session->clntSock->close(ijoon::write);
    
    ijn_print(DP_INFO, "sendRequestThread Finished.");

    return NULL;
}

bool ijoon::ClientSession::sendRequest(google::protobuf::Message *request)
{
    int size = request->ByteSize() + PACKET_HEADER_SIZE;
    char buf[size];
    google::protobuf::io::ArrayOutputStream aos(buf,size);
    google::protobuf::io::CodedOutputStream coded_output(&aos);
    coded_output.WriteVarint32(request->ByteSize());
    coded_output.WriteVarint32(BaseMessageRegistry->GetType(request->GetTypeName()));
    coded_output.WriteVarint32((google::protobuf::uint32)0);
    
    request->SerializeToCodedStream(&coded_output);

    if(!this->clntSock->safeSend(buf, 0, size, 0))
    {
        // Error sending data, errno
        return false;
    }
    
    return true;
}

bool ijoon::ClientSession::recvResponse()
{
    char headerBuffer[PACKET_HEADER_SIZE];
    if(!this->clntSock->safeRecv(headerBuffer, 0, PACKET_HEADER_SIZE, MSG_PEEK))
    {
        return false;
    }

    ijoon::MessageHeader header = makeHeader(headerBuffer);

    const int responseSize = header.dataSize + PACKET_HEADER_SIZE;
    char *responseBuffer = new char[responseSize]; // size of the payload and hdr

    // Read the entire buffer including the header
    if(!this->clntSock->safeRecv(responseBuffer, 0, responseSize, 0))
    {
        delete []responseBuffer;
        return false;
    }

    google::protobuf::Message *response = BaseMessageRegistry->Create(header.packetType);

    deSerializeResponse(response, responseBuffer, responseSize, header);
    this->manager->onCallback(this->getServerIndex(), response);
    delete response;
    delete []responseBuffer;
    return true;
}
