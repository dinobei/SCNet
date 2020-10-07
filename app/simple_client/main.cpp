#include <ijoon/coreutils.h>
#include "server.h"
#include "registry.h"

/* implemented proto messages */
#include "packet.pb.h"
#include "packet_type.pb.h"
#include "get_image.pb.h"
using namespace example;

void onPacket1(std::shared_ptr<ijoon::Session> sess, Packet1 *pkt1);
void onPacket2(std::shared_ptr<ijoon::Session> sess, Packet2 *pkt2);
void onPacket3(std::shared_ptr<ijoon::Session> sess, Packet3 *pkt3);
void onPacket4(std::shared_ptr<ijoon::Session> sess, Packet4 *pkt4);
void onArrayMessage(std::shared_ptr<ijoon::Session> sess, ArrayMessage *arrayMessage);
void onImageResponse(std::shared_ptr<ijoon::Session> sess, ImageResponse *imageResponse);
void onRawByteArray(std::shared_ptr<ijoon::Session> session, void *buffer, unsigned int length);
void onRawByteArray2(std::shared_ptr<ijoon::Session> session, void *buffer, unsigned int length);

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
    SCNET_RAW_MESSAGE_REGISTRATION(0, onRawByteArray);
    SCNET_RAW_MESSAGE_REGISTRATION(1, onRawByteArray2);
    
    ijoon::Server server("127.0.0.1", 9190, 1000);
    server.onAttaching = onAttaching;
    server.onAttachFailed = attachFailed;
    server.onAttached = attached;
    server.onDetached = detached;
    server.onDetach = detach;
    server.onTimeout = timeout;
    server.attach();

    int cnt = 300;
    while(cnt--) {
//        ijn_msleep(33);
        ijn_msleep(1000);
        auto packet1 = std::shared_ptr<Packet1>(new Packet1());
        packet1->set_number(11);
//        server.control(packet1);
        if(_g_sess != nullptr) _g_sess->send2(packet1.get());

//        ijn_msleep(33);
        ijn_msleep(1000);
        auto packet2 = std::shared_ptr<Packet2>(new Packet2());
        packet2->set_str("this is sample string");
//        server.control(packet2);
        if(_g_sess != nullptr) _g_sess->send2(packet2.get());

//        ijn_msleep(33);
        ijn_msleep(1000);
        auto packet3 = std::shared_ptr<Packet3>(new Packet3());
        packet3->set_boolvalue(true);
//        server.control(packet3);
        if(_g_sess != nullptr) _g_sess->send2(packet3.get());

//        ijn_msleep(33);
        ijn_msleep(1000);
        auto packet4 = std::shared_ptr<Packet4>(new Packet4());
        packet4->set_doublevalue(5000.123);
        packet4->set_floatvalue(123.4f);
//        server.control(packet4);
        if(_g_sess != nullptr) _g_sess->send2(packet4.get());
        
//        ijn_msleep(33);
        ijn_msleep(1000);
        auto arrayMessage = std::shared_ptr<ArrayMessage>(new ArrayMessage());
        arrayMessage->add_strarr("this");
        arrayMessage->add_strarr("is");
        arrayMessage->add_strarr("SCNet");
        arrayMessage->add_strarr("example");
//        server.control(arrayMessage);
        if(_g_sess != nullptr) _g_sess->send2(arrayMessage.get());
        
//        ijn_msleep(33);
        ijn_msleep(1000);
        auto imageRequest = std::shared_ptr<ImageRequest>(new ImageRequest());
        imageRequest->set_name("hello.jpg");
//        server.control(imageRequest);
        if(_g_sess != nullptr) _g_sess->send2(imageRequest.get());
        
//        ijn_msleep(33);
//        char rawMessage[255] = "hello world";
//        server.control(0, rawMessage, strlen(rawMessage));
//
//        ijn_msleep(33);
//        sprintf(rawMessage, "next world");
//        server.control(1, rawMessage, strlen(rawMessage));
    }

    ijn_sleep(1);
    ijn_print(DP_INFO, "Press Enter to detach");
    getchar();

    server.detach();
    ijn_print(DP_INFO, "Press enter to quit");
    getchar();
    return 0;
}

void onPacket1(std::shared_ptr<ijoon::Session> sess, Packet1 *pkt1) {
    ijn_print(DP_DEBUG, "[onPacket1()] number=%d", pkt1->number());
}

void onPacket2(std::shared_ptr<ijoon::Session> sess, Packet2 *pkt2) {
    ijn_print(DP_DEBUG, "[onPacket2()] str=%s", pkt2->str().c_str());
}

void onPacket3(std::shared_ptr<ijoon::Session> sess, Packet3 *pkt3) {
    ijn_print(DP_DEBUG, "[onPacket3()] boolvalue=%s", pkt3->boolvalue()?"true":"false");
}

void onPacket4(std::shared_ptr<ijoon::Session> sess, Packet4 *pkt4) {
    ijn_print(DP_DEBUG, "[onPacket4()] floatvalue=%f, doublevalue=%lf", pkt4->floatvalue(), pkt4->doublevalue());
}

void onArrayMessage(std::shared_ptr<ijoon::Session> sess, ArrayMessage *arrayMessage) {
    ijn_print(DP_INFO, "[onArrayMessage()] received array size: %d, message: ", arrayMessage->strarr_size());
    for(int i = 0 ; i < arrayMessage->strarr_size() ; i++) {
        printf("%s ", arrayMessage->strarr(i).c_str());
    }
    printf("\n");
}

void onImageResponse(std::shared_ptr<ijoon::Session> sess, ImageResponse *imageResponse) {
    const char *imageBuffer = imageResponse->imagebuffer().c_str();
    ImageHeader header = imageResponse->header();
    
    ijn_print(DP_DEBUG, "[onImageResponse()] imageResponse received, name=%s, width=%d, height=%d, size=%d", header.name().c_str(), header.width(), header.height(), header.size());
}

void onRawByteArray(std::shared_ptr<ijoon::Session> session, void *buffer, unsigned int length)
{
    char *message = nullptr;
    if(buffer != nullptr) {
        message = static_cast<char *>(buffer);
    }
    
    message[length] = '\0';
    ijn_print(DP_DEBUG, "onRawByteArray, length: %u %s", length, message);
}

void onRawByteArray2(std::shared_ptr<ijoon::Session> session, void *buffer, unsigned int length)
{
    char *message = nullptr;
    if(buffer != nullptr) {
        message = static_cast<char *>(buffer);
    }
    
    message[length] = '\0';
    ijn_print(DP_DEBUG, "onRawByteArray2, length: %u %s", length, message);
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
//    ijn_print(DP_INFO, "[%d] timeout", sess->getClientSocket()->getSocketIdentifier());
//    sess->send(0, nullptr, 0);
}
