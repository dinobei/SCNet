#pragma once
#include <iostream>
#include "message_header.h"
#include "header.pb.h"

namespace ijoon {
    class Session;
    class BaseSession {
    public:
        virtual ~BaseSession() {}
        virtual bool send(google::protobuf::Message *message, std::function<void(std::shared_ptr<Session>, scnet::Header *, google::protobuf::Message *)> cb) {return false;} // for test
        virtual bool send(scnet::Header *_header, google::protobuf::Message *message, std::function<void(std::shared_ptr<Session>, scnet::Header *, google::protobuf::Message *)> cb) {return false;} // for test
        
        virtual bool recv() {return false;} // for test

        time_t getPing() { return ping; }
        void setPing(time_t ping) { this->ping = ping; }
        
    private:
        time_t ping;
    };

    class Session: public BaseSession {
    public:
        Session() : cs(std::shared_ptr<ijoon::TCPSocket>(new ijoon::TCPSocket())) {}
        Session(std::shared_ptr<ijoon::TCPSocket> cs): cs(cs) {}
        ~Session() = default;
        bool send(google::protobuf::Message *message, std::function<void(std::shared_ptr<Session>, scnet::Header *, google::protobuf::Message *)> cb) override;
        bool send(scnet::Header *_header, google::protobuf::Message *message, std::function<void(std::shared_ptr<Session>, scnet::Header *, google::protobuf::Message *)> cb = nullptr) override;
        bool recv() override;

        std::shared_ptr<ijoon::TCPSocket> getClientSocket() {return this->cs;}

    private:
        std::shared_ptr<ijoon::TCPSocket> cs;
        std::mutex snd_mtx;
        std::mutex rcv_mtx;
    };
}

