#include "client_manager.h"
#include <map>
#include <fstream>
#include <sys/stat.h>

#ifndef __IJN_WINDOWS__
#include <sys/select.h>
#endif

#include "registry.h"

fd_set reads;
void ijoon::ClientManager::ServerMainThread() {
    std::cout << "server mode: " << (this->isMultiThreadBased()? "multithread based" : "multiplexing based") << std::endl;
    if(this->onServerStarted != nullptr)
        this->onServerStarted();
    
    // change to user input
    this->servSocket = new cppsocket::tcp_socket(this->getServerPort());

    if(this->isMultiThreadBased()) {
        while(this->condition) {
            auto client = servSocket->accept();
            if(client == nullptr) {
                __msleep(1);
                continue;
            }
            
            if(this->getClientSession == nullptr) {
                throw std::exception();
            }
            auto sess = this->getClientSession(client);
            
            sess->updatePing();
            this->addClient(client, sess);
        }
    }
    else {
        fd_set cpy_reads;
        struct timeval timeout;
        int fd_max, fd_num;
        FD_ZERO(&reads);
        FD_SET(servSocket->get_socket_identifier(), &reads);
        
        fd_max = servSocket->get_socket_identifier();
        auto registry = Registry<int, google::protobuf::Message* >().Get();
        auto timeoutMs = std::chrono::milliseconds(this->getRecvTimeoutMs());
        auto lastCheckTime = std::chrono::system_clock::now();
        while(this->condition) {
            cpy_reads = reads;
            timeout.tv_sec = this->getRecvTimeoutMs() / 1000;
            timeout.tv_usec = (this->getRecvTimeoutMs() % 1000) * 1000;
            if( (fd_num = select(fd_max + 1, &cpy_reads, 0, 0, &timeout)) == -1)
                break;
            
            auto currentTime = std::chrono::system_clock::now();
            if(lastCheckTime + timeoutMs <= currentTime) {
                lastCheckTime = currentTime;
                
                auto clientMap = this->getClientMap();
                for(auto iter : clientMap) {
                    if(iter.second->getPing() + timeoutMs < currentTime && this->onClientTimeout != nullptr) {
                        this->onClientTimeout(iter.second);
                    }
                }
            }
            
            if(fd_num == 0) continue;
            
            for(int i = 0 ; i < fd_max+1 ; i++)
            {
                if(FD_ISSET(i, &cpy_reads))
                {
                    if(i == servSocket->get_socket_identifier())
                    {
                        auto client = servSocket->accept();
                        
                        if(this->getClientSession == nullptr) {
                            throw std::exception();
                        }
                        auto sess = this->getClientSession(client);
                        sess->updatePing();
                        this->addClient(client, sess);
                        
                        int clientSocketId = client->get_socket_identifier();
                        FD_SET(clientSocketId, &reads);
                        if(fd_max < clientSocketId)
                            fd_max = clientSocketId;
                        
                        if(this->onClientConnected != nullptr)
                            this->onClientConnected(sess);
                    }
                    else
                    {
                        auto sess = this->session(i);
                        if(sess == nullptr) continue;
                        if(!sess->recv()) {
                            if(this->onClientDisconnected != nullptr)
                                this->onClientDisconnected(sess);
                            this->removeClient(i);
                            continue;
                        }
                        
                        sess->updatePing();
                    }
                }
            }
        }
    }
    
    if(this->onServerStopped != nullptr)
        this->onServerStopped();
}

void ijoon::ClientManager::ServerServiceThread(int socketId) {
    auto sess = this->session(socketId);
    if(sess == nullptr) return;
    
    if(this->onClientConnected != nullptr)
        this->onClientConnected(sess);
    
    auto registry = Registry<int, google::protobuf::Message *>().Get();
    while(1) {
        int fd_num = sess->getClientSocket()->event(this->getRecvTimeoutMs());
        if(fd_num < 0) {
            break;
        }
        if(fd_num == 0) {
            if(this->onClientTimeout != nullptr)
                this->onClientTimeout(sess);
            continue;
        }
        
        if(!sess->recv()) {
            break;
        }
        
        sess->updatePing();
        
    }
    
    __msleep(1000);
    this->removeClient(socketId);
    
    if(this->onClientDisconnected != nullptr)
        this->onClientDisconnected(sess);
}

ijoon::ClientManager::ClientManager(ushort port, int recvTimeoutMs, bool useMultiThread) {
    this->port = port;
    this->recvTimeoutMs = recvTimeoutMs;
    this->thread = nullptr;
    this->useMultiThread = useMultiThread;
}

ijoon::ClientManager::~ClientManager() {
}

ushort ijoon::ClientManager::getServerPort() {
    return this->port;
}

int ijoon::ClientManager::getRecvTimeoutMs() {
    return this->recvTimeoutMs;
}

bool ijoon::ClientManager::start() {
    if(this->thread != nullptr) {
        return false;
    }
    
    this->condition.store(true);
    this->thread = new std::thread(std::bind(&ClientManager::ServerMainThread, this));
    return true;
}

bool ijoon::ClientManager::stop() {
    if(this->thread != nullptr) {
        this->condition.store(false);
        this->servSocket->close();
        if(this->thread->joinable()) {
            this->thread->join();
            this->thread = nullptr;
        }
        return true;
    }
    return false;
}

bool ijoon::ClientManager::addClient(std::shared_ptr<cppsocket::tcp_socket> clientSocket, std::shared_ptr<Session> sess) {
    if(this->clientMap.count(clientSocket->get_socket_identifier()) == 0) {
        clientMap[clientSocket->get_socket_identifier()] = sess;
        
        if(isMultiThreadBased()) {
            std::thread *thread = new std::thread(std::bind(&ClientManager::ServerServiceThread, this, clientSocket->get_socket_identifier())); // TODO: manage ServerServiceThread
        }
        
        return true;
    }

    return false;
}

bool ijoon::ClientManager::removeClient(std::shared_ptr<ijoon::Session> session) {
    if(session == nullptr) return false;
    
    auto socketId = session->getClientSocket()->get_socket_identifier();
    
    if(this->clientMap.count(socketId) == 0)
        return false;
    
    FD_CLR(socketId, &reads);
    
    if(!useMultiThread && onClientDisconnected != nullptr) {
        onClientDisconnected(session);
    }
    
    this->clientMap.erase(socketId);
    close(socketId);
    return true;
}

bool ijoon::ClientManager::removeClient(std::shared_ptr<cppsocket::tcp_socket> clientSocket) {
    return removeClient(clientSocket->get_socket_identifier());
}

bool ijoon::ClientManager::removeClient(int socketId) {
    if(this->clientMap.count(socketId) == 0)
        return false;
    
    FD_CLR(socketId, &reads);
    
    this->clientMap.erase(socketId);
    close(socketId);
    return true;
}

int ijoon::ClientManager::clientSize() {
    return this->clientMap.size();
}

std::shared_ptr<ijoon::Session> ijoon::ClientManager::session(int socketId) {
    if(this->clientMap.count(socketId) == 0) return nullptr;
    return this->clientMap[socketId];
}

std::map<int, std::shared_ptr<ijoon::Session>> &ijoon::ClientManager::getClientMap() {
    return clientMap;
}
