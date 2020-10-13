#include "client.h"
#include "registry.h"

void recvThreadFunc(scnet::Client *server);

void clientMainThreadFunc(scnet::Client *server)
{
    char portStr[20];
    sprintf(portStr, "%d", server->getServerPort());

    while(server->mainThreadCondition) {
        // CALLBACK::ATTACHING
        if(server->onAttaching != nullptr) {
            server->onAttaching(server->getSession());
        }

        bool connected = server->getSession()->getClientSocket()->connect(server->getServerIPAddress(),
                                                                          portStr,
                                                                          server->getTimeoutMillis());
        if(!connected)
        {
            if(errno != EINPROGRESS)
            {
                // CALLBACK::ATTACH_FAILED
                if(server->onAttachFailed != nullptr) {
                    server->onAttachFailed(server->getSession());
                }
                continue;
            }
        }

        server->recvThreadCondition.store(true);
        server->recvThread = new std::thread(recvThreadFunc, server);

        // CALLBACK::ATTACHED
        if(server->onAttached != nullptr) {
            server->onAttached(server->getSession());
        }

        if(server->recvThread != nullptr) {
            server->recvThread->join();
        }

        // CALLBACK::DETACHED
        if(server->onDetached != nullptr) {
            server->onDetached(server->getSession());
        }
    }

    // CALLBACK::DETACH
    if(server->onDetach != nullptr) {
        server->onDetach(server->getSession());
    }
}

void recvThreadFunc(scnet::Client *server)
{
    auto registry = Registry<int, google::protobuf::Message *>().Get();
    int timeoutCount = 0;
    const int timeoutMax = 50;
    while(server->recvThreadCondition)
    {
        int fd_num = server->getSession()->getClientSocket()->event(100);

        if(fd_num == -1)
        {
            break; // select error
        }

        if(fd_num == 0)
        {
            if(timeoutMax < timeoutCount++ &&
               server->onTimeout != nullptr) {
                server->onTimeout(server->getSession());
                timeoutCount = 0;
            }
            continue;
        }
        
        timeoutCount = 0;

        if(!server->getSession()->recv()) {
            break;
        }
    }

    server->recvThread = nullptr;
    server->getSession()->getClientSocket()->close(cppsocket::read);
}

void scnet::Client::attach() {
    if(this->mainThread != nullptr) return;
    this->mainThreadCondition.store(true);
    this->mainThread = new std::thread(clientMainThreadFunc, this);
}

void scnet::Client::detach() {
    if(this->recvThread != nullptr) {
        this->recvThreadCondition.store(false);
    }

    if(this->mainThread != nullptr) {
        this->mainThreadCondition.store(false);
        if(this->mainThread->joinable()) {
            this->mainThread->join();
            this->mainThread = nullptr;
        }
    }
}
