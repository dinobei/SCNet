#pragma once
#include <iostream>
#include "message_header.h"
#include "ikcp.h"

namespace ijoon {
    class BaseSession {
    public:
        virtual ~BaseSession() {}
        virtual bool send(int packetType, char *message, unsigned int length) {return false;}
        virtual bool send(google::protobuf::Message *message) {return false;}
        virtual bool send(std::shared_ptr<google::protobuf::Message> message) {return false;}
        
        virtual bool recvHeader(MessageHeader &messageHeader) {return false;}
        virtual google::protobuf::Message *recvProtobufBody(MessageHeader &messageHeader) {return nullptr;}
        virtual char *recvRawBody(MessageHeader &messageHeader) {return nullptr;};

    };
    
    class KcpPeer {
    public:
        KcpPeer(std::shared_ptr<ijoon::UDPSocket> socket, std::string ip, std::string port,
                int (*output)(const char *buf, int len, ikcpcb *kcp, void *user)): socket(socket), connectionID(0) {
            // setup peer object
            peer.setIP(ip);
            peer.setPort(port);
            
            // setup kcp object
            kcp = ikcp_create(0x11223344, (void *)this);
            kcp->output = output;
            
            ikcp_wndsize(kcp, 128, 128);
            ikcp_nodelay(kcp, 0, 10, 0, 0);
            
            next = 0;
        }
        
        std::shared_ptr<ijoon::UDPSocket> getClientSocket()  {return this->socket;}
        ijoon::Peer getPeer() { return peer; }
        ikcpcb *getKcp() { return kcp; }
        
        void setConnectionID(int connectionID) { this->connectionID = connectionID; }
        int getConnectionID() { return connectionID; }
        
    public:
        ijoon::Mutex mutex;
        IUINT32 next;
        time_t lastPing;
        
    private:
        std::shared_ptr<ijoon::UDPSocket> socket;
        ijoon::Peer peer;
        ikcpcb *kcp;
        int connectionID;
    };
    
    class RendezvousSession: public BaseSession {
    public:
        RendezvousSession(std::shared_ptr<ijoon::UDPSocket> socket, uint connectionID): socket(socket), connectionID(connectionID) {
            
        }
        ~RendezvousSession() {}
        
    public:
        void setPrivateKcpPeer(std::string ip, std::string port,
                               int (*output)(const char *buf, int len, ikcpcb *kcp, void *user));
        void setPublicKcpPeer(std::string ip, std::string port,
                              int (*output)(const char *buf, int len, ikcpcb *kcp, void *user));
        void setRelayKcpPeer(std::string ip, std::string port,
                             int (*output)(const char *buf, int len, ikcpcb *kcp, void *user));
        void setPrivateKcpPeer(std::shared_ptr<ijoon::KcpPeer> kcpPeer);
        void setPublicKcpPeer(std::shared_ptr<ijoon::KcpPeer> kcpPeer);
        void setRelayKcpPeer(std::shared_ptr<ijoon::KcpPeer> kcpPeer);
        std::shared_ptr<KcpPeer> getPrivateKcpPeer();
        std::shared_ptr<KcpPeer> getPublicKcpPeer();
        std::shared_ptr<KcpPeer> getRelayKcpPeer();
        void clearPrivateKcpPeer();
        void clearPublicKcpPeer();
        void clearRelayKcpPeer();
        
        bool isPublic();
        bool isConnected();
        
        std::shared_ptr<ijoon::UDPSocket> getClientSocket()  {return this->socket;}
        uint getConnectionID() {return connectionID;}
        void setConnectionID(int connectionID) { this->connectionID = connectionID; }
        
        bool send(int packetType, char *message, unsigned int length) override;
        bool send(std::shared_ptr<google::protobuf::Message> message) override;
        
    public:
        time_t lastPing;
        
    private:
        std::shared_ptr<ijoon::UDPSocket> socket;
        uint connectionID;
        
        std::shared_ptr<ijoon::KcpPeer> privateKcpPeer;
        std::shared_ptr<ijoon::KcpPeer> publicKcpPeer;
        std::shared_ptr<ijoon::KcpPeer> relayKcpPeer;
    };
    
    class Session: public BaseSession {
    public:
        Session() : cs(std::shared_ptr<ijoon::TCPSocket>(new ijoon::TCPSocket())) {}
        Session(std::shared_ptr<ijoon::TCPSocket> cs): cs(cs) {}
        ~Session() = default;
        bool send(int packetType, char *message, unsigned int length) override;
        bool send(google::protobuf::Message *message) override;
        bool send(std::shared_ptr<google::protobuf::Message> message) override;
        
        bool recvHeader(MessageHeader &messageHeader) override;
        google::protobuf::Message *recvProtobufBody(MessageHeader &messageHeader) override;
        char *recvRawBody(MessageHeader &messageHeader) override;
        
        std::shared_ptr<ijoon::TCPSocket> getClientSocket() {return this->cs;}
        
    private:
        std::shared_ptr<ijoon::TCPSocket> cs;
    };
}

