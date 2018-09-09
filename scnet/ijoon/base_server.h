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
        BaseServer(int port, int recvTimeoutMs);
        ~BaseServer();
        
        bool start();
        bool stop();
        
        int clientSize();
        BaseSession* session(std::string key);
        
        bool addClient(BaseSession *sess);
        bool removeClient(std::string key);
        
        int getServerPort();
        int getRecvTimeoutMs();
        
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
        std::map<std::string, BaseSession *> clientMap;
        Thread *thread;
        int port;
        int recvTimeoutMs;
    };
}
