#pragma once
/* std headers */
#include <functional>
#include <assert.h>
#include <thread>
#include <atomic>

/* custom headers */
#include "session.h"

namespace ijoon {
    class Server {
    public:
        Server(std::string ip, int port, int timeoutMillis): sess(new Session()), identifier(-1), serverIPAddress(ip), serverPort(port), timeoutMillis(timeoutMillis) {}
        Server(int identifier, std::string ip, int port, int timeoutMillis): sess(new Session()), identifier(identifier), serverIPAddress(ip), serverPort(port), timeoutMillis(timeoutMillis) {}
        ~Server() {}
        
        // Server control method
        void attach();
        void detach();

        // Getter for server
        std::string getServerIPAddress() { return serverIPAddress; }
        int getServerPort() { return serverPort; }
        int getIdentifier() { return identifier; }
        int getTimeoutMillis() { return timeoutMillis; }
        std::shared_ptr<Session> getSession() { return sess; }
        
        // Connection lifecycle
        std::function<void(std::shared_ptr<Session>)> onAttaching;
        std::function<void(std::shared_ptr<Session>)> onAttachFailed;
        std::function<void(std::shared_ptr<Session>)> onAttached;
        std::function<void(std::shared_ptr<Session>)> onDetached;
        std::function<void(std::shared_ptr<Session>)> onDetach;
        std::function<void(std::shared_ptr<Session>)> onTimeout;
        
    public:
        std::thread *mainThread;
        std::atomic_bool mainThreadCondition;
        std::thread *recvThread;
        std::atomic_bool recvThreadCondition;
        
    private:
        std::shared_ptr<Session> sess;
        int identifier;
        
        std::string serverIPAddress;
        int serverPort;
        
        int timeoutMillis;
    };
}
