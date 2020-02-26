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
        std::map<int, std::shared_ptr<Session>> &getClientMap();
        
        bool addClient(std::shared_ptr<TCPSocket> clientSocket, std::shared_ptr<Session> sess);
        bool removeClient(std::shared_ptr<ijoon::Session> session);
        bool removeClient(std::shared_ptr<TCPSocket> clientSocket);
        bool removeClient(int socketId);
        
        ushort getServerPort();
        int getRecvTimeoutMs();
        
        bool isMultiThreadBased() { return this->useMultiThread; }
        
        // Server lifecycle
        std::function<void()> onServerStarted;
        std::function<void()> onServerStopped;
        
        // Client lifecycle
        std::function<std::shared_ptr<ijoon::Session>(std::shared_ptr<ijoon::TCPSocket>)> getClientSession;
        std::function<void(std::shared_ptr<Session>)> onClientConnected;
        std::function<void(std::shared_ptr<Session>)> onClientTimeout;
        std::function<void(std::shared_ptr<Session>)> onClientDisconnected;

    private:
        std::map<int, std::shared_ptr<Session>> clientMap;
        Thread *thread;
        ushort port;
        int recvTimeoutMs;
        bool useMultiThread;
    };
}
