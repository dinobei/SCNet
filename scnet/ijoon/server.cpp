#include "server.h"
#include "registry.h"

ijoon::THREAD_RET THREAD_API clientMainThreadFunc(void *arg);
ijoon::THREAD_RET THREAD_API sendThreadFunc(void *arg);
ijoon::THREAD_RET THREAD_API recvThreadFunc(void *arg);

ijoon::THREAD_RET THREAD_API clientMainThreadFunc(void *arg)
{
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::Server *server = (ijoon::Server *)thread->getParam();

    char portStr[20];
    sprintf(portStr, "%d", server->getServerPort());

    while(!thread->isInterrupted()) {
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

        server->recvThread = new ijoon::Thread(recvThreadFunc, "recvThread");
        server->recvThread->start((void *)server);

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

    return NULL;
}

ijoon::THREAD_RET THREAD_API recvThreadFunc(void *arg)
{
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::Server *server = (ijoon::Server *)thread->getParam();

    auto registry = Registry<int, google::protobuf::Message *>().Get();
    int timeoutCount = 0;
    const int timeoutMax = 50;
    while(!thread->isInterrupted())
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
    server->getSession()->getClientSocket()->close(ijoon::read);

    ijn_print(DP_INFO, "recvResponseThread Finished.");

    return THREAD_EXIT;
}

void ijoon::Server::attach() {
    this->mainThread = new ijoon::Thread(clientMainThreadFunc, "mainThread");
    this->mainThread->start(this);
}

void ijoon::Server::detach() {
    if(this->recvThread != nullptr) {
        this->recvThread->interrupt();
    }

    if(this->mainThread != nullptr) {
        this->mainThread->interrupt();
        this->mainThread->join();
    }
}
