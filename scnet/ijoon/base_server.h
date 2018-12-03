#pragma once
/* std headers */
#include <map>
#include <functional>
#include <assert.h>

/* custom headers */
#include "base_session.h"

namespace ijoon {
    class BaseServer {
    public:
        BaseServer(int port, int recvTimeoutMs, bool useMultiThread);
        ~BaseServer();
        
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
        virtual void onServerStarted() {};
        virtual void onServerStopped() {};
        virtual BaseSession *getSession(std::shared_ptr<ijoon::JClientSocket> clntSocket) = 0;
        
        // Client lifecycle
        virtual void onClientServiceStarted(BaseSession *session) {};
        virtual void onClientServiceTimeout(BaseSession *session) {};
        virtual void onClientServiceDisconnected(BaseSession *session) {};
        virtual void onClientServiceStopped(BaseSession *session) {};
        virtual void onClientServiceCallback(BaseSession *session, google::protobuf::Message *message) = 0;
        

    private:
        std::map<NativeSocket, BaseSession *> clientMap;
        Thread *thread;
        int port;
        int recvTimeoutMs;
        bool useMultiThread;
    };
}
