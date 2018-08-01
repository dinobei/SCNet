#include <ijoon/coreutils.h>
#include "client_manager.h"
#include "registry.h"

/* implemented proto messages */
#include "packet.pb.h"
using namespace simple;

void callback(int serverIndex, google::protobuf::Message *response)
{
    google::protobuf::uint32 type = BaseMessageRegistry->GetType(response->GetTypeName());

    switch (type) {
        case simple::packetType1:
        {
            simple::packet_1 *pkt = static_cast<simple::packet_1 *>(response);
            ijn_print(DP_INFO, "Response data is packetType1(%d), pkt->number(): %d", type, pkt->number());
        }
            break;
        case simple::packetType2:
        {
            simple::packet_2 *pkt = static_cast<simple::packet_2 *>(response);
            ijn_print(DP_INFO, "Response data is packetType2(%d), pkt->number(): %d", type, pkt->number());
        }
            break;
        case simple::packetType3:
        {
            simple::packet_3 *pkt = static_cast<simple::packet_3 *>(response);
            ijn_print(DP_INFO, "Response data is packetType3(%d), pkt->number(): %d", type, pkt->number());
        }
            break;
        case simple::packetType4:
        {
            simple::packet_4 *pkt = static_cast<simple::packet_4 *>(response);
            ijn_print(DP_INFO, "Response data is packetType4(%d), pkt->number(): %d", type, pkt->number());
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
    IJN_REGISTER_MESSAGES(simple, simple::PacketType::packetType1, packet_1);
    IJN_REGISTER_MESSAGES(simple, simple::PacketType::packetType2, packet_2);
    IJN_REGISTER_MESSAGES(simple, simple::PacketType::packetType3, packet_3);
    IJN_REGISTER_MESSAGES(simple, simple::PacketType::packetType4, packet_4);
    
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
        simple::packet_1 *packet1 = new simple::packet_1();
        packet1->set_number(11);
        manager.Control(serverIndex, packet1);

        ijn_msleep(33);
        simple::packet_2 *packet2 = new simple::packet_2();
        packet2->set_number(22);
        manager.Control(serverIndex, packet2);

        ijn_msleep(33);
        simple::packet_3 *packet3 = new simple::packet_3();
        packet3->set_number(33);
        manager.Control(serverIndex, packet3);

        ijn_msleep(33);
        simple::packet_4 *packet4 = new simple::packet_4();
        packet4->set_number(44);
        manager.Control(serverIndex, packet4);
    }

    ijn_sleep(1);
    ijn_print(DP_INFO, "Press Enter to detach");
    getchar();

    manager.Detach(serverIndex);
    ijn_print(DP_INFO, "Press enter to quit");
    getchar();
    return 0;
}
