#include "client.h"
#include "registry.h"

/* implemented proto messages */
#include "packet.pb.h"
#include "packet_type.pb.h"
#include "get_image.pb.h"
using namespace example;

void onPacket1(std::shared_ptr<scnet::Session> sess, scnet::Header *header, Packet1 *pkt1);
void onPacket2(std::shared_ptr<scnet::Session> sess, scnet::Header *header, Packet2 *pkt2);
void onPacket3(std::shared_ptr<scnet::Session> sess, scnet::Header *header, Packet3 *pkt3);
void onPacket4(std::shared_ptr<scnet::Session> sess, scnet::Header *header, Packet4 *pkt4);
void onArrayMessage(std::shared_ptr<scnet::Session> sess, scnet::Header *header, ArrayMessage *arrayMessage);
void onImageResponse(std::shared_ptr<scnet::Session> sess, scnet::Header *header, ImageResponse *imageResponse);

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
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(packetType1, Packet1, onPacket1);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(packetType2, Packet2, onPacket2);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(packetType3, Packet3, onPacket3);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(packetType4, Packet4, onPacket4);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(arrayMessageType, ArrayMessage, onArrayMessage);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(imageRequest, ImageRequest, nullptr);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(imageResponse, ImageResponse, onImageResponse);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(9, scnet::Ping, nullptr);

    scnet::Client client("127.0.0.1", 9190, 1000);
    client.onAttaching = onAttaching;
    client.onAttachFailed = attachFailed;
    client.onAttached = attached;
    client.onDetached = detached;
    client.onDetach = detach;
    client.onTimeout = timeout;
    client.attach();

    const int interval_ms = 33;
    int cnt = 300;
    while(cnt--) {
        scnet::Header header;
        __msleep(interval_ms);
        auto packet1 = std::shared_ptr<Packet1>(new Packet1());
        packet1->set_number(11);
        if(_g_sess != nullptr) {
            _g_sess->send(packet1.get(),
                           [](std::shared_ptr<scnet::Session> sess, scnet::Header *header, google::protobuf::Message *message){
                               auto pkt1 = dynamic_cast<example::Packet1 *>(message);
                               std::cout << "Packet1 received, number=" << pkt1->number() << std::endl;
                           });
        }

        __msleep(interval_ms);
        auto packet2 = std::shared_ptr<Packet2>(new Packet2());
        packet2->set_str("this is sample string");
        if(_g_sess != nullptr) _g_sess->send(packet2.get(), [](std::shared_ptr<scnet::Session> sess, scnet::Header *header, google::protobuf::Message *message){
            auto pkt2 = dynamic_cast<example::Packet2 *>(message);
            std::cout << "Packet2 received, str=" << pkt2->str() << std::endl;
        });

        __msleep(interval_ms);
        auto packet3 = std::shared_ptr<Packet3>(new Packet3());
        packet3->set_boolvalue(true);
        if(_g_sess != nullptr) _g_sess->send(packet3.get(), [](std::shared_ptr<scnet::Session> sess, scnet::Header *header, google::protobuf::Message *message){
            auto pkt3 = dynamic_cast<example::Packet3 *>(message);
            std::cout << "Packet3 received, boolVal=" << pkt3->boolvalue() << std::endl;
        });

        __msleep(interval_ms);
        auto packet4 = std::shared_ptr<Packet4>(new Packet4());
        packet4->set_doublevalue(5000.123);
        packet4->set_floatvalue(123.4f);
        if(_g_sess != nullptr) _g_sess->send(packet4.get(), [](std::shared_ptr<scnet::Session> sess, scnet::Header *header, google::protobuf::Message *message){
            auto pkt4 = dynamic_cast<example::Packet4 *>(message);
            std::cout << "Packet4 received, floatVal=" << pkt4->floatvalue() << "doubleVal=" << pkt4->doublevalue() << std::endl;
        });
        
        __msleep(interval_ms);
        auto arrayMessage = std::shared_ptr<ArrayMessage>(new ArrayMessage());
        arrayMessage->add_strarr("this");
        arrayMessage->add_strarr("is");
        arrayMessage->add_strarr("SCNet");
        arrayMessage->add_strarr("example");
        if(_g_sess != nullptr) _g_sess->send(arrayMessage.get(), [](std::shared_ptr<scnet::Session> sess, scnet::Header *header, google::protobuf::Message *message){
            auto arrayMessage = dynamic_cast<example::ArrayMessage *>(message);
            std::cout << "ArrayMessage received, arr: ";
            for(int i = 0 ; i < arrayMessage->strarr_size() ; i++) {
                std::cout << arrayMessage->strarr(i) << " ";
            }
            std::cout << std::endl;
        });
        
        __msleep(interval_ms);
        auto imageRequest = std::shared_ptr<ImageRequest>(new ImageRequest());
        imageRequest->set_name("hello.jpg");
        if(_g_sess != nullptr) _g_sess->send(imageRequest.get(), [](std::shared_ptr<scnet::Session> sess, scnet::Header *header, google::protobuf::Message *message){
            auto imageResponse = dynamic_cast<example::ImageResponse *>(message);
            const char *imageBuffer = imageResponse->imagebuffer().c_str();
            ImageHeader imageHeader = imageResponse->header();
            std::cout << "imageResponse received, name=" << imageHeader.name() << ", width=" << imageHeader.width() << ", height=" << imageHeader.height() << ", size=" << imageHeader.size() << std::endl;
        });
    }

    std::cout << "Press Enter to detach" << std::endl;
    getchar();

    client.detach();
    std::cout << "Press enter to quit" << std::endl;
    getchar();
    return 0;
}

void onPacket1(std::shared_ptr<scnet::Session> sess, scnet::Header *header, Packet1 *pkt1)
{
    std::cout << "[onPacket1()] number=" << pkt1->number() << std::endl;
}

void onPacket2(std::shared_ptr<scnet::Session> sess, scnet::Header *header, Packet2 *pkt2)
{
    std::cout << "[onPacket2()] str=" << pkt2->str() << std::endl;
}

void onPacket3(std::shared_ptr<scnet::Session> sess, scnet::Header *header, Packet3 *pkt3)
{
    std::cout << "[onPacket3()] boolvalue=" << (pkt3->boolvalue() ? "true" : "false") << std::endl;
}

void onPacket4(std::shared_ptr<scnet::Session> sess, scnet::Header *header, Packet4 *pkt4)
{
    std::cout << "[onPacket4()] floatvalue=" << pkt4->floatvalue() << ", doublevalue=" << pkt4->doublevalue() << std::endl;
}

void onArrayMessage(std::shared_ptr<scnet::Session> sess, scnet::Header *header, ArrayMessage *arrayMessage)
{
    std::cout << "[onArrayMessage()] received array size=" << arrayMessage->strarr_size() << ", message: ";
    for (int i = 0; i < arrayMessage->strarr_size(); i++)
    {
        std::cout << arrayMessage->strarr(i) << " ";
    }
    std::cout << std::endl;
}

void onImageResponse(std::shared_ptr<scnet::Session> sess, scnet::Header *header, ImageResponse *imageResponse)
{
    const char *imageBuffer = imageResponse->imagebuffer().c_str();
    ImageHeader imageHeader = imageResponse->header();

    std::cout << "[onImageResponse()] imageResponse received, name=" << imageHeader.name() << ", width=" << imageHeader.width() << ", height=" << imageHeader.height() << ", size=" << imageHeader.size() << std::endl;
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
    auto ping = scnet::Ping();
    sess->send(&ping, nullptr);
}
