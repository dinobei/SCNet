#pragma once
#include <iostream>
#include "message_header.h"

namespace ijoon {
    class Session {
    public:
        Session() : cs(std::shared_ptr<ijoon::TCPSocket>(new ijoon::TCPSocket())) {}
        Session(std::shared_ptr<ijoon::TCPSocket> cs): cs(cs) {}
        ~Session() {this->cs->close();};
        bool send(int packetType, char *message, unsigned int length);
        bool send(google::protobuf::Message *message);
        bool send(std::shared_ptr<google::protobuf::Message> message);
        bool recvHeader(MessageHeader &messageHeader);
        
        google::protobuf::Message *recvProtobufBody(MessageHeader &messageHeader);
        char *recvRawBody(MessageHeader &messageHeader);
        
        std::shared_ptr<ijoon::TCPSocket> getClientSocket() {return this->cs;}

    private:
        inline void makeHeader(char *buf, MessageHeader &messageHeader);
        std::shared_ptr<ijoon::TCPSocket> cs;
    };
}

