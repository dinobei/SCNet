#pragma once
#include <map>
#include <ijoon/coreutils.h>
#include "randezvous_peer.h"

namespace ijoon {
    ijoon::THREAD_RET THREAD_API randezvousThread(void *arg);
    
    // udp 소켓으로 메시지를 수신해서 메시지를 읽어옴
    // 이후 소켓과 피어정보로 랑데뷰피어를 만들어서 메시지를 처리하도록 한다.
    
    class RandezvousPeerInfo {
    public:
        uint connectionID;
        std::string spUniqueKey;
        std::string tpUniqueKey;
    };
    
    class RandezvousServer {
    public:
        RandezvousServer(int port): serverPort(port) {}
        ~RandezvousServer() {}

        void start();
                
    public:
        int serverPort;
        std::map<std::string, std::shared_ptr<RandezvousPeer>> registeredRandezvousPeer;
        std::map<std::string, std::shared_ptr<ijoon::Peer>> relayServerMap;
        
        std::map<int, std::shared_ptr<RandezvousPeerInfo>> randezvousPeerInfoMap;
        
        ijoon::Thread *thread;
    };
}
