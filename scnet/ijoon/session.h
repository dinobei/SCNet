#pragma once
#include <iostream>
#include "message_header.h"

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
    
    class RendezvousSession: public BaseSession {
    public:
        RendezvousSession(std::shared_ptr<ijoon::UDPSocket> socket, uint connectionID): socket(socket), connectionID(connectionID), isSetPrivatePeer(false), isSetPublicPeer(false), isSetRelayPeer(false) {}
        ~RendezvousSession() {}
        
        bool send(int packetType, char *message, unsigned int length) override;
        bool send(google::protobuf::Message *message) override;
        bool send(std::shared_ptr<google::protobuf::Message> message) override;
        
        std::shared_ptr<ijoon::UDPSocket> getClientSocket()  {return this->socket;}
        
        void setPrivatePeer(std::string ip, std::string port);
        void setPublicPeer(std::string ip, std::string port);
        void setRelayPeer(std::string ip, std::string port);
        
        void clearPrivatePeer();
        void clearPublicPeer();
        void clearRelayPeer();
        
        uint getConnectionID() {return connectionID;}
        
    private:
        std::shared_ptr<ijoon::UDPSocket> socket;
        uint connectionID;
        
        bool isSetPrivatePeer;
        bool isSetPublicPeer;
        bool isSetRelayPeer;
        ijoon::Peer privatePeer;
        ijoon::Peer publicPeer;
        ijoon::Peer relayPeer;
        
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

