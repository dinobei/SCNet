#pragma once
/* std headers */
#include <functional>
#include <assert.h>

/* custom headers */
#include "session.h"

namespace ijoon {
    void *clientMainThread(void *arg);
    void *sendThread(void *arg);
    void *recvThread(void *arg);
    
    class Server: public Session {
    public:
        Server(std::string ip, int port, int timeoutMillis): Session(), identifier(-1), serverIPAddress(ip), serverPort(port), timeoutMillis(timeoutMillis) {}
        Server(int identifier, std::string ip, int port, int timeoutMillis): Session(), identifier(identifier), serverIPAddress(ip), serverPort(port), timeoutMillis(timeoutMillis) {}
        ~Server() {}
        
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
        std::function<void(Server *)> onAttaching;
        std::function<void(Server *)> onAttachFailed;
        std::function<void(Server *)> onAttached;
        std::function<void(Server *)> onDetached;
        std::function<void(Server *)> onDetach;
        
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
