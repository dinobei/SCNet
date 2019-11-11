#pragma once
#include <ijoon/coreutils.h>
#include <functional>
#include "registry.h"
#include "rendezvous_message.h"
#include "rendezvous_client_callback.h"

namespace ijoon {
    class RendezvousClientLifeCycleCallback {
    public:
        void onServerConnectingCallback();
        void onServerConnectFailedCallback();
        void onServerConnectedCallback(std::string extIP, std::string extPort);
        void onServerDisconnectedCallback();
        
        void onConnectingCallback(int connectionID);
        void onConnectedCallback(std::shared_ptr<ijoon::KcpPeer> rendezvousClient);
        void onConnectFailedCallback(std::string ip, std::string port);
        void onDisconnectedCallback(std::shared_ptr<ijoon::KcpPeer> rendezvousClient);
        
    public:
        std::function<void()> onServerConnecting;
        std::function<void()> onServerConnectFailed;
        std::function<void(std::string extIP, std::string extPort)> onServerConnected;
        std::function<void()> onServerDisconnected;
        
        std::function<void(int connectionID)> onConnecting;
        std::function<void(std::shared_ptr<ijoon::KcpPeer> rendezvousClient)> onConnected;
        std::function<void(std::string ip, std::string port)> onConnectFailed;
        std::function<void(std::shared_ptr<ijoon::KcpPeer> rendezvousClient)> onDisconnected;
    };

    // reference for singleton: https://stackoverflow.com/a/52308483
    class RendezvousClient {
    public:
        static void init(std::string ip, std::string port, std::string serial, std::string mac, std::string version) // enable moving in
        {
            getInstanceImpl(&ip, &port, &serial, &mac, &version);
        }
        
        static RendezvousClient& getInstance() {
            return getInstanceImpl();
        }
        
        void start();
        void stop();
        void connect(std::string ip, std::string port);
        
        RendezvousClient(RendezvousClient const&) = delete;
        void operator=(RendezvousClient const&) = delete;
    private:
        static RendezvousClient& getInstanceImpl(std::string* const ip = nullptr,
                                                 std::string* const port = nullptr,
                                                 std::string* const serial = nullptr,
                                                 std::string* const mac = nullptr,
                                                 std::string* const version = nullptr) {
            static RendezvousClient instance{ ip, port, serial, mac, version };
            return instance;
        }
        
        RendezvousClient(std::string* const ip, std::string* const port, std::string* const serial, std::string* const mac, std::string* const version):
            socket(std::shared_ptr<UDPSocket>(new UDPSocket(0))),
            serverIP{ ip ? move(*ip) : std::string{} },
            serverPort{ port ? move(*port) : std::string{} },
            serial{ serial ? move(*serial) : std::string{} },
            mac{ mac ? move(*mac) : std::string{} },
            version{ version ? move(*version) : std::string{} }
        {
            if(nullptr == ip || nullptr == port ||
               nullptr == serial || nullptr == mac || nullptr == version) {
                throw std::runtime_error{"RendezvousClient did not initialized properly"};
            }
            
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::REGISTRATION_RENDEZVOUS_CLIENT_RESPONSE, renc::onRegistrationRendezvousClientResponse);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::CONNECTION_RELAY_SERVICE_RESULT, renc::onConnectionRelayServiceResult);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::DIRECT_CONNECTION_REQUEST, renc::onDirectConnectionRequest);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::DIRECT_CONNECTION_RESPONSE, renc::onDirectConnectionResponse);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::REVERSE_CONNECTION_REQUEST, renc::onReverseConnectionRequest);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::REVERSE_CONNECTION_RESPONSE, renc::onReverseConnectionResponse);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::UDP_HOLE_PUNCHING_REQUEST, renc::onUdpHolePunchingRequest);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::UDP_HOLE_PUNCHING_RESPONSE, renc::onUdpHolePunchingResponse);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::CONNECTION_TARGET_INVALID, renc::onConnectionTargetInvalid);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::CONNECTION_ID_CREATED, renc::onConnectionIdCreated);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::DIRECT_CONNECTION_AVAILABLE, renc::onDirectConnectionAvailable);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::REVERSE_CONNECTION_READY, renc::onReverseConnectionReady);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::REVERSE_CONNECTION, renc::onReverseConnection);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::UDP_HOLE_PUNCHING_AVAILABLE, renc::onUdpHolePunchingAvailable);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::RELAY_SERVER_INFORMATION, renc::onRelayServerInformation);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::RELAY_SESSION_INVALID, renc::onRelaySessionInvalid);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::RELAY_SERVER_DISCONNECTED, renc::onRelayServerDisconnected);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::PING_REQUEST, renc::onPingRequest);
            SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::PING_RESPONSE, renc::onPingResponse);
            
            auto serverKcpPeer = getKcpPeer(ijoon::Peer(serverIP, serverPort));
            serverKcpPeer->type = ijoon::PeerType::RENDEZVOUS_SERVER;
        }
        ~RendezvousClient() {}
        
    public:
        std::shared_ptr<UDPSocket> socket;
        ijoon::Thread *registerThread;
        ijoon::Thread *recvThread;
        ijoon::Thread *rawRecvThread;
        
        std::map<std::string, std::shared_ptr<ijoon::KcpPeer>> kcpPeerMap;
        std::multimap<int, ijoon::Peer> connFilterMap;
        ijoon::Mutex mutexForKcpPeerMap;
        ijoon::Mutex mutexForConnFilterMap;
        
        std::string serverIP;
        std::string serverPort;
        std::string serial;
        std::string mac;
        std::string version;
        uint connectionID;
        
        ijoon::RendezvousClientLifeCycleCallback callback;

        std::shared_ptr<ijoon::KcpPeer> getKcpPeer(ijoon::Peer peer);
        std::shared_ptr<ijoon::KcpPeer> getKcpPeer(int connectionID);
    };
}
