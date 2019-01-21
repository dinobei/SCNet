#pragma once
#include <iostream>
#include "message_header.h"

namespace ijoon {
    class Session {
    public:
        Session() : cs(std::shared_ptr<ijoon::ClientTCPSocket>(new ijoon::ClientTCPSocket())) {}
        Session(std::shared_ptr<ijoon::ClientTCPSocket> cs): cs(cs) {}
        ~Session() {this->cs->close();};
        bool send(int packetType, char *message, unsigned int length);
        bool send(google::protobuf::Message *message);
        bool send(std::shared_ptr<google::protobuf::Message> message);
        bool recvHeader(MessageHeader &messageHeader);
        
        google::protobuf::Message *recvProtobufBody(MessageHeader &messageHeader);
        char *recvRawBody(MessageHeader &messageHeader);
        
        std::shared_ptr<ijoon::ClientTCPSocket> getClientSocket() {return this->cs;}

    private:
        std::shared_ptr<ijoon::ClientTCPSocket> cs;
        inline void makeHeader(char *buf, MessageHeader &messageHeader);
    };
}

