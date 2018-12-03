#include <ijoon/coreutils.h>
#include "client_manager.h"
#include "registry.h"

/* implemented proto messages */
#include "packet.pb.h"
#include "packet_type.pb.h"
#include "get_image.pb.h"
using namespace example;

void callback(int serverIndex, google::protobuf::Message *response)
{
    google::protobuf::uint32 type = BaseMessageRegistry->GetType(response->GetTypeName());

    switch (type) {
        case PacketType::packetType1:
        {
            Packet1 *pkt = static_cast<Packet1 *>(response);
            ijn_print(DP_INFO, "Response data is packetType1(%d), pkt->number(): %d", type, pkt->number());
        }
            break;
        case PacketType::packetType2:
        {
            Packet2 *pkt = static_cast<Packet2 *>(response);
            ijn_print(DP_INFO, "Response data is packetType2(%d), pkt->str().c_str(): %s", type, pkt->str().c_str());
        }
            break;
        case PacketType::packetType3:
        {
            Packet3 *pkt = static_cast<Packet3 *>(response);
            ijn_print(DP_INFO, "Response data is packetType3(%d), pkt->boolValue(): %s", type, pkt->boolvalue()?"true":"false");
        }
            break;
        case PacketType::packetType4:
        {
            Packet4 *pkt = static_cast<Packet4 *>(response);
            ijn_print(DP_INFO, "Response data is packetType4(%d), pkt->doubleValue(): %lf, pkt->floatValue(): %f", type, pkt->doublevalue(), pkt->floatvalue());
        }
            break;
        case PacketType::arrayMessageType:
        {
            ArrayMessage *pkt = static_cast<ArrayMessage *>(response);
            for(int i = 0 ; i < pkt->strarr_size() ; i++) {
                ijn_print(DP_INFO, "%d) %s", i, pkt->strarr(i).c_str());
            }
        }
            break;
        case PacketType::imageResponse:
        {
            ijn_print(DP_DEBUG, "imageResponse called");
            ImageResponse *pkt = static_cast<ImageResponse *>(response);
            const char *imageBuffer = pkt->imagebuffer().c_str();
            ImageHeader header = pkt->header();
        }
            break;
        default:
            break;
    }
}

void attaching(int serverIndex) {
    ijn_print(DP_INFO, "[%d] attaching", serverIndex);
}

void attachFailed(int serverIndex) {
    ijn_print(DP_INFO, "[%d] attachFailed", serverIndex);
}

void attached(int serverIndex) {
    ijn_print(DP_INFO, "[%d] attached", serverIndex);
}

void detached(int serverIndex) {
    ijn_print(DP_INFO, "[%d] detached", serverIndex);
}

void detach(int serverIndex) {
    ijn_print(DP_INFO, "[%d] detach", serverIndex);
}

int main(int argv, char** argc)
{
    ijoon::initGlobalVariables();
    IJN_REGISTER_MESSAGES(example, PacketType::packetType1, Packet1);
    IJN_REGISTER_MESSAGES(example, PacketType::packetType2, Packet2);
    IJN_REGISTER_MESSAGES(example, PacketType::packetType3, Packet3);
    IJN_REGISTER_MESSAGES(example, PacketType::packetType4, Packet4);
    IJN_REGISTER_MESSAGES(example, PacketType::arrayMessageType, ArrayMessage);
    IJN_REGISTER_MESSAGES(example, PacketType::imageRequest, ImageRequest);
    IJN_REGISTER_MESSAGES(example, PacketType::imageResponse, ImageResponse);
    
    ijoon::ClientManager manager;
    manager.onCallback = callback;
    manager.onAttaching = attaching;
    manager.onAttachFailed = attachFailed;
    manager.onAttached = attached;
    manager.onDetached = detached;
    manager.onDetach = detach;

    const char* host_name="127.0.0.1";
    int host_port= 9190;

    int serverIndex = manager.Attach(host_name, host_port);
    if(serverIndex < 0) {
        ijn_print(DP_INFO, "can't connect to server");
        return -1;
    }

    ijn_print(DP_INFO, "serverIndex : %d", serverIndex);

    ijn_sleep(1);

    int cnt = 300;
    while(cnt--) {
        ijn_msleep(33);
        Packet1 *packet1 = new Packet1();
        packet1->set_number(11);
        manager.Control(serverIndex, packet1);

        ijn_msleep(33);
        Packet2 *packet2 = new Packet2();
        packet2->set_str("this is sample string");
        manager.Control(serverIndex, packet2);

        ijn_msleep(33);
        Packet3 *packet3 = new Packet3();
        packet3->set_boolvalue(true);
        manager.Control(serverIndex, packet3);

        ijn_msleep(33);
        Packet4 *packet4 = new Packet4();
        packet4->set_doublevalue(5000.123);
        packet4->set_floatvalue(123.4f);
        manager.Control(serverIndex, packet4);
        
        ijn_msleep(33);
        ArrayMessage *arrayMessage = new ArrayMessage();
        arrayMessage->add_strarr("this");
        arrayMessage->add_strarr("is");
        arrayMessage->add_strarr("SCNet");
        arrayMessage->add_strarr("example");
        manager.Control(serverIndex, arrayMessage);
        
        ijn_msleep(33);
        ImageRequest *imageRequest = new ImageRequest();
        imageRequest->set_name("hello.jpg");
        manager.Control(serverIndex, imageRequest);
    }

    ijn_sleep(1);
    ijn_print(DP_INFO, "Press Enter to detach");
    getchar();

    manager.Detach(serverIndex);
    ijn_print(DP_INFO, "Press enter to quit");
    getchar();
    return 0;
}
