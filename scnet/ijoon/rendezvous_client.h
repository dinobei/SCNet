#pragma once
#include <ijoon/coreutils.h>
#include <functional>
#include "session.h"

namespace ijoon {
    ijoon::THREAD_RET THREAD_API registerThreadFunc(void *arg);
    ijoon::THREAD_RET THREAD_API rawRecvThreadFunc(void *arg);
    ijoon::THREAD_RET THREAD_API recvThreadFunc(void *arg);
    
    class RendezvousClient {
    public:
        RendezvousClient(std::string ip, std::string port, std::string ifname): socket(std::shared_ptr<UDPSocket>(new UDPSocket(0))), ifname(ifname), serial("") {
            rendezvousServerPeer.setIP(ip);
            rendezvousServerPeer.setPort(port);
        }
        RendezvousClient(std::string serial, std::string ip, std::string port, std::string ifname): socket(std::shared_ptr<UDPSocket>(new UDPSocket(0))), ifname(ifname), serial(serial) {
            rendezvousServerPeer.setIP(ip);
            rendezvousServerPeer.setPort(port);
        }
        ~RendezvousClient() {}
        
        void start();

    public:
        std::shared_ptr<UDPSocket> socket;
        ijoon::Thread *registerThread;
        ijoon::Thread *recvThread;
        ijoon::Thread *rawRecvThread;
        
        std::map<std::string, std::shared_ptr<ijoon::KcpPeer>> kcpPeerMap;
        std::map<int, std::shared_ptr<ijoon::RendezvousSession>> rendezvousSessionMap;
        
        ijoon::Peer rendezvousServerPeer;
        std::string ifname;
        std::string serial;
        
        std::function<void(std::shared_ptr<RendezvousSession> rendezvousClient)> onConnectingCallback;
        std::function<void(std::shared_ptr<RendezvousSession> rendezvousClient)> onConnectedCallback;
        std::function<void(std::shared_ptr<RendezvousSession> rendezvousClient)> onConnectFailedCallback;
        
        void send(std::shared_ptr<ijoon::KcpPeer> kcpPeer, int packetType, char *message, unsigned int length);
        void send(std::shared_ptr<ijoon::KcpPeer> kcpPeer, std::shared_ptr<google::protobuf::Message> message);
        void send(ijoon::Peer peer, int packetType, char *message, unsigned int length);
        void send(ijoon::Peer peer, std::shared_ptr<google::protobuf::Message> message);
    };
}
