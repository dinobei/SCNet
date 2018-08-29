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
    google::protobuf::io::ArrayInputStream ais(buf, MAX_PACKET_HEADER_SIZE);
    google::protobuf::io::CodedInputStream coded_input(&ais);
    coded_input.ReadVarint32(&header.dataSize); // Decode the HDR and get the size
    coded_input.ReadVarint32(&header.packetType); // Decode the HDR and get the packet type
    coded_input.ReadVarint32(&header.messageType); // Decode the message type
    coded_input.ReadVarint32(&header.cryptType); // Decode the Crypt
    
    return header;
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

        google::protobuf::Message *response = session->recvResponse();
        if(!response) {
            break;
        }
        session->manager->onCallback(session->getServerIndex(), response);
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

bool ijoon::ClientSession::sendRequest(google::protobuf::Message *message)
{
    int size = MAGIC_PACKET_LENGTH + MAX_PACKET_HEADER_SIZE+ message->ByteSize();
    char *buf = new char[size];
    google::protobuf::io::ArrayOutputStream aos(buf,size);
    google::protobuf::io::CodedOutputStream coded_output(&aos);
    coded_output.WriteRaw(MAGIC_PACKET, MAGIC_PACKET_LENGTH);
    coded_output.WriteVarint32(message->ByteSize());
    coded_output.WriteVarint32(BaseMessageRegistry->GetType(message->GetTypeName()));
    coded_output.WriteVarint32(0); // message type
    coded_output.WriteVarint32(0);
    
    message->SerializeToCodedStream(&coded_output);
    
    if(!this->clntSock->safeSend(buf, 0, coded_output.ByteCount(), 0)) {
        delete[] buf;
        return false;
    }
    
    delete []buf;
    return true;
}

google::protobuf::Message *ijoon::ClientSession::recvResponse()
{
    char magicPacket[2] = {0,};
    // read magic packet
    if(!this->clntSock->safeRecv(magicPacket, 0, MAGIC_PACKET_LENGTH, 0)) {
        return nullptr;
    }
    
    if(magicPacket[0] != MAGIC_PACKET[0] || magicPacket[1] != MAGIC_PACKET[1]) {
        return nullptr;
    }
    
    char headerBuffer[MAX_PACKET_HEADER_SIZE] = {0,};
    
    // read header
    int readingHeaderSize = 0;
    int receivedHeaderComponent = 0;
    
    while(true) {
        if(!this->clntSock->safeRecv(headerBuffer, readingHeaderSize++, 1, 0)) {
            return nullptr;
        }
        
        if((headerBuffer[readingHeaderSize-1]&0xFF) > 127) {
            continue;
        }
        
        if(++receivedHeaderComponent == HEADER_ELEMENTS) {
            break;
        }
    }

    ijoon::MessageHeader header = makeHeader(headerBuffer);
    
    google::protobuf::Message *response = BaseMessageRegistry->Create(header.packetType);
    if(response == nullptr) {
        ijn_print(DP_INFO, "Unknown packet type(=%d)", header.packetType);
        return nullptr;
    }
    
    // read contents
    const int responseSize = header.dataSize;
    if(responseSize > 0) {
        char *responseBuffer = new char[responseSize];
        
        // Read the entire buffer including the header
        if(!this->clntSock->safeRecv(responseBuffer, 0, responseSize, 0)) {
            delete []responseBuffer;
            return nullptr;
        }
        
        response->ParseFromArray(responseBuffer, header.dataSize);
        delete []responseBuffer;
    }
    
    return response;
}
