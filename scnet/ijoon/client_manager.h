#pragma once
/* std headers */
#include <map>
#include <functional>
#include <assert.h>

/* custom headers */
#include "session.h"

namespace ijoon {
    class ClientManager {
    public:
        ClientManager(ushort port, int recvTimeoutMs, bool useMultiThread);
        ~ClientManager();
        
        bool start();
        bool stop();
        
        int clientSize();
        std::shared_ptr<Session> session(int socketId);
        
        bool addClient(std::shared_ptr<TCPSocket> clientSocket, std::shared_ptr<Session> sess);
        bool removeClient(std::shared_ptr<TCPSocket> clientSocket);
        bool removeClient(int socketId);
        
        ushort getServerPort();
        int getRecvTimeoutMs();
        
        bool isMultiThreadBased() { return this->useMultiThread; }
        
        // Server lifecycle
        std::function<void()> onServerStarted;
        std::function<void()> onServerStopped;
        
        // Client lifecycle
        std::function<void(std::shared_ptr<Session>)> onClientConnected;
        std::function<void(std::shared_ptr<Session>)> onClientServiceStarted;
        std::function<void(std::shared_ptr<Session>)> onClientServiceTimeout;
        std::function<void(std::shared_ptr<Session>)> onClientServiceDisconnected;
        std::function<void(std::shared_ptr<Session>)> onClientServiceStopped;

    private:
        std::map<int, std::shared_ptr<Session>> clientMap;
        Thread *thread;
        ushort port;
        int recvTimeoutMs;
        bool useMultiThread;
    };
}
