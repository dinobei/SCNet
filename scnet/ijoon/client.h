#pragma once
/* std headers */
#include <functional>
#include <assert.h>

/* custom headers */
#include "base_session.h"

namespace ijoon {
    void *clientMainThread(void *arg);
    void *sendThread(void *arg);
    void *recvThread(void *arg);
    
    class Client: public BaseSession {
    public:
        Client(std::string ip, int port, int timeoutMillis): BaseSession(), identifier(-1), serverIPAddress(ip), serverPort(port), timeoutMillis(timeoutMillis) {}
        Client(int identifier, std::string ip, int port, int timeoutMillis): BaseSession(), identifier(identifier), serverIPAddress(ip), serverPort(port), timeoutMillis(timeoutMillis) {}
        ~Client() {}
        
        // Server control method
        void attach();
        void detach();
        void control(google::protobuf::Message *message);
        
        // Getter for server
        std::string getServerIPAddress() { return serverIPAddress; }
        int getServerPort() { return serverPort; }
        int getIdentifier() { return identifier; }
        int getTimeoutMillis() { return timeoutMillis; }
        BlockingQueue<google::protobuf::Message *> *getEventQueue() { return eventQueue; }
        
        // Connection lifecycle
        std::function<void(Client *)> onAttaching;
        std::function<void(Client *)> onAttachFailed;
        std::function<void(Client *)> onAttached;
        std::function<void(Client *)> onDetached;
        std::function<void(Client *)> onDetach;
        
    public:
        Thread *mainThread;
        Thread *sendThread;
        Thread *recvThread;
        
    private:
        int identifier;
        
        BlockingQueue<google::protobuf::Message *> *eventQueue;
        
        std::string serverIPAddress;
        int serverPort;
        
        int timeoutMillis;
    };
}
