#pragma once
#include <ijoon/coreutils.h>
#include <functional>
#include "session.h"

namespace ijoon {
    ijoon::THREAD_RET THREAD_API registerThread(void *arg);
    ijoon::THREAD_RET THREAD_API recvThread(void *arg);
    
    class RendezvousClient {
    public:
        RendezvousClient(std::string ip, std::string port): socket(std::shared_ptr<UDPSocket>(new UDPSocket(0))) {
            rendezvousServerPeer.setIP(ip);
            rendezvousServerPeer.setPort(port);
        }
        ~RendezvousClient() {}
        
        void start();

    public:
        std::shared_ptr<UDPSocket> socket;
        ijoon::Thread *register_thread;
        ijoon::Thread *recv_thread;
        
        std::map<int, std::shared_ptr<ijoon::RendezvousSession>> rendezvousSessionMap;
        
        ijoon::Peer rendezvousServerPeer;
        
        std::function<void(std::shared_ptr<RendezvousSession> rendezvousClient)> onConnectingCallback;
        std::function<void(std::shared_ptr<RendezvousSession> rendezvousClient)> onConnectedCallback;
        std::function<void(std::shared_ptr<RendezvousSession> rendezvousClient)> onConnectFailedCallback;
    };
}
