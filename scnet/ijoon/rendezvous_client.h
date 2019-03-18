#pragma once
#include <ijoon/coreutils.h>
#include <functional>
#include "session.h"

namespace ijoon {
    ijoon::THREAD_RET THREAD_API registerThreadFunc(void *arg);
    ijoon::THREAD_RET THREAD_API recvThreadFunc(void *arg);
    
    class RendezvousClient {
    public:
        RendezvousClient(std::string ip, std::string port, std::string ifname, std::string serial = "unknown"): socket(std::shared_ptr<UDPSocket>(new UDPSocket(0))), ifname(ifname), serial(serial), lastRegistrationTime(0) {
            assert(!serial.empty());
            rendezvousServerKcpPeer = getKcpPeer(std::shared_ptr<ijoon::Peer>(new ijoon::Peer(ip, port)));
        }
        ~RendezvousClient() {}
        
        void start();
        void stop();

    public:
        std::shared_ptr<UDPSocket> socket;
        ijoon::Thread *registerThread;
        ijoon::Thread *recvThread;
        ijoon::Thread *rawRecvThread;
        
        std::map<std::string, std::shared_ptr<ijoon::KcpPeer>> kcpPeerMap;
        std::map<int, std::shared_ptr<ijoon::RendezvousSession>> rendezvousSessionMap;
        ijoon::Mutex mutexForKcpPeerMap;
        ijoon::Mutex mutexForRendezvousSessionMap;
        
        std::shared_ptr<ijoon::KcpPeer> rendezvousServerKcpPeer;
        std::string ifname;
        std::string serial;
        
        long lastRegistrationTime;
        
        std::function<void()> onServerConnecting;
        std::function<void()> onServerConnectFailed;
        std::function<void(std::string extIP, std::string extPort)> onServerConnected;
        std::function<void()> onServerDisconnected;
        
        std::function<void(std::shared_ptr<RendezvousSession> rendezvousClient)> onConnectingCallback;
        std::function<void(std::shared_ptr<RendezvousSession> rendezvousClient)> onConnectedCallback;
        std::function<void(std::shared_ptr<RendezvousSession> rendezvousClient)> onConnectionTargetInvalidCallback;
        std::function<void(int connectionID, std::string targetIP, std::string targetPort)> onConnectionIDCreatedCallback;
        std::function<void(std::shared_ptr<RendezvousSession> rendezvousClient)> onConnectFailedCallback;
        
        std::shared_ptr<ijoon::KcpPeer> getKcpPeer(std::shared_ptr<ijoon::Peer> peer);
    };
}
