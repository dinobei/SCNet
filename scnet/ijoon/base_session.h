#pragma once
#include <iostream>
#include "message_header.h"

namespace ijoon {
    class BaseSession {
    public:
        BaseSession() : cs(std::shared_ptr<ijoon::JClientSocket>(new ijoon::JClientSocket(ijoon::tcp))) {}
        BaseSession(std::shared_ptr<ijoon::JClientSocket> cs): cs(cs) {}
        ~BaseSession() {this->cs->close();};
        bool send(google::protobuf::Message *message);
        bool send(std::shared_ptr<google::protobuf::Message> message);
        bool recvHeader(MessageHeader &messageHeader);
        google::protobuf::Message *recvBody(MessageHeader &messageHeader);
        inline int event(int timeMs) {return this->cs->event(timeMs);}
        void startThread(FuncPointer func, std::string name, void *param);
        std::shared_ptr<ijoon::JClientSocket> getClientSocket() {return this->cs;}

    private:
        ijoon::Thread *thread;
        std::shared_ptr<ijoon::JClientSocket> cs;
        inline void makeHeader(char *buf, MessageHeader &messageHeader);
    };
}

