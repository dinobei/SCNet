#pragma once
#include <iostream>
#include "message_header.h"
#include "ikcp.h"

namespace ijoon {
    class BaseSession {
    public:
        virtual ~BaseSession() {}
        virtual bool send(ijoon::MessageHeader& messageHeader, char *message = nullptr, unsigned int length = 0) {return false;} // for relay packet
        virtual bool send(int packetType, char *message = nullptr, unsigned int length = 0) {return false;}
        virtual bool send(google::protobuf::Message *message) {return false;}
        virtual bool send(std::shared_ptr<google::protobuf::Message> message) {return false;}
        
        virtual bool recvHeader(MessageHeader &messageHeader) {return false;}
        virtual google::protobuf::Message *recvProtobufBody(MessageHeader &messageHeader) {return nullptr;}
        virtual char *recvRawBody(MessageHeader &messageHeader) {return nullptr;};
    };
    
    enum class PeerType {
        NONE = 0,
        RENDEZVOUS_SERVER = 1,
        RENDEZVOUS_CLIENT = 2,
        RELAY_SERVER = 3,
    };

    enum class PeerStatus {
        NONE = 0,
        REGISTERED = 1,
        COMPATIBLE = 2,
        NOT_COMPATIBLE = 3,
        UNREGISTERED = 4,
    };
    
    class KcpPeer: public BaseSession {
    public:
        KcpPeer(std::shared_ptr<ijoon::UDPSocket> socket, std::string ip, std::string port,
                int (*output)(const char *buf, int len, ikcpcb *kcp, void *user)): type(PeerType::NONE), status(PeerStatus::NONE), socket(socket) {
            // setup peer object
            peer.setIP(ip);
            peer.setPort(port);
            
            // setup kcp object
            kcp = ikcp_create(0x11223344, (void *)this);
            kcp->output = output;
            
            ikcp_wndsize(kcp, 128, 128);
            ikcp_nodelay(kcp, 1, 20, 2, 1);
            
            next = 0;
            
            lastPing = ijoon::ComputableTime::getCurrentTimeSec();
        }
        ~KcpPeer() {
            ikcp_release(kcp);
        }
        bool send(ijoon::MessageHeader& messageHeader, char *message = nullptr, unsigned int length = 0) override;
        bool send(int packetType, char *message = nullptr, unsigned int length = 0) override;
        bool send(google::protobuf::Message *message) override;
        bool send(std::shared_ptr<google::protobuf::Message> message) override;
        int getSendBufSize();
        
        std::shared_ptr<ijoon::UDPSocket> getClientSocket()  {return this->socket;}
        ijoon::Peer getPeer() { return peer; }
        ikcpcb *getKcp() { return kcp; }
        
    public:
        ijoon::Mutex mutex;
        IUINT32 next;
        time_t lastPing;
        PeerType type;
        PeerStatus status;
        uint connectionID;
        
    private:
        std::shared_ptr<ijoon::UDPSocket> socket;
        ijoon::Peer peer;
        ikcpcb *kcp;
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

