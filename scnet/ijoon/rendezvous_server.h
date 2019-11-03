#pragma once
#include <functional>
#include <map>
#include <ijoon/coreutils.h>
#include "session.h"

namespace ijoon {
    class ConnectionInfo {
    public:
        uint connectionID;
        std::shared_ptr<ijoon::Peer> publicSP;
        std::shared_ptr<ijoon::Peer> privateSP;
        std::shared_ptr<ijoon::Peer> publicTP;
        std::shared_ptr<ijoon::Peer> privateTP;
    };
    
    class RendezvousServerLocalCallback {
    public:
        bool registerRendezvousClientCallback(std::string serial, std::string publicIP, std::string publicPort, std::string privateIP, std::string privatePort, std::string mac, std::string version);
        bool removeRendezvousClientCallback(std::string ip, std::string port);
        std::shared_ptr<ijoon::Peer> getRendezvousClientPeerCallback(std::string ip, std::string port);
        
        bool registerRelayServerCallback(std::string name, std::string ip, std::string port, std::string version);
        bool removeRelayServerCallback(std::string ip, std::string port);
        bool isExistRelayServerCallback(std::string ip, std::string port);
        std::shared_ptr<ijoon::Peer> getRelayServerPeerCallback();
        
    public:
        std::function<bool(std::string, std::string, std::string, std::string, std::string, std::string, std::string)> registerRendezvousClient;
        std::function<bool(std::string, std::string)> removeRendezvousClient;
        std::function<std::shared_ptr<ijoon::Peer>(std::string, std::string)> getRendezvousClientPeer;
        
        std::function<bool(std::string, std::string, std::string, std::string)> registerRelayServer; // name, pub_ip, pub_port, ver
        std::function<bool(std::string, std::string)> removeRelayServer; // pub_ip, pub_port
        std::function<bool(std::string, std::string)> isExistRelayServer; // pub_ip, pub_port
        std::function<std::shared_ptr<ijoon::Peer>()> getRelayServerPeer;
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
        ijoon::Mutex mutexForKcpPeerMap;
        
        int connectionIDCursor;
        std::map<int, std::shared_ptr<ConnectionInfo>> connectionInfoMap;
        ijoon::Mutex mutexForConnectionInfoMap;
        
        ijoon::Thread *checkThread;        
        ijoon::Thread *recvThread;
        ijoon::Thread *rawRecvThread;
        
        std::shared_ptr<ijoon::KcpPeer> getKcpPeer(std::shared_ptr<ijoon::Peer> peer);
        
    public:
        RendezvousServerLocalCallback callback;
    };
}
