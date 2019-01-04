#include "base_server.h"
#include "utils.h"
#include <map>
#include <fstream>
#include <sys/stat.h>
#include <sys/select.h>

#include "registry.h"

ijoon::THREAD_RET THREAD_API ServerMainThread(void* param) {
    ijoon::Thread *thread = (ijoon::Thread *)param;
    ijoon::BaseServer *server = (ijoon::BaseServer *)thread->getParam();
    ijn_print(DP_INFO, "server mode: %s\n", server->isMultiThreadBased()? "multithread based" : "multiplexing based");
    server->onServerStarted();
    
    // change to user input
    ijoon::JServerSocket servSocket(ijoon::IPv4);
    servSocket.option(ijoon::SOCK_REUSE, 1);
    if(!servSocket.initialize(server->getServerPort(), 10)) {
        exit(-1);
    }

    if(server->isMultiThreadBased()) {
        while(!thread->isInterrupted()) {
            auto client = servSocket.accept();
            
            auto sess = server->getSession(client);
            server->addClient(client, sess);
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
                        
                        auto sess = server->getSession(client);
                        server->addClient(client, sess);
                        
                        ijoon::NativeSocket clientSocketId = client->getSocketIdentifier();
                        FD_SET(clientSocketId, &reads);
                        if(fd_max < clientSocketId)
                            fd_max = clientSocketId;
                        
                        server->onClientServiceStarted(sess);
                    }
                    else
                    {
                        auto sess = server->session(i);
                        ijoon::MessageHeader messageHeader;
                        if(!sess->recvHeader(messageHeader)) {
                            FD_CLR(i, &reads);
                            
                            server->removeClient(i);
                            server->onClientServiceDisconnected(sess);
                            continue;
                        }
                        
                        google::protobuf::Message *message = sess->recvBody(messageHeader);
                        BaseMessageRegistry->GetCallbackWrapper(messageHeader.packetType)->callback(sess, message);
                        delete message;
                    }
                }
            }
        }

    }
    
    server->onServerStopped();
    
#ifdef _WIN32
    return 0;
#else
    return nullptr;
#endif
}

ijoon::THREAD_RET THREAD_API ServerServiceThread(void* param) {
    ijoon::Thread *thread = (ijoon::Thread *)param;
    ijoon::BaseServer *server = (ijoon::BaseServer *)thread->getParam();
    ijoon::NativeSocket nativeSocket = atoi(thread->getName().c_str());
    auto sess = server->session(nativeSocket);
    
    server->onClientServiceStarted(sess);
    
    while(1) {
        int fd_num = sess->event(server->getRecvTimeoutMs());
        if(fd_num < 0) {
            server->onClientServiceDisconnected(sess);
            break;
        }
        if(fd_num == 0) {
            server->onClientServiceTimeout(sess);
            continue;
        }
        
        ijoon::MessageHeader messageHeader;
        if(sess->recvHeader(messageHeader)) {
            server->onClientServiceDisconnected(sess);
            break;
        }
        
        google::protobuf::Message *message = sess->recvBody(messageHeader);
        BaseMessageRegistry->GetCallbackWrapper(messageHeader.packetType)->callback(sess, message);
        delete message;
    }
    
    server->removeClient(nativeSocket);
    
    server->onClientServiceStopped(sess);
    
#ifdef _WIN32
    return 0;
#else
    return nullptr;
#endif
    
}

ijoon::BaseServer::BaseServer(int port, int recvTimeoutMs, bool useMultiThread) {
    initRandomString();
    this->port = port;
    this->recvTimeoutMs = recvTimeoutMs;
    this->thread = nullptr;
    this->useMultiThread = useMultiThread;
}

ijoon::BaseServer::~BaseServer() {
}

int ijoon::BaseServer::getServerPort() {
    return this->port;
}

int ijoon::BaseServer::getRecvTimeoutMs() {
    return this->recvTimeoutMs;
}

bool ijoon::BaseServer::start() {
    if(this->thread != nullptr) {
        return false;
    }
    
    this->thread = new ijoon::Thread(ServerMainThread, "Main Thread");
    thread->start(this);
    return true;
}

bool ijoon::BaseServer::stop() {
    if(this->thread != nullptr) {
        this->thread->interrupt();
        this->thread = nullptr;
        return true;
    }
    return false;
}

bool ijoon::BaseServer::addClient(std::shared_ptr<JClientSocket> clientSocket, BaseSession *sess) {
    int retryCnt = 10;
    do {
        if(this->clientMap.count(clientSocket->getSocketIdentifier()) == 0) {
            clientMap[clientSocket->getSocketIdentifier()] = sess;
            
            if(isMultiThreadBased()) {
                sess->startThread(ServerServiceThread, std::to_string(clientSocket->getSocketIdentifier()), this);
            }
            
            return true;
        }
    } while(--retryCnt);

    return false;
}

bool ijoon::BaseServer::removeClient(std::shared_ptr<JClientSocket> clientSocket) {
    return removeClient(clientSocket->getSocketIdentifier());
}

bool ijoon::BaseServer::removeClient(NativeSocket nativeSocket) {
    if(this->clientMap.count(nativeSocket) == 0)
        return false;
    
    ijoon::BaseSession *sess = this->clientMap[nativeSocket];
    delete sess;
    
    this->clientMap.erase(nativeSocket);
    return true;
}

int ijoon::BaseServer::clientSize() {
    return this->clientMap.size();
}

ijoon::BaseSession* ijoon::BaseServer::session(NativeSocket nativeSocket) {
    assert(this->clientMap.count(nativeSocket) != 0);
    return this->clientMap[nativeSocket];
}
