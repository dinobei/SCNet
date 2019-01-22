#pragma once
#include <map>
#include <ijoon/coreutils.h>

namespace ijoon {
    ijoon::THREAD_RET THREAD_API registerThread(void *arg);
    ijoon::THREAD_RET THREAD_API mainThread(void *arg);
    
    class RelayPeerInfo {
    public:
        ijoon::Peer sourcePeer;
        ijoon::Peer targetPeer;
    };
    
    class RelayServer {
    public:
        RelayServer(int port, std::string ranIP, std::string ranPort): serverPort(port), socket(std::shared_ptr<UDPSocket>(new UDPSocket(serverPort))) {
            randezvousPeer.setIP(ranIP);
            randezvousPeer.setPort(ranPort);
        }
        ~RelayServer() {}
        
        void start();
        
    public:
        int serverPort;
        std::shared_ptr<UDPSocket> socket;
        
        std::map<int, std::shared_ptr<RelayPeerInfo>> map;
        std::map<int, int> sessionCheckMap;
        
        ijoon::Thread *mainThread;
        ijoon::Thread *registerThread;
        
        ijoon::Peer randezvousPeer;
    };
}
