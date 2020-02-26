#pragma once
/* std headers */
#include <functional>
#include <assert.h>

/* custom headers */
#include "session.h"

namespace ijoon {
    class MessageWrapper {
    public:
        MessageWrapper(google::protobuf::Message *message): messageType(MESSAGE_TYPE::PROTOBUF), message(message) {
            length = static_cast<google::protobuf::Message *>(message)->ByteSize();
        }

        MessageWrapper(int packetType, char *message, unsigned int length): messageType(MESSAGE_TYPE::RAWBYTE), packetType(packetType), message(message), length(length) {}
        
        ijoon::MESSAGE_TYPE messageType;
        int packetType;
        void *message;
        unsigned int length;
    };
    
    class Server {
    public:
        Server(std::string ip, int port, int timeoutMillis): sess(new Session()), identifier(-1), serverIPAddress(ip), serverPort(port), timeoutMillis(timeoutMillis) {}
        Server(int identifier, std::string ip, int port, int timeoutMillis): sess(new Session()), identifier(identifier), serverIPAddress(ip), serverPort(port), timeoutMillis(timeoutMillis) {}
        ~Server() {}
        
        // Server control method
        void attach();
        void detach();
        void control(google::protobuf::Message *message);
        void control(int packetType, char *message, unsigned int length);
        
        // Getter for server
        std::string getServerIPAddress() { return serverIPAddress; }
        int getServerPort() { return serverPort; }
        int getIdentifier() { return identifier; }
        int getTimeoutMillis() { return timeoutMillis; }
        BlockingQueue<MessageWrapper *> *getEventQueue() { return eventQueue; }
        std::shared_ptr<Session> getSession() { return sess; }
        
        // Connection lifecycle
        std::function<void(std::shared_ptr<Session>)> onAttaching;
        std::function<void(std::shared_ptr<Session>)> onAttachFailed;
        std::function<void(std::shared_ptr<Session>)> onAttached;
        std::function<void(std::shared_ptr<Session>)> onDetached;
        std::function<void(std::shared_ptr<Session>)> onDetach;
        std::function<void(std::shared_ptr<Session>)> onTimeout;
        
    public:
        Thread *mainThread;
        Thread *sendThread;
        Thread *recvThread;
        
    private:
        std::shared_ptr<Session> sess;
        int identifier;
        
        BlockingQueue<MessageWrapper *> *eventQueue;
        
        std::string serverIPAddress;
        int serverPort;
        
        int timeoutMillis;
    };
}
