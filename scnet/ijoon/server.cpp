#include "server.h"
#include "registry.h"

void *ijoon::clientMainThread(void *arg)
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

        bool connected = server->getSession()->getClientSocket()->connect(server->getServerIPAddress().c_str(), portStr, server->getTimeoutMillis());
        if(!connected)
        {
            if(errno != EINPROGRESS)
            {
                // CALLBACK::ATTACH_FAILED
                if(server->onAttachFailed != nullptr) {
                    server->onAttachFailed(server->getSession());
                }

                server->getEventQueue()->clear();
                continue;
            }
        }

        server->sendThread = new ijoon::Thread(ijoon::sendThread, "sendThread");
        server->sendThread->start((void *)server);
        server->recvThread = new ijoon::Thread(ijoon::recvThread, "recvThread");
        server->recvThread->start((void *)server);

        // CALLBACK::ATTACHED
        if(server->onAttached != nullptr) {
            server->onAttached(server->getSession());
        }

        if(server->recvThread != nullptr) {
            server->recvThread->join();
        }
        if(server->sendThread != nullptr) {
            server->sendThread->join();
        }

        // CALLBACK::DETACHED
        if(server->onDetached != nullptr) {
            server->onDetached(server->getSession());
        }

        server->getEventQueue()->clear(); // User may think that his request is ignored when received DISCONNECTED callback.
    }

    // CALLBACK::DETACH
    if(server->onDetach != nullptr) {
        server->onDetach(server->getSession());
    }

    return NULL;
}

void *ijoon::recvThread(void *arg)
{
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::Server *server = (ijoon::Server *)thread->getParam();

    while(!thread->isInterrupted())
    {
        int fd_num = server->getSession()->getClientSocket()->event(5000);

        if(fd_num == -1)
        {
            break; // select error
        }

        if(fd_num == 0)
        {
            ijn_print(DP_INFO, "Response timeout");
            continue;
        }

        ijoon::MessageHeader messageHeader;
        if(!server->getSession()->recvHeader(messageHeader)) {
            break;
        }

        switch (messageHeader.messageType) {
            case ijoon::MESSAGE_TYPE::PROTOBUF:
            {
                google::protobuf::Message *message = server->getSession()->recvProtobufBody(messageHeader);
                if(message == nullptr) break;
                AbstractCallbackWrapper *callbackWrapper = BaseMessageRegistry->GetCallbackWrapper(messageHeader.messageType, messageHeader.packetType);
                if(callbackWrapper == nullptr) {
                    delete message;
                    break;
                }
                callbackWrapper->callback(server->getSession(), message);
                delete message;
                break;
            }
            case ijoon::MESSAGE_TYPE::RAWBYTE:
            {
                char *message = server->getSession()->recvRawBody(messageHeader);
                AbstractCallbackWrapper *callbackWrapper = BaseMessageRegistry->GetCallbackWrapper(messageHeader.messageType, messageHeader.packetType);
                if(callbackWrapper == nullptr) {
                    delete message;
                    break;
                }
                
                callbackWrapper->callback(server->getSession(), message, messageHeader.dataSize);
                delete message;
                break;
            }
            default:
                break;
        }
    }

    server->recvThread = nullptr;

    if(server->sendThread != nullptr) {
        server->sendThread->interrupt();
        server->getEventQueue()->interrupt();
    }

    server->getSession()->getClientSocket()->close(ijoon::read);

    ijn_print(DP_INFO, "recvResponseThread Finished.");

    return NULL;
}

void *ijoon::sendThread(void *arg)
{
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::Server *server = (ijoon::Server *)thread->getParam();

    while(!thread->isInterrupted())
    {
        MessageWrapper *messageWrapper = server->getEventQueue()->get(50*1000);
        if(messageWrapper == NULL)
        {
            continue; // timeout
        }
        
        if(messageWrapper->message == nullptr) {
            delete messageWrapper;
            continue;
        }
        
        switch (messageWrapper->messageType) {
            case ijoon::MESSAGE_TYPE::PROTOBUF:
            {
                if(!server->getSession()->send((google::protobuf::Message *)messageWrapper->message)) {
                    ijn_print(DP_DEBUG, "send failed");
                    break;
                }
                break;
            }
            case ijoon::MESSAGE_TYPE::RAWBYTE:
            {
                if(!server->getSession()->send(messageWrapper->packetType, (char *)messageWrapper->message, messageWrapper->length)) {
                    ijn_print(DP_DEBUG, "send failed");
                    break;
                }
                break;
            }
            default:
                break;
        }
        
        delete messageWrapper;
    }

    server->sendThread = nullptr;

    if(server->recvThread != nullptr) {
        server->recvThread->interrupt();
    }

    server->getSession()->getClientSocket()->close(ijoon::write);

    ijn_print(DP_INFO, "sendRequestThread Finished.");

    return NULL;
}

void ijoon::Server::attach() {
    this->eventQueue = new ijoon::BlockingQueue<MessageWrapper *>();

    this->mainThread = new ijoon::Thread(clientMainThread, "mainThread");
    this->mainThread->start(this);
}

void ijoon::Server::detach() {
    if(this->recvThread != nullptr) {
        this->recvThread->interrupt();
    }

    if(this->sendThread != nullptr) {
        this->sendThread->interrupt();
    }

    if(this->mainThread != nullptr) {
        this->mainThread->interrupt();
        this->mainThread->join();
    }

    delete this->eventQueue;
    this->eventQueue = NULL;
}

void ijoon::Server::control(int packetType, char *message, unsigned int length) {
    MessageWrapper *messageWrapper = new MessageWrapper(packetType, message, length);
    this->eventQueue->put(messageWrapper);
}

void ijoon::Server::control(google::protobuf::Message *message) {
    MessageWrapper *messageWrapper = new MessageWrapper(message);
    this->eventQueue->put(messageWrapper);
}
