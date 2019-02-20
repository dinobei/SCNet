#pragma once
#include <map>
#include <ijoon/coreutils.h>
#include "session.h"

namespace ijoon {
    ijoon::THREAD_RET THREAD_API rendezvousCheckThread(void *arg);
    
    ijoon::THREAD_RET THREAD_API rawRecvThreadFunc(void *arg);
    ijoon::THREAD_RET THREAD_API recvThreadFunc(void *arg);
    
    class ConnectionInfo {
    public:
        uint connectionID;
        ijoon::Peer sp;
        ijoon::Peer tp;
    };
    
    class RendezvousServer {
    public:
        RendezvousServer(int port): serverPort(port), socket(std::shared_ptr<UDPSocket>(new UDPSocket(port))), connectionIDCursor(10) {}
        ~RendezvousServer() {}

        void start();
                
    public:
        int serverPort;
        std::shared_ptr<UDPSocket> socket;
        std::map<std::string, std::shared_ptr<KcpPeer>> kcpPeerMap;
        std::map<std::string, std::shared_ptr<RendezvousSession>> rendezvousSessionMap;
        std::map<std::string, std::shared_ptr<KcpPeer>> relayServerMap;
        ijoon::Mutex mutex;
        
        int connectionIDCursor;
        std::map<int, std::shared_ptr<ConnectionInfo>> connectionInfoMap;
        
        ijoon::Thread *thread;
        ijoon::Thread *checkThread;
        
        ijoon::Thread *recvThread;
        ijoon::Thread *rawRecvThread;
        
        uint32_t lastCheckTime;
    };
}
