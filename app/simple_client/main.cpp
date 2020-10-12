#include <ijoon/coreutils.h>
#include "server.h"
#include "registry.h"

/* implemented proto messages */
#include "packet.pb.h"
#include "packet_type.pb.h"
#include "get_image.pb.h"
using namespace example;

void onPacket1(std::shared_ptr<ijoon::Session> sess, scnet::Header *header, Packet1 *pkt1);
void onPacket2(std::shared_ptr<ijoon::Session> sess, scnet::Header *header, Packet2 *pkt2);
void onPacket3(std::shared_ptr<ijoon::Session> sess, scnet::Header *header, Packet3 *pkt3);
void onPacket4(std::shared_ptr<ijoon::Session> sess, scnet::Header *header, Packet4 *pkt4);
void onArrayMessage(std::shared_ptr<ijoon::Session> sess, scnet::Header *header, ArrayMessage *arrayMessage);
void onImageResponse(std::shared_ptr<ijoon::Session> sess, scnet::Header *header, ImageResponse *imageResponse);

void onAttaching(std::shared_ptr<ijoon::Session> sess);
void attachFailed(std::shared_ptr<ijoon::Session> sess);
void attached(std::shared_ptr<ijoon::Session> sess);
void detached(std::shared_ptr<ijoon::Session> sess);
void detach(std::shared_ptr<ijoon::Session> sess);
void timeout(std::shared_ptr<ijoon::Session> sess);

std::shared_ptr<ijoon::Session> _g_sess = nullptr;

int main(int argv, char** argc)
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
    
    ijoon::Server server("127.0.0.1", 9190, 1000);
    server.onAttaching = onAttaching;
    server.onAttachFailed = attachFailed;
    server.onAttached = attached;
    server.onDetached = detached;
    server.onDetach = detach;
    server.onTimeout = timeout;
    server.attach();

    const int interval_ms = 33;
    int cnt = 300;
    while(cnt--) {
        scnet::Header header;
        ijn_msleep(interval_ms);
        auto packet1 = std::shared_ptr<Packet1>(new Packet1());
        packet1->set_number(11);
        if(_g_sess != nullptr) {
            _g_sess->send(packet1.get(),
                           [](std::shared_ptr<ijoon::Session> sess, scnet::Header *header, google::protobuf::Message *message){
                               auto pkt1 = dynamic_cast<example::Packet1 *>(message);
                               std::cout << "Packet1 received, number=" << pkt1->number() << std::endl;
                           });
        }

        ijn_msleep(interval_ms);
        auto packet2 = std::shared_ptr<Packet2>(new Packet2());
        packet2->set_str("this is sample string");
        if(_g_sess != nullptr) _g_sess->send(packet2.get(), [](std::shared_ptr<ijoon::Session> sess, scnet::Header *header, google::protobuf::Message *message){
            auto pkt2 = dynamic_cast<example::Packet2 *>(message);
            std::cout << "Packet2 received, str=" << pkt2->str() << std::endl;
        });

        ijn_msleep(interval_ms);
        auto packet3 = std::shared_ptr<Packet3>(new Packet3());
        packet3->set_boolvalue(true);
        if(_g_sess != nullptr) _g_sess->send(packet3.get(), [](std::shared_ptr<ijoon::Session> sess, scnet::Header *header, google::protobuf::Message *message){
            auto pkt3 = dynamic_cast<example::Packet3 *>(message);
            std::cout << "Packet3 received, boolVal=" << pkt3->boolvalue() << std::endl;
        });

        ijn_msleep(interval_ms);
        auto packet4 = std::shared_ptr<Packet4>(new Packet4());
        packet4->set_doublevalue(5000.123);
        packet4->set_floatvalue(123.4f);
        if(_g_sess != nullptr) _g_sess->send(packet4.get(), [](std::shared_ptr<ijoon::Session> sess, scnet::Header *header, google::protobuf::Message *message){
            auto pkt4 = dynamic_cast<example::Packet4 *>(message);
            std::cout << "Packet4 received, floatVal=" << pkt4->floatvalue() << "doubleVal=" << pkt4->doublevalue() << std::endl;
        });
        
        ijn_msleep(interval_ms);
        auto arrayMessage = std::shared_ptr<ArrayMessage>(new ArrayMessage());
        arrayMessage->add_strarr("this");
        arrayMessage->add_strarr("is");
        arrayMessage->add_strarr("SCNet");
        arrayMessage->add_strarr("example");
        if(_g_sess != nullptr) _g_sess->send(arrayMessage.get(), [](std::shared_ptr<ijoon::Session> sess, scnet::Header *header, google::protobuf::Message *message){
            auto arrayMessage = dynamic_cast<example::ArrayMessage *>(message);
            std::cout << "ArrayMessage received, arr: ";
            for(int i = 0 ; i < arrayMessage->strarr_size() ; i++) {
                std::cout << arrayMessage->strarr(i) << " ";
            }
            std::cout << std::endl;
        });
        
        ijn_msleep(interval_ms);
        auto imageRequest = std::shared_ptr<ImageRequest>(new ImageRequest());
        imageRequest->set_name("hello.jpg");
        if(_g_sess != nullptr) _g_sess->send(imageRequest.get(), [](std::shared_ptr<ijoon::Session> sess, scnet::Header *header, google::protobuf::Message *message){
            auto imageResponse = dynamic_cast<example::ImageResponse *>(message);
            const char *imageBuffer = imageResponse->imagebuffer().c_str();
            ImageHeader imageHeader = imageResponse->header();
            std::cout << "imageResponse received, name=" << imageHeader.name() << ", width=" << imageHeader.width() << ", height=" << imageHeader.height() << ", size=" << imageHeader.size() << std::endl;
        });
    }

    ijn_print(DP_INFO, "Press Enter to detach");
    getchar();

    server.detach();
    ijn_print(DP_INFO, "Press enter to quit");
    getchar();
    return 0;
}

