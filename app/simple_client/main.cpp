#include <ijoon/coreutils.h>
#include "client.h"
#include "registry.h"

/* implemented proto messages */
#include "packet.pb.h"
#include "packet_type.pb.h"
#include "get_image.pb.h"
using namespace example;

void onPacket1(ijoon::Client *client, Packet1 *pkt1) {
    ijn_print(DP_DEBUG, "[onPacket1()] number=%d", pkt1->number());
}

void onPacket2(ijoon::Client *session, Packet2 *pkt2) {
    ijn_print(DP_DEBUG, "[onPacket2()] str=%s", pkt2->str().c_str());
}

void onPacket3(ijoon::Client *session, Packet3 *pkt3) {
    ijn_print(DP_DEBUG, "[onPacket3()] boolvalue=%s", pkt3->boolvalue()?"true":"false");
}

void onPacket4(ijoon::Client *session, Packet4 *pkt4) {
    ijn_print(DP_DEBUG, "[onPacket4()] floatvalue=%f, doublevalue=%lf", pkt4->floatvalue(), pkt4->doublevalue());
}

void onArrayMessage(ijoon::Client *session, ArrayMessage *arrayMessage) {
    ijn_print(DP_INFO, "[onArrayMessage()] received array size: %d, message: ", arrayMessage->strarr_size());
    for(int i = 0 ; i < arrayMessage->strarr_size() ; i++) {
        printf("%s ", arrayMessage->strarr(i).c_str());
    }
    printf("\n");
}

void onImageResponse(ijoon::Client *client, ImageResponse *imageResponse) {
    const char *imageBuffer = imageResponse->imagebuffer().c_str();
    ImageHeader header = imageResponse->header();
    
    ijn_print(DP_DEBUG, "[onImageResponse()] imageResponse received, name=%s, width=%d, height=%d, size=%d", header.name().c_str(), header.width(), header.height(), header.size());
}

void onAttaching(ijoon::Client *client) {
    ijn_print(DP_INFO, "[%d] attaching", client->getIdentifier());
}

void attachFailed(ijoon::Client *client) {
    ijn_print(DP_INFO, "[%d] attachFailed", client->getIdentifier());
}

void attached(ijoon::Client *client) {
    ijn_print(DP_INFO, "[%d] attached", client->getIdentifier());
}

void detached(ijoon::Client *client) {
    ijn_print(DP_INFO, "[%d] detached", client->getIdentifier());
}

void detach(ijoon::Client *client) {
    ijn_print(DP_INFO, "[%d] detach", client->getIdentifier());
}

int main(int argv, char** argc)
{
    ijoon::initGlobalVariables();
    SCNET_MESSAGE_REGISTRATION_WITH_RECV_CALLBACK(example, PacketType::packetType1, Packet1, ijoon::Client, onPacket1);
    SCNET_MESSAGE_REGISTRATION_WITH_RECV_CALLBACK(example, PacketType::packetType2, Packet2, ijoon::Client, onPacket2);
    SCNET_MESSAGE_REGISTRATION_WITH_RECV_CALLBACK(example, PacketType::packetType3, Packet3, ijoon::Client, onPacket3);
    SCNET_MESSAGE_REGISTRATION_WITH_RECV_CALLBACK(example, PacketType::packetType4, Packet4, ijoon::Client, onPacket4);
    SCNET_MESSAGE_REGISTRATION_WITH_RECV_CALLBACK(example, PacketType::arrayMessageType, ArrayMessage, ijoon::Client, onArrayMessage);
    SCNET_MESSAGE_REGISTRATION(example, PacketType::imageRequest, ImageRequest);
    SCNET_MESSAGE_REGISTRATION_WITH_RECV_CALLBACK(example, PacketType::imageResponse, ImageResponse, ijoon::Client, onImageResponse);
    
    ijoon::Client client("127.0.0.1", 9190, 1000);
    client.onAttaching = onAttaching;
    client.onAttachFailed = attachFailed;
    client.onAttached = attached;
    client.onDetached = detached;
    client.onDetach = detach;
    client.attach();

    int cnt = 300;
    while(cnt--) {
        ijn_msleep(33);
        Packet1 *packet1 = new Packet1();
        packet1->set_number(11);
        client.control(packet1);

        ijn_msleep(33);
        Packet2 *packet2 = new Packet2();
        packet2->set_str("this is sample string");
        client.control(packet2);

        ijn_msleep(33);
        Packet3 *packet3 = new Packet3();
        packet3->set_boolvalue(true);
        client.control(packet3);

        ijn_msleep(33);
        Packet4 *packet4 = new Packet4();
        packet4->set_doublevalue(5000.123);
        packet4->set_floatvalue(123.4f);
        client.control(packet4);
        
        ijn_msleep(33);
        ArrayMessage *arrayMessage = new ArrayMessage();
        arrayMessage->add_strarr("this");
        arrayMessage->add_strarr("is");
        arrayMessage->add_strarr("SCNet");
        arrayMessage->add_strarr("example");
        client.control(arrayMessage);
        
        ijn_msleep(33);
        ImageRequest *imageRequest = new ImageRequest();
        imageRequest->set_name("hello.jpg");
        client.control(imageRequest);
    }

    ijn_sleep(1);
    ijn_print(DP_INFO, "Press Enter to detach");
    getchar();

    client.detach();
    ijn_print(DP_INFO, "Press enter to quit");
    getchar();
    return 0;
}
