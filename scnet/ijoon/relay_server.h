#pragma once
#include <map>
#include <ijoon/coreutils.h>
#include "session.h"

namespace ijoon {
    class RelayPeerInfo {
    public:
        RelayPeerInfo() {}
        ~RelayPeerInfo() {}
    public:
        std::shared_ptr<ijoon::KcpPeer> sourceKcpPeer;
        std::shared_ptr<ijoon::KcpPeer> targetKcpPeer;
    };
    
    class RelayServer {
    public:
        RelayServer(int port, std::string renIP, std::string renPort): serverPort(port), socket(std::shared_ptr<UDPSocket>(new UDPSocket(serverPort))), lastRegistrationTime(0) {
            serverPeer = std::shared_ptr<ijoon::Peer>(new ijoon::Peer(renIP, renPort));
            
            auto serverKcpPeer = getKcpPeer(serverPeer);
            serverKcpPeer->type = ijoon::PeerType::RENDEZVOUS_SERVER;
        }
        ~RelayServer() {}
        
        void start();
        
        std::shared_ptr<ijoon::KcpPeer> getKcpPeer(std::shared_ptr<ijoon::Peer> peer);
        
    public:
        int serverPort;
        std::shared_ptr<UDPSocket> socket;
        
        std::map<std::string, std::shared_ptr<ijoon::KcpPeer>> kcpPeerMap;
        std::map<int, std::shared_ptr<RelayPeerInfo>> map;
        std::map<int, int> sessionCheckMap;
        
        ijoon::Mutex mutexForKcpPeerMap;
        ijoon::Mutex mutexForMap;
        
        ijoon::Thread *rawRecvThread;
        ijoon::Thread *recvThread;
        ijoon::Thread *registerThread;
        
        std::shared_ptr<ijoon::Peer> serverPeer;

        long lastRegistrationTime;
    };
}
