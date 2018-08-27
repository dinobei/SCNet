#pragma once
#include <iostream>
#include "message_header.h"

namespace ijoon {
    class BaseSession {
    public:
        BaseSession(std::shared_ptr<ijoon::JClientSocket> cs) {this->cs = cs;}
        ~BaseSession() {this->cs->close();};
        bool send(google::protobuf::Message *message);
        bool send(std::shared_ptr<google::protobuf::Message> message);
        google::protobuf::Message *recv();
        inline int event(int timeMs) {return this->cs->event(timeMs);}
        void startThread(FuncPointer func, std::string name, void *param);

    private:
        ijoon::Thread *thread;
        std::shared_ptr<ijoon::JClientSocket> cs;
        inline MessageHeader makeHeader(char *buf);
    };
}

