#pragma once
#include <map>
#include <ijoon/coreutils.h>
#include "session.h"

namespace ijoon {
    ijoon::THREAD_RET THREAD_API registerThread(void *arg);
    ijoon::THREAD_RET THREAD_API rawRecvThreadFunc(void *arg);
    ijoon::THREAD_RET THREAD_API recvThreadFunc(void *arg);
    
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
        RelayServer(int port, std::string ranIP, std::string ranPort): serverPort(port), socket(std::shared_ptr<UDPSocket>(new UDPSocket(serverPort))) {
            serverPeer.setIP(ranIP);
            serverPeer.setPort(ranPort);
        }
        ~RelayServer() {}
        
        void start();
        
    public:
        int serverPort;
        std::shared_ptr<UDPSocket> socket;
        
        std::map<std::string, std::shared_ptr<ijoon::KcpPeer>> kcpPeerMap;
        std::map<int, std::shared_ptr<RelayPeerInfo>> map;
        std::map<int, int> sessionCheckMap;
        
        ijoon::Thread *rawRecvThread;
        ijoon::Thread *recvThread;
        ijoon::Thread *registerThread;
        
        ijoon::Peer serverPeer;
    };
}
