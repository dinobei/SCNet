#pragma once
#include <ijoon/coreutils.h>
#include <functional>
#include "session.h"

namespace ijoon {
    class RendezvousClient {
    public:
        RendezvousClient(std::string ip, std::string port, std::string ifname, std::string serial = "unknown"): socket(std::shared_ptr<UDPSocket>(new UDPSocket(0))), serverIP(ip), serverPort(port), ifname(ifname), serial(serial), isConected(false) {
            assert(!serial.empty());
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
        
        std::string serverIP;
        std::string serverPort;
        std::string ifname;
        std::string serial;
        bool isConected;
        
        void onServerConnectingCallback();
        void onServerConnectFailedCallback();
        void onServerConnectedCallback(std::string extIP, std::string extPort);
        void onServerDisconnectedCallback();
        
        std::function<void()> onServerConnecting;
        std::function<void()> onServerConnectFailed;
        std::function<void(std::string extIP, std::string extPort)> onServerConnected;
        std::function<void()> onServerDisconnected;
        
        std::function<void(std::shared_ptr<RendezvousSession> rendezvousClient)> onConnecting;
        std::function<void(std::shared_ptr<RendezvousSession> rendezvousClient)> onConnected;
        std::function<void(std::shared_ptr<RendezvousSession> rendezvousClient)> onConnectionTargetInvalid;
        std::function<void(int connectionID, std::string targetIP, std::string targetPort)> onConnectionIDCreated;
        std::function<void(std::shared_ptr<RendezvousSession> rendezvousClient)> onConnectFailed;
        std::function<void(std::shared_ptr<RendezvousSession> rendezvousClient)> onDisconnected;
        
        std::shared_ptr<ijoon::KcpPeer> getKcpPeer(ijoon::Peer peer);
    };
}
