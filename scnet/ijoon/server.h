#pragma once
/* std headers */
#include <map>
#include <functional>
#include <assert.h>

/* custom headers */
#include "base_session.h"

namespace ijoon {
    class Server {
    public:
        Server(int port, int recvTimeoutMs, bool useMultiThread);
        ~Server();
        
        bool start();
        bool stop();
        
        int clientSize();
        BaseSession* session(NativeSocket nativeSocket);
        
        bool addClient(std::shared_ptr<JClientSocket> clientSocket, BaseSession *sess);
        bool removeClient(std::shared_ptr<JClientSocket> clientSocket);
        bool removeClient(NativeSocket nativeSocket);
        
        int getServerPort();
        int getRecvTimeoutMs();
        
        bool isMultiThreadBased() { return this->useMultiThread; }
        
        // Server lifecycle
        std::function<void()> onServerStarted;
        std::function<void()> onServerStopped;
        
        // Client lifecycle
        std::function<void(BaseSession *)> onClientConnected;
        std::function<void(BaseSession *)> onClientServiceStarted;
        std::function<void(BaseSession *)> onClientServiceTimeout;
        std::function<void(BaseSession *)> onClientServiceDisconnected;
        std::function<void(BaseSession *)> onClientServiceStopped;

    private:
        std::map<NativeSocket, BaseSession *> clientMap;
        Thread *thread;
        int port;
        int recvTimeoutMs;
        bool useMultiThread;
    };
}
