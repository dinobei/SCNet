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
        ClientManager(int port, int recvTimeoutMs, bool useMultiThread);
        ~ClientManager();
        
        bool start();
        bool stop();
        
        int clientSize();
        Session* session(NativeSocket nativeSocket);
        
        bool removeClient(NativeSocket nativeSocket);
        bool addClient(std::shared_ptr<ClientTCPSocket> clientSocket, Session *sess);
        bool removeClient(std::shared_ptr<ClientTCPSocket> clientSocket);
        
        int getServerPort();
        int getRecvTimeoutMs();
        
        bool isMultiThreadBased() { return this->useMultiThread; }
        
        // Server lifecycle
        std::function<void()> onServerStarted;
        std::function<void()> onServerStopped;
        
        // Client lifecycle
        std::function<void(Session *)> onClientConnected;
        std::function<void(Session *)> onClientServiceStarted;
        std::function<void(Session *)> onClientServiceTimeout;
        std::function<void(Session *)> onClientServiceDisconnected;
        std::function<void(Session *)> onClientServiceStopped;

    private:
        std::map<NativeSocket, Session *> clientMap;
        Thread *thread;
        int port;
        int recvTimeoutMs;
        bool useMultiThread;
    };
}
