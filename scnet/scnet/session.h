#pragma once
#include <iostream>
#include <chrono>
#include <functional>

#include "message_header.h"
#include "header.pb.h"

namespace scnet {
using namespace std::chrono;

    struct CallbackContext;
    class Session;
    typedef std::function<void(std::shared_ptr<Session>, scnet::Header *, google::protobuf::Message *)> DedicatedCallback;
    class BaseSession {
    public:
        virtual ~BaseSession() {}
        virtual bool send(scnet::Header *_header,
                          google::protobuf::Message *message,
                          DedicatedCallback cb,
                          std::function<void()> cbTimeout,
                          std::function<void()> sessionEnded) {return false;}
        virtual bool send(google::protobuf::Message *message,
                          DedicatedCallback cb,
                          std::function<void()> cbTimeout,
                          std::function<void()> sessionEnded) {return false;}
        
        virtual bool recv() {return false;}

        system_clock::time_point getPing() { return this->ping; }
        void updatePing() {
            this->ping = system_clock::now();
        }
        
    private:
        system_clock::time_point ping;
    protected:
        struct CallbackContext {
            DedicatedCallback onReceived;
            std::function<void()> onTimeout;
            std::function<void()> onEnded;
            time_t reqUts;
            time_t resUts;
            time_t timeout;
            time_t ctxTimeout;
        };
        std::map<int, CallbackContext> cbCtxMap;
    };

    class Session: public BaseSession {
    public:
        Session() : cs(std::shared_ptr<cppsocket::tcp_socket>(new cppsocket::tcp_socket())) {}
        Session(std::shared_ptr<cppsocket::tcp_socket> cs): cs(cs) {}
        ~Session() = default;
        bool send(scnet::Header *_header,
                  google::protobuf::Message *message,
                  DedicatedCallback cb = nullptr,
                  std::function<void()> cbTimeout = nullptr,
                  std::function<void()> sessionEnded = nullptr) override;
        bool send(google::protobuf::Message *message,
                  DedicatedCallback cb = nullptr,
                  std::function<void()> cbTimeout = nullptr,
                  std::function<void()> sessionEnded = nullptr) override;
        bool recv() override;

        std::shared_ptr<cppsocket::tcp_socket> getClientSocket() {return this->cs;}
        void updateDedicatedCallbacks();

    private:
        std::shared_ptr<cppsocket::tcp_socket> cs;
        std::mutex snd_mtx;
        std::mutex rcv_mtx;
    };
}

