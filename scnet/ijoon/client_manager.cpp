#include "client_manager.h"
#include "utils.h"
#include <map>
#include <fstream>
#include <sys/stat.h>
#include <sys/select.h>

#include "registry.h"

ijoon::THREAD_RET THREAD_API ServerMainThread(void* param) {
    ijoon::Thread *thread = (ijoon::Thread *)param;
    ijoon::ClientManager *server = (ijoon::ClientManager *)thread->getParam();
    ijn_print(DP_INFO, "server mode: %s\n", server->isMultiThreadBased()? "multithread based" : "multiplexing based");
    if(server->onServerStarted != nullptr)
        server->onServerStarted();
    
    // change to user input
    ijoon::ServerTCPSocket servSocket(server->getServerPort());
    servSocket.option(ijoon::SOCK_REUSE, 1);

    if(server->isMultiThreadBased()) {
        while(!thread->isInterrupted()) {
            auto client = servSocket.accept();
            
            auto sess = new ijoon::Session(client);
            server->addClient(client, sess);
            if(server->onClientConnected != nullptr)
                server->onClientConnected(sess);
        }
    }
    else {
        fd_set reads, cpy_reads;
        struct timeval timeout;
        int fd_max, fd_num;
        FD_ZERO(&reads);
        FD_SET(servSocket.getSocketIdentifier(), &reads);
        
        fd_max = servSocket.getSocketIdentifier();
        while(!thread->isInterrupted()) {
            cpy_reads = reads;
            timeout.tv_sec = server->getRecvTimeoutMs() / 1000;
            timeout.tv_usec = (server->getRecvTimeoutMs() % 1000) * 1000;
            if( (fd_num = select(fd_max + 1, &cpy_reads, 0, 0, &timeout)) == -1)
                break;
            if(fd_num == 0)
            {
                if(server->onClientServiceTimeout != nullptr)
                    server->onClientServiceTimeout(nullptr);
                continue;
            }
            
            for(int i = 0 ; i < fd_max+1 ; i++)
            {
                if(FD_ISSET(i, &cpy_reads))
                {
                    if(i == servSocket.getSocketIdentifier())
                    {
                        auto client = servSocket.accept();
                        
                        auto sess = new ijoon::Session(client);
                        server->addClient(client, sess);
                        if(server->onClientConnected != nullptr)
                            server->onClientConnected(sess);
                        
                        ijoon::SocketIdentifier clientSocketId = client->getSocketIdentifier();
                        FD_SET(clientSocketId, &reads);
                        if(fd_max < clientSocketId)
                            fd_max = clientSocketId;
                        
                        if(server->onClientServiceStarted != nullptr)
                            server->onClientServiceStarted(sess);
                    }
                    else
                    {
                        auto sess = server->session(i);
                        ijoon::MessageHeader messageHeader;
                        if(!sess->recvHeader(messageHeader)) {
                            FD_CLR(i, &reads);
                            
                            if(server->onClientServiceDisconnected != nullptr)
                                server->onClientServiceDisconnected(sess);
                            server->removeClient(i);
                            continue;
                        }
                        


                        switch (messageHeader.messageType) {
                            case ijoon::MESSAGE_TYPE::PROTOBUF:
                            {
                                google::protobuf::Message *message = sess->recvProtobufBody(messageHeader);
                                if(message == nullptr) continue;
                                AbstractCallbackWrapper *callbackWrapper = BaseMessageRegistry->GetCallbackWrapper(messageHeader.messageType, messageHeader.packetType);
                                if(callbackWrapper == nullptr) {
                                    delete message;
                                    break;
                                }
                                callbackWrapper->callback(sess, message);
                                delete message;
                                break;
                            }
                            case ijoon::MESSAGE_TYPE::RAWBYTE:
                            {
                                char *message = sess->recvRawBody(messageHeader);
                                AbstractCallbackWrapper *callbackWrapper = BaseMessageRegistry->GetCallbackWrapper(messageHeader.messageType, messageHeader.packetType);
                                if(callbackWrapper == nullptr) {
                                    delete message;
                                    break;
                                }
                                
                                callbackWrapper->callback(sess, message, messageHeader.dataSize);
                                delete message;
                                break;
                            }
                            default:
                                break;
                        }
                    }
                }
            }
        }

    }
    
    if(server->onServerStopped != nullptr)
        server->onServerStopped();
    
#ifdef _WIN32
    return 0;
#else
    return nullptr;
#endif
}

