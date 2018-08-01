#include "embodied_server.h"

int main(int argv, char** argc) {
    ijoon::initGlobalVariables();
    IJN_REGISTER_MESSAGES(simple, simple::PacketType::packetType1, packet_1);
    IJN_REGISTER_MESSAGES(simple, simple::PacketType::packetType2, packet_2);
    IJN_REGISTER_MESSAGES(simple, simple::PacketType::packetType3, packet_3);
    IJN_REGISTER_MESSAGES(simple, simple::PacketType::packetType4, packet_4);
    
    ijoon::EmbodiedServer myServer;
    myServer.start();
    getchar();
    
    myServer.stop();
    getchar();
    
    return 0;
}
