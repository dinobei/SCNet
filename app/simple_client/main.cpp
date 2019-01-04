#include <ijoon/coreutils.h>
#include "server.h"
#include "registry.h"

/* implemented proto messages */
#include "packet.pb.h"
#include "packet_type.pb.h"
#include "get_image.pb.h"
using namespace example;

void onPacket1(ijoon::Server *server, Packet1 *pkt1);
void onPacket2(ijoon::Server *server, Packet2 *pkt2);
void onPacket3(ijoon::Server *server, Packet3 *pkt3);
void onPacket4(ijoon::Server *server, Packet4 *pkt4);
void onArrayMessage(ijoon::Server *server, ArrayMessage *arrayMessage);
void onImageResponse(ijoon::Server *server, ImageResponse *imageResponse);

void onAttaching(ijoon::Server *server);
void attachFailed(ijoon::Server *server);
void attached(ijoon::Server *server);
void detached(ijoon::Server *server);
void detach(ijoon::Server *server);

int main(int argv, char** argc)
{
    ijoon::initGlobalVariables();
    SCNET_MESSAGE_REGISTRATION_WITH_RECV_CALLBACK(example, PacketType::packetType1, Packet1, ijoon::Server, onPacket1);
    SCNET_MESSAGE_REGISTRATION_WITH_RECV_CALLBACK(example, PacketType::packetType2, Packet2, ijoon::Server, onPacket2);
    SCNET_MESSAGE_REGISTRATION_WITH_RECV_CALLBACK(example, PacketType::packetType3, Packet3, ijoon::Server, onPacket3);
    SCNET_MESSAGE_REGISTRATION_WITH_RECV_CALLBACK(example, PacketType::packetType4, Packet4, ijoon::Server, onPacket4);
    SCNET_MESSAGE_REGISTRATION_WITH_RECV_CALLBACK(example, PacketType::arrayMessageType, ArrayMessage, ijoon::Server, onArrayMessage);
    SCNET_MESSAGE_REGISTRATION(example, PacketType::imageRequest, ImageRequest);
    SCNET_MESSAGE_REGISTRATION_WITH_RECV_CALLBACK(example, PacketType::imageResponse, ImageResponse, ijoon::Server, onImageResponse);
    
    ijoon::Server server("127.0.0.1", 9190, 1000);
    server.onAttaching = onAttaching;
    server.onAttachFailed = attachFailed;
    server.onAttached = attached;
    server.onDetached = detached;
    server.onDetach = detach;
    server.attach();

    int cnt = 300;
    while(cnt--) {
        ijn_msleep(33);
        Packet1 *packet1 = new Packet1();
        packet1->set_number(11);
        server.control(packet1);

        ijn_msleep(33);
        Packet2 *packet2 = new Packet2();
        packet2->set_str("this is sample string");
        server.control(packet2);

        ijn_msleep(33);
        Packet3 *packet3 = new Packet3();
        packet3->set_boolvalue(true);
        server.control(packet3);

        ijn_msleep(33);
        Packet4 *packet4 = new Packet4();
        packet4->set_doublevalue(5000.123);
        packet4->set_floatvalue(123.4f);
        server.control(packet4);
        
        ijn_msleep(33);
        ArrayMessage *arrayMessage = new ArrayMessage();
        arrayMessage->add_strarr("this");
        arrayMessage->add_strarr("is");
        arrayMessage->add_strarr("SCNet");
        arrayMessage->add_strarr("example");
        server.control(arrayMessage);
        
        ijn_msleep(33);
        ImageRequest *imageRequest = new ImageRequest();
        imageRequest->set_name("hello.jpg");
        server.control(imageRequest);
    }

    ijn_sleep(1);
    ijn_print(DP_INFO, "Press Enter to detach");
    getchar();

    server.detach();
    ijn_print(DP_INFO, "Press enter to quit");
    getchar();
    return 0;
}

void onPacket1(ijoon::Server *server, Packet1 *pkt1) {
    ijn_print(DP_DEBUG, "[onPacket1()] number=%d", pkt1->number());
}

void onPacket2(ijoon::Server *server, Packet2 *pkt2) {
    ijn_print(DP_DEBUG, "[onPacket2()] str=%s", pkt2->str().c_str());
}

void onPacket3(ijoon::Server *server, Packet3 *pkt3) {
    ijn_print(DP_DEBUG, "[onPacket3()] boolvalue=%s", pkt3->boolvalue()?"true":"false");
}

void onPacket4(ijoon::Server *server, Packet4 *pkt4) {
    ijn_print(DP_DEBUG, "[onPacket4()] floatvalue=%f, doublevalue=%lf", pkt4->floatvalue(), pkt4->doublevalue());
}

void onArrayMessage(ijoon::Server *server, ArrayMessage *arrayMessage) {
    ijn_print(DP_INFO, "[onArrayMessage()] received array size: %d, message: ", arrayMessage->strarr_size());
    for(int i = 0 ; i < arrayMessage->strarr_size() ; i++) {
        printf("%s ", arrayMessage->strarr(i).c_str());
    }
    printf("\n");
}

void onImageResponse(ijoon::Server *server, ImageResponse *imageResponse) {
    const char *imageBuffer = imageResponse->imagebuffer().c_str();
    ImageHeader header = imageResponse->header();
    
    ijn_print(DP_DEBUG, "[onImageResponse()] imageResponse received, name=%s, width=%d, height=%d, size=%d", header.name().c_str(), header.width(), header.height(), header.size());
}

void onAttaching(ijoon::Server *server) {
    ijn_print(DP_INFO, "[%d] attaching", server->getIdentifier());
}

void attachFailed(ijoon::Server *server) {
    ijn_print(DP_INFO, "[%d] attachFailed", server->getIdentifier());
}

void attached(ijoon::Server *server) {
    ijn_print(DP_INFO, "[%d] attached", server->getIdentifier());
}

void detached(ijoon::Server *server) {
    ijn_print(DP_INFO, "[%d] detached", server->getIdentifier());
}

void detach(ijoon::Server *server) {
    ijn_print(DP_INFO, "[%d] detach", server->getIdentifier());
}
