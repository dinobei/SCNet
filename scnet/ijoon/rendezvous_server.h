#pragma once
#include <map>
#include <ijoon/coreutils.h>
#include "session.h"

namespace ijoon {
    ijoon::THREAD_RET THREAD_API rendezvousCheckThreadFunc(void *arg);
    ijoon::THREAD_RET THREAD_API rawRecvThreadFunc(void *arg);
    ijoon::THREAD_RET THREAD_API recvThreadFunc(void *arg);
    
    class ConnectionInfo {
    public:
        uint connectionID;
        std::shared_ptr<ijoon::Peer> publicSP;
        std::shared_ptr<ijoon::Peer> privateSP;
        std::shared_ptr<ijoon::Peer> publicTP;
        std::shared_ptr<ijoon::Peer> privateTP;
    };
    
    class RendezvousServer {
    public:
        RendezvousServer(int port): serverPort(port), socket(std::shared_ptr<UDPSocket>(new UDPSocket(port))), connectionIDCursor(1) {}
        ~RendezvousServer() {}

        void start();
                
    public:
        int serverPort;
        std::shared_ptr<UDPSocket> socket;
        std::map<std::string, std::shared_ptr<KcpPeer>> kcpPeerMap; // public address, kcp peer
        std::map<std::string, std::shared_ptr<KcpPeer>> relayServerMap;
        
        int connectionIDCursor;
        std::map<int, std::shared_ptr<ConnectionInfo>> connectionInfoMap;
        
        ijoon::Thread *checkThread;        
        ijoon::Thread *recvThread;
        ijoon::Thread *rawRecvThread;
        
    public:
        std::function<void(std::string, std::string, std::string, std::string, std::string)> registerRendezvousClient;
        std::function<void(std::string, std::string)> removeRendezvousClient;
        std::function<std::shared_ptr<ijoon::Peer>(std::string, std::string)> getRendezvousClient;
    };
}
