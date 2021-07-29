#include "client.h"
#include "registry.h"

/* implemented proto messages */
#include "packet_type.pb.h"
#include "example.pb.h"
using namespace example;

void onPing(std::shared_ptr<scnet::Session> sess, scnet::Header *header, Ping *ping) {
    std::cout << "Received Ping from server" << std::endl;
    sess->send(ping, nullptr);
}

void onAttaching(std::shared_ptr<scnet::Session> sess);
void attachFailed(std::shared_ptr<scnet::Session> sess);
void attached(std::shared_ptr<scnet::Session> sess);
void detached(std::shared_ptr<scnet::Session> sess);
void detach(std::shared_ptr<scnet::Session> sess);
void timeout(std::shared_ptr<scnet::Session> sess);

std::shared_ptr<scnet::Session> _g_sess = nullptr;

int main(int argv, char **argc)
{
    signal(SIGPIPE, SIG_IGN);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(DummyPacket1, [](std::shared_ptr<scnet::Session> session, scnet::Header *header, DummyPacket1 *pkt1) {
        std::cout << "[Callback] DummyPacket1 received, ";
        std::cout << "title: " << pkt1->title() << ", number: " << pkt1->number() << std::endl;
    });
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(DummyPacket2, [](std::shared_ptr<scnet::Session> session, scnet::Header *header, DummyPacket2 *pkt2) {
        std::cout << "[Callback] DummyPacket2 received, ";
        std::cout << "array size: " << pkt2->strarr_size() << ", message: ";
        for(int i = 0 ; i < pkt2->strarr_size() ; i++) {
            std::cout << pkt2->strarr(i) << " ";
        }
        std::cout << std::endl;
    });
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(Ping, onPing);

    scnet::Client client("127.0.0.1", 9190, 1000);
    client.onAttaching = onAttaching;
    client.onAttachFailed = attachFailed;
    client.onAttached = attached;
    client.onDetached = detached;
    client.onDetach = detach;
    client.onTimeout = timeout;
    client.attach();

    const int interval_ms = 1000;
    
    __msleep(interval_ms);
    auto packet1 = std::shared_ptr<DummyPacket1>(new DummyPacket1());
    packet1->set_title("hello world");
    packet1->set_number(123);
    if(_g_sess != nullptr) {
        _g_sess->send(packet1.get(),
                       [](std::shared_ptr<scnet::Session> sess, scnet::Header *header, google::protobuf::Message *message){
                           auto pkt1 = dynamic_cast<example::DummyPacket1 *>(message);
                           std::cout << "[Dedicated Callback] DummyPacket1 received, title: " << pkt1->title() << ", number: " << pkt1->number() << std::endl;
                       },
                      []() { std::cout << "Packet1::onTimeout" << std::endl; },
                      []() { std::cout << "Packet1::onEnded" << std::endl; });
    }
    
    __msleep(interval_ms);
    auto packet2 = std::shared_ptr<DummyPacket2>(new DummyPacket2());
    packet2->add_strarr("A");
    packet2->add_strarr("B");
    packet2->add_strarr("C");
    if(_g_sess != nullptr) _g_sess->send(packet2.get(),
         [](std::shared_ptr<scnet::Session> sess, scnet::Header *header, google::protobuf::Message *message){
            auto packet2 = dynamic_cast<example::DummyPacket2 *>(message);
            std::cout << "[Dedicated Callback] DummyPacket2 received, array size: " << packet2->strarr_size() << ", message: ";
            for(int i = 0 ; i < packet2->strarr_size() ; i++) {
                std::cout << packet2->strarr(i) << " ";
            }
            std::cout << std::endl;
        },
        []() { std::cout << "Packet2::onTimeout" << std::endl; },
        []() { std::cout << "Packet2::onEnded" << std::endl; });
    
    client.detach();
    __msleep(30*1000);
    return 0;
}

void onAttaching(std::shared_ptr<scnet::Session> sess)
{
    std::cout << "[" << sess->getClientSocket()->get_socket_identifier() << "] attaching" << std::endl;
}

void attachFailed(std::shared_ptr<scnet::Session> sess)
{
    std::cout << "[" << sess->getClientSocket()->get_socket_identifier() << "] attachFailed" << std::endl;
}

void attached(std::shared_ptr<scnet::Session> sess)
{
    std::cout << "[" << sess->getClientSocket()->get_socket_identifier() << "] attached" << std::endl;
    _g_sess = sess;
}

void detached(std::shared_ptr<scnet::Session> sess)
{
    std::cout << "[" << sess->getClientSocket()->get_socket_identifier() << "] detached" << std::endl;
    _g_sess = nullptr;
}

void detach(std::shared_ptr<scnet::Session> sess)
{
    std::cout << "[" << sess->getClientSocket()->get_socket_identifier() << "] detach" << std::endl;
}

void timeout(std::shared_ptr<scnet::Session> sess)
{
    std::cout << "[" << sess->getClientSocket()->get_socket_identifier() << "] timeout" << std::endl;
}
