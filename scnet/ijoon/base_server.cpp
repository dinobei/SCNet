#include "base_server.h"
#include "utils.h"
#include <map>
#include <fstream>
#include <sys/stat.h>

ijoon::THREAD_RET THREAD_API ServerMainThread(void* param) {
    ijoon::Thread *thread = (ijoon::Thread *)param;
    ijoon::BaseServer *server = (ijoon::BaseServer *)thread->getParam();
    
    server->onServerStarted();
    
    // change to user input
    int host_port= 9190;
    ijoon::JServerSocket servSocket(ijoon::IPv4);
    servSocket.option(ijoon::SOCK_REUSE, 1);
    if(!servSocket.initialize(host_port, 10)) {
        exit(-1);
    }

    while(!thread->isInterrupted()) {
        auto client = servSocket.accept();
        std::cout << "Connected client: " << client->getAddress() << std::endl;

        auto sess = server->getSession(client);
        server->addClient(sess);
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
    auto sess = server->session(thread->getName());
    
    server->onClientServiceStarted(sess);
    
    while(1) {
        int fd_num = sess->event(30 * 1000);
        if(fd_num < 0) {
            server->onClientServiceDisconnected(sess);
            break;
        }
        if(fd_num == 0) {
            server->onClientServiceTimeout(sess);
            continue;
        }
        
        google::protobuf::Message *message = sess->recv();
        if(message == nullptr) {
            server->onClientServiceDisconnected(sess);
            break;
        }

        server->onClientServiceCallback(sess, message);
        delete message;
    }
    
    server->removeClient(thread->getName());
    
    server->onClientServiceStopped(sess);
    
#ifdef _WIN32
    return 0;
#else
    return nullptr;
#endif
    
}

ijoon::BaseServer::BaseServer() {
    initRandomString();
}

ijoon::BaseServer::~BaseServer() {
}

bool ijoon::BaseServer::start() {
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

bool ijoon::BaseServer::addClient(BaseSession *sess) {
    int retryCnt = 10;
    do {
        std::string key = generateRandomString(20);

        if(this->clientMap.count(key) == 0) {
            clientMap[key] = sess;
            
            sess->startThread(ServerServiceThread, key, (void *)this);
            return true;
        }
    } while(--retryCnt);

    return false;
}

bool ijoon::BaseServer::removeClient(std::string key) {
    if(this->clientMap.count(key) == 0)
        return false;

    ijoon::BaseSession *sess = this->clientMap[key];
    delete sess;

    this->clientMap.erase(key);
    return true;
}

int ijoon::BaseServer::clientSize() {
    return this->clientMap.size();
}

ijoon::BaseSession* ijoon::BaseServer::session(std::string key) {
    assert(this->clientMap.count(key) != 0);
    return this->clientMap[key];
}