void onPacket1(std::shared_ptr<ijoon::Session> sess, scnet::Header *header, Packet1 *pkt1) {
    ijn_print(DP_DEBUG, "[onPacket1()] number=%d", pkt1->number());
}

void onPacket2(std::shared_ptr<ijoon::Session> sess, scnet::Header *header, Packet2 *pkt2) {
    ijn_print(DP_DEBUG, "[onPacket2()] str=%s", pkt2->str().c_str());
}

void onPacket3(std::shared_ptr<ijoon::Session> sess, scnet::Header *header, Packet3 *pkt3) {
    ijn_print(DP_DEBUG, "[onPacket3()] boolvalue=%s", pkt3->boolvalue()?"true":"false");
}

void onPacket4(std::shared_ptr<ijoon::Session> sess, scnet::Header *header, Packet4 *pkt4) {
    ijn_print(DP_DEBUG, "[onPacket4()] floatvalue=%f, doublevalue=%lf", pkt4->floatvalue(), pkt4->doublevalue());
}

void onArrayMessage(std::shared_ptr<ijoon::Session> sess, scnet::Header *header, ArrayMessage *arrayMessage) {
    ijn_print(DP_INFO, "[onArrayMessage()] received array size: %d, message: ", arrayMessage->strarr_size());
    for(int i = 0 ; i < arrayMessage->strarr_size() ; i++) {
        printf("%s ", arrayMessage->strarr(i).c_str());
    }
    printf("\n");
}

void onImageResponse(std::shared_ptr<ijoon::Session> sess, scnet::Header *header, ImageResponse *imageResponse) {
    const char *imageBuffer = imageResponse->imagebuffer().c_str();
    ImageHeader imageHeader = imageResponse->header();
    
    ijn_print(DP_DEBUG, "[onImageResponse()] imageResponse received, name=%s, width=%d, height=%d, size=%d", imageHeader.name().c_str(), imageHeader.width(), imageHeader.height(), imageHeader.size());
}

void onAttaching(std::shared_ptr<ijoon::Session> sess) {
    ijn_print(DP_INFO, "[%d] attaching", sess->getClientSocket()->getSocketIdentifier());
}

void attachFailed(std::shared_ptr<ijoon::Session> sess) {
    ijn_print(DP_INFO, "[%d] attachFailed", sess->getClientSocket()->getSocketIdentifier());
}

void attached(std::shared_ptr<ijoon::Session> sess) {
    ijn_print(DP_INFO, "[%d] attached", sess->getClientSocket()->getSocketIdentifier());
    _g_sess = sess;
}

void detached(std::shared_ptr<ijoon::Session> sess) {
    ijn_print(DP_INFO, "[%d] detached", sess->getClientSocket()->getSocketIdentifier());
    _g_sess = nullptr;
}

void detach(std::shared_ptr<ijoon::Session> sess) {
    ijn_print(DP_INFO, "[%d] detach", sess->getClientSocket()->getSocketIdentifier());
}

void timeout(std::shared_ptr<ijoon::Session> sess) {
    ijn_print(DP_INFO, "[%d] timeout", sess->getClientSocket()->getSocketIdentifier());
    auto ping = scnet::Ping();
    sess->send(&ping, nullptr);
}
