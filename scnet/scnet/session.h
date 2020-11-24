#pragma once
#include <iostream>
#include <chrono>
#include <functional>

#include "message_header.h"
#include "header.pb.h"

namespace scnet {
using namespace std::chrono;

    class Session;
    class BaseSession {
    public:
        virtual ~BaseSession() {}
        virtual bool send(google::protobuf::Message *message, std::function<void(std::shared_ptr<Session>, scnet::Header *, google::protobuf::Message *)> cb) {return false;}
        virtual bool send(scnet::Header *_header, google::protobuf::Message *message, std::function<void(std::shared_ptr<Session>, scnet::Header *, google::protobuf::Message *)> cb) {return false;}
        
        virtual bool recv() {return false;}

        system_clock::time_point getPing() { return this->ping; }
        void updatePing() {
            this->ping = system_clock::now();
        }
        
    private:
        system_clock::time_point ping;
    };

    class Session: public BaseSession {
    public:
        Session() : cs(std::shared_ptr<cppsocket::tcp_socket>(new cppsocket::tcp_socket())) {}
        Session(std::shared_ptr<cppsocket::tcp_socket> cs): cs(cs) {}
        ~Session() = default;
        bool send(google::protobuf::Message *message, std::function<void(std::shared_ptr<Session>, scnet::Header *, google::protobuf::Message *)> cb = nullptr) override;
        bool send(scnet::Header *_header, google::protobuf::Message *message, std::function<void(std::shared_ptr<Session>, scnet::Header *, google::protobuf::Message *)> cb = nullptr) override;
        bool recv() override;

        std::shared_ptr<cppsocket::tcp_socket> getClientSocket() {return this->cs;}

    private:
        std::shared_ptr<cppsocket::tcp_socket> cs;
        std::mutex snd_mtx;
        std::mutex rcv_mtx;
    };
}

