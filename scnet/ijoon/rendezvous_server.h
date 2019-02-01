#pragma once
#include <map>
#include <ijoon/coreutils.h>
#include "rendezvous_peer.h"

namespace ijoon {
    ijoon::THREAD_RET THREAD_API rendezvousThread(void *arg);
    
    // udp 소켓으로 메시지를 수신해서 메시지를 읽어옴
    // 이후 소켓과 피어정보로 랑데뷰피어를 만들어서 메시지를 처리하도록 한다.
    
    class RendezvousPeerInfo {
    public:
        uint connectionID;
        ijoon::Peer sp;
        ijoon::Peer tp;
    };
    
    class RendezvousServer {
    public:
        RendezvousServer(int port): serverPort(port), connectionIDCursor(0) {}
        ~RendezvousServer() {}

        void start();
                
    public:
        int serverPort;
        std::map<std::string, std::shared_ptr<RendezvousPeer>> registeredRendezvousPeer;
        std::map<std::string, std::shared_ptr<ijoon::Peer>> relayServerMap;
        
        int connectionIDCursor;
        std::map<int, std::shared_ptr<RendezvousPeerInfo>> connectionInfoMap;
        
        ijoon::Thread *thread;
    };
}
