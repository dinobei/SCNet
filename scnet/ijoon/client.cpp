#include "client.h"
#include "registry.h"

void *ijoon::clientMainThread(void *arg)
{
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::Client *client = (ijoon::Client *)thread->getParam();

    char portStr[20];
    sprintf(portStr, "%d", client->getServerPort());

    while(!thread->isInterrupted()) {
        // CALLBACK::ATTACHING
        if(client->onAttaching != nullptr) {
            client->onAttaching(client);
        }

        bool connected = client->getClientSocket()->connect(client->getServerIPAddress().c_str(), portStr, client->getTimeoutMillis());
        if(!connected)
        {
            if(errno != EINPROGRESS)
            {
                // CALLBACK::ATTACH_FAILED
                if(client->onAttachFailed != nullptr) {
                    client->onAttachFailed(client);
                }

                client->getEventQueue()->clear();
                continue;
            }
        }

        client->sendThread = new ijoon::Thread(ijoon::sendThread, "sendThread");
        client->sendThread->start((void *)client);
        client->recvThread = new ijoon::Thread(ijoon::recvThread, "recvThread");
        client->recvThread->start((void *)client);

        // CALLBACK::ATTACHED
        if(client->onAttached != nullptr) {
            client->onAttached(client);
        }

        if(client->recvThread != nullptr) {
            client->recvThread->join();
        }
        if(client->sendThread != nullptr) {
            client->sendThread->join();
        }

        // CALLBACK::DETACHED
        if(client->onDetached != nullptr) {
            client->onDetached(client);
        }

        client->getEventQueue()->clear(); // User may think that his request is ignored when received DISCONNECTED callback.
    }

    // CALLBACK::DETACH
    if(client->onDetach != nullptr) {
        client->onDetach(client);
    }

    return NULL;
}

void *ijoon::recvThread(void *arg)
{
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::Client *client = (ijoon::Client *)thread->getParam();

    while(!thread->isInterrupted())
    {
        int fd_num = client->event(5000);

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
        if(!client->recvHeader(messageHeader)) {
            break;
        }

        google::protobuf::Message *message = client->recvBody(messageHeader);
        BaseMessageRegistry->GetCallbackWrapper(messageHeader.packetType)->callback(client, message);
        delete message;
    }

    client->recvThread = nullptr;

    if(client->sendThread != nullptr) {
        client->sendThread->interrupt();
        client->getEventQueue()->interrupt();
    }

    client->getClientSocket()->close(ijoon::read);

    ijn_print(DP_INFO, "recvResponseThread Finished.");

    return NULL;
}

void *ijoon::sendThread(void *arg)
{
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::Client *client = (ijoon::Client *)thread->getParam();

    while(!thread->isInterrupted())
    {
        google::protobuf::Message *request= (google::protobuf::Message *)client->getEventQueue()->get(50*1000);
        if(request == NULL)
        {
            continue; // timeout
        }

        if(!client->send(request)) {
            ijn_print(DP_DEBUG, "send failed");
            break;
        }
    }

    client->sendThread = nullptr;

    if(client->recvThread != nullptr) {
        client->recvThread->interrupt();
    }

    client->getClientSocket()->close(ijoon::write);

    ijn_print(DP_INFO, "sendRequestThread Finished.");

    return NULL;
}

void ijoon::Client::attach() {
    this->eventQueue = new ijoon::BlockingQueue<google::protobuf::Message *>();

    this->mainThread = new ijoon::Thread(clientMainThread, "mainThread");
    this->mainThread->start(this);
}

void ijoon::Client::detach() {
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

void ijoon::Client::control(google::protobuf::Message *message) {
    this->eventQueue->put(message);
}
