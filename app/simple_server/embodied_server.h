#pragma once
#include "base_server.h"
#include "embodied_session.h"
#include "registry.h"

/* implemented proto messages */
#include "packet.pb.h"
#include "packet_type.pb.h"
#include "get_image.pb.h"
using namespace example;

namespace ijoon {
    
    class EmbodiedServer : public BaseServer {
    public:
        EmbodiedServer(int port, int recvTimeoutMs, bool useMultiThread): BaseServer(port, recvTimeoutMs, useMultiThread) {}
        ~EmbodiedServer() {}
        
        // Virtual functions of BaseServer
        virtual void onServerStarted();
        virtual void onServerStopped();
        
        virtual void onClientServiceStarted(BaseSession *session);
        virtual void onClientServiceTimeout(BaseSession *session);
        virtual void onClientServiceDisconnected(BaseSession *session);
        virtual void onClientServiceStopped(BaseSession *session);
        
        virtual BaseSession *getSession(std::shared_ptr<ijoon::JClientSocket> clntSocket);
    };
    
}