ijoon::THREAD_RET THREAD_API ServerServiceThread(void* param) {
    ijoon::Thread *thread = (ijoon::Thread *)param;
    ijoon::ClientManager *server = (ijoon::ClientManager *)thread->getParam();
    ijoon::SocketIdentifier socketId = atoi(thread->getName().c_str());
    auto sess = server->session(socketId);
    
    if(server->onClientServiceStarted != nullptr)
        server->onClientServiceStarted(sess);
    
    while(1) {
        int fd_num = sess->event(server->getRecvTimeoutMs());
        if(fd_num < 0) {
            if(server->onClientServiceDisconnected != nullptr)
                server->onClientServiceDisconnected(sess);
            break;
        }
        if(fd_num == 0) {
            if(server->onClientServiceTimeout != nullptr)
                server->onClientServiceTimeout(sess);
            continue;
        }
        
        ijoon::MessageHeader messageHeader;
        if(sess->recvHeader(messageHeader)) {
            if(server->onClientServiceDisconnected != nullptr)
                server->onClientServiceDisconnected(sess);
            break;
        }
        
        switch (messageHeader.messageType) {
            case ijoon::MESSAGE_TYPE::PROTOBUF:
            {
                google::protobuf::Message *message = sess->recvProtobufBody(messageHeader);
                if(message == nullptr) break;
                AbstractCallbackWrapper *callbackWrapper = BaseMessageRegistry->GetCallbackWrapper(messageHeader.messageType, messageHeader.packetType);
                if(callbackWrapper == nullptr) {
                    delete message;
                    break;
                }
                callbackWrapper->callback(sess, message);
                delete message;
                break;
            }
            case ijoon::MESSAGE_TYPE::RAWBYTE:
            {
                char *message = sess->recvRawBody(messageHeader);
                AbstractCallbackWrapper *callbackWrapper = BaseMessageRegistry->GetCallbackWrapper(messageHeader.messageType, messageHeader.packetType);
                if(callbackWrapper == nullptr) {
                    delete message;
                    break;
                }
                
                callbackWrapper->callback(sess, message, messageHeader.dataSize);
                delete message;
                break;
            }
            default:
                break;
        }
    }
    
    server->removeClient(socketId);
    
    if(server->onClientServiceStopped != nullptr)
        server->onClientServiceStopped(sess);
    
#ifdef _WIN32
    return 0;
#else
    return nullptr;
#endif
    
}

ijoon::ClientManager::ClientManager(int port, int recvTimeoutMs, bool useMultiThread) {
    initRandomString();
    this->port = port;
    this->recvTimeoutMs = recvTimeoutMs;
    this->thread = nullptr;
    this->useMultiThread = useMultiThread;
}

ijoon::ClientManager::~ClientManager() {
}

int ijoon::ClientManager::getServerPort() {
    return this->port;
}

int ijoon::ClientManager::getRecvTimeoutMs() {
    return this->recvTimeoutMs;
}

bool ijoon::ClientManager::start() {
    if(this->thread != nullptr) {
        return false;
    }
    
    this->thread = new ijoon::Thread(ServerMainThread, "Main Thread");
    thread->start(this);
    return true;
}

bool ijoon::ClientManager::stop() {
    if(this->thread != nullptr) {
        this->thread->interrupt();
        this->thread = nullptr;
        return true;
    }
    return false;
}

bool ijoon::ClientManager::addClient(std::shared_ptr<ClientTCPSocket> clientSocket, Session *sess) {
    int retryCnt = 10;
    do {
        if(this->clientMap.count(clientSocket->getSocketIdentifier()) == 0) {
            clientMap[clientSocket->getSocketIdentifier()] = sess;
            
            if(isMultiThreadBased()) {
                ijoon::Thread *thread = new ijoon::Thread(ServerServiceThread, std::to_string(clientSocket->getSocketIdentifier()));
                thread->start(this);
            }
            
            return true;
        }
    } while(--retryCnt);

    return false;
}

bool ijoon::ClientManager::removeClient(std::shared_ptr<ClientTCPSocket> clientSocket) {
    return removeClient(clientSocket->getSocketIdentifier());
}

bool ijoon::ClientManager::removeClient(SocketIdentifier socketId) {
    if(this->clientMap.count(socketId) == 0)
        return false;
    
    ijoon::Session *sess = this->clientMap[socketId];
    delete sess;
    
    this->clientMap.erase(socketId);
    return true;
}

int ijoon::ClientManager::clientSize() {
    return this->clientMap.size();
}

ijoon::Session* ijoon::ClientManager::session(SocketIdentifier socketId) {
    assert(this->clientMap.count(socketId) != 0);
    return this->clientMap[socketId];
}
