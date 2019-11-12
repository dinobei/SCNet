#pragma once
#include <functional>
#include <map>
#include <ijoon/coreutils.h>
#include "registry.h"
#include "rendezvous_message.h"
#include "rendezvous_server_callback.h"

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
        static void init(int port) // enable moving in
        {
            getInstanceImpl(port);
        }
        
        static RendezvousServer& getInstance() {
            return getInstanceImpl();
        }
        
        void start();
        
        RendezvousServer(RendezvousServer const&) = delete;
        void operator=(RendezvousServer const&) = delete;
        
    private:
        static RendezvousServer& getInstanceImpl(int port = -1) {
            static RendezvousServer instance{ port };
            return instance;
        }
        
        RendezvousServer(int port):
            serverPort(port),
            socket(std::shared_ptr<UDPSocket>(new UDPSocket(port))),
            connectionIDCursor(1)
        {
            if(port < 0 || port > 65535) {
                throw std::runtime_error{"RendezvousServer did not initialized properly"};
            }
            
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::REGISTRATION_RENDEZVOUS_CLIENT_REQUEST, rens::onRegistrationRendezvousClientRequest);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::REGISTRATION_RELAY_SERVER_REQUEST, rens::onRegistrationRelayServerRequest);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::RELAY_SESSION_READY, rens::onRelaySessionReady);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::RELAY_SESSION_CREATED, rens::onRelaySessionCreated);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::CONNECTION_REQUEST, rens::onConnectionRequest);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::CONNECTION_ID_RECEIVED, rens::onConnectionIdReceived);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::PING_REQUEST, rens::onPingRequest);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::UNREGISTRATION_RENDEZVOUS_CLIENT_REQUEST, rens::onUnregistrationRendezvousClientRequest);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::UNREGISTRATION_RELAY_SERVER_REQUEST, rens::onUnregistrationRelayServerRequest);
        }
        
        ~RendezvousServer() {}
        
    private:
        std::map<int, std::shared_ptr<ConnectionInfo>> connectionInfoMap;
        ijoon::Mutex mutexForConnectionInfoMap;
        std::map<std::string, std::shared_ptr<KcpPeer>> kcpPeerMap; // public address, kcp peer
        ijoon::Mutex mutexForKcpPeerMap;
        
    public:
        int serverPort;
        std::shared_ptr<UDPSocket> socket;
        
        ijoon::Thread *checkThread;
        ijoon::Thread *recvThread;
        ijoon::Thread *rawRecvThread;
        
        int connectionIDCursor;
        
        // Access ConnectionInfoMap
        std::map<int, std::shared_ptr<ConnectionInfo>> getConnectionInfoMap();
        std::shared_ptr<ConnectionInfo> getConnectionInfo(int connectionID);
        void setConnectionInfo(int connectionID, std::shared_ptr<ConnectionInfo> connectionInfo);
        void removeConnectionInfo(int connectionID);
        
        // Access KcpPeerMap
        std::map<std::string, std::shared_ptr<KcpPeer>> getKcpPeerMap();
        std::shared_ptr<ijoon::KcpPeer> getKcpPeer(std::shared_ptr<ijoon::Peer> peer);
        std::shared_ptr<ijoon::KcpPeer> getKcpPeerWithLock(std::shared_ptr<ijoon::Peer> peer);
        void removeKcpPeer(std::string key);
        void checkKcpPeerMap(const int timeout, const int checkIntervalMs);
        void receivedDataDistToKcpPeer(char *buffer, IUINT32 current);
        
    public:
        RendezvousServerLocalCallback callback;
    };
}
