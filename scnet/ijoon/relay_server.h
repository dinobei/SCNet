#pragma once
#include <map>
#include <ijoon/coreutils.h>
#include "session.h"
#include "registry.h"
#include "rendezvous_message.h"
#include "relay_server_callback.h"

namespace ijoon {
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
        static void init(int port, std::string renIP, std::string renPort) // enable moving in
        {
            getInstanceImpl(port, &renIP, &renPort);
        }
        
        static RelayServer& getInstance() {
            return getInstanceImpl();
        }
        
        void start();
        
        RelayServer(RelayServer const&) = delete;
        void operator=(RelayServer const&) = delete;
        
    private:
        static RelayServer& getInstanceImpl(int port = -1,
                                            std::string* const renIP = nullptr,
                                            std::string* const renPort = nullptr) {
            static RelayServer instance{ port, renIP, renPort };
            return instance;
        }
        
        RelayServer(int port, std::string* rensIP, std::string* rensPort):
            serverPort(port),
            socket(std::shared_ptr<UDPSocket>(new UDPSocket(serverPort))),
            rensIP{ rensIP ? move(*rensIP) : std::string{} },
            rensPort{ rensPort ? move(*rensPort) : std::string{} } {
                
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::RELAY_SERVICE_REQUEST, rels::onRelayServiceRequest);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::REGISTRATION_RELAY_SERVER_RESPONSE, rels::onRegistrationRelayServerResponse);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::REGISTRATION_RELAY_PEER_REQUEST, rels::onRegistrationRelayPeerRequest);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::PING_REQUEST, rels::onPingRequest);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::PING_RESPONSE, rels::onPingResponse);
                
                
            serverPeer = ijoon::Peer(this->rensIP, this->rensPort);
            
            auto serverKcpPeer = getKcpPeer(serverPeer);
            serverKcpPeer->type = ijoon::PeerType::RENDEZVOUS_SERVER;
        }
        ~RelayServer() {}
        
    public:
        int serverPort;
        std::shared_ptr<UDPSocket> socket;
        
        ijoon::Thread *rawRecvThread;
        ijoon::Thread *recvThread;
        ijoon::Thread *registerThread;
        
        ijoon::Peer serverPeer;
        std::string rensIP;
        std::string rensPort;
        
    private:
        std::map<std::string, std::shared_ptr<ijoon::KcpPeer>> kcpPeerMap;
        std::map<int, std::shared_ptr<RelayPeerInfo>> map;
        std::map<int, int> sessionCheckMap;
        
        ijoon::Mutex mutexForKcpPeerMap;
        ijoon::Mutex mutexForMap;
        ijoon::Mutex mutexForSessionCheckMap;
        
    public:
        // Access kcpPeerMap
        std::map<std::string, std::shared_ptr<KcpPeer>> getKcpPeerMap();
        std::shared_ptr<ijoon::KcpPeer> getKcpPeer(ijoon::Peer &peer);
        std::shared_ptr<ijoon::KcpPeer> getKcpPeerWithLock(ijoon::Peer &peer);
        void checkKcpPeerMap(bool &connFlag, const int timeoutSec, const int pingIntervalSec, const int loopIntervalMs);
        void removeConnectionlessKcpPeer();
        void receivedDataDistToKcpPeer(char *buffer, IUINT32 current);
        
        // Access relayPeerInfoMap
        std::map<int, std::shared_ptr<RelayPeerInfo>> getRelayPeerInfoMap();
        void removeUnregisteredRelayPeerInfo();
        std::shared_ptr<RelayPeerInfo> getRelayPeerInfo(int connectionID);
        void setRelayPeerInfo(int connectionID, std::shared_ptr<RelayPeerInfo> relayPeerInfo);
        void removeRelayPeerInfo(int connectionID);
        
        // Access sessionCheckMap
        std::map<int, int> getSessionCheckMap();
        bool isExistCheckSession(int connectionID);
        void setCheckSession(int connectionID, int count);
        int getCheckSession(int connectionID);
        void removeCheckSession(int connectionID);
        
        
    };
}
