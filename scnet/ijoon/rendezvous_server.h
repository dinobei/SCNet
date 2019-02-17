#pragma once
#include <map>
#include <ijoon/coreutils.h>
#include "rendezvous_peer.h"

namespace ijoon {
    ijoon::THREAD_RET THREAD_API rendezvousThread(void *arg);
    ijoon::THREAD_RET THREAD_API rendezvousCheckThread(void *arg);
    
    // udp 소켓으로 메시지를 수신해서 메시지를 읽어옴
    // 이후 소켓과 피어정보로 랑데뷰피어를 만들어서 메시지를 처리하도록 한다.
    
    class ConnectionInfo {
    public:
        uint connectionID;
        ijoon::Peer sp;
        ijoon::Peer tp;
    };
    
    class RendezvousServer {
    public:
        RendezvousServer(int port): serverPort(port), socket(std::shared_ptr<UDPSocket>(new UDPSocket(port))), connectionIDCursor(0) {}
        ~RendezvousServer() {}

        void start();
                
    public:
        int serverPort;
        std::shared_ptr<UDPSocket> socket;
        std::map<std::string, std::shared_ptr<RendezvousPeer>> rendezvousPeerMap;
        std::map<std::string, std::shared_ptr<RendezvousPeer>> relayServerMap;
        
        int connectionIDCursor;
        std::map<int, std::shared_ptr<ConnectionInfo>> connectionInfoMap;
        
        ijoon::Thread *thread;
        ijoon::Thread *checkThread;
    };
}
