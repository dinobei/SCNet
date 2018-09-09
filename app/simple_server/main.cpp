#include "embodied_server.h"

int main(int argv, char** argc) {
    ijoon::initGlobalVariables();
    IJN_REGISTER_MESSAGES(example, example::PacketType::packetType1, Packet1);
    IJN_REGISTER_MESSAGES(example, example::PacketType::packetType2, Packet2);
    IJN_REGISTER_MESSAGES(example, example::PacketType::packetType3, Packet3);
    IJN_REGISTER_MESSAGES(example, example::PacketType::packetType4, Packet4);
    IJN_REGISTER_MESSAGES(example, example::PacketType::arrayMessageType, ArrayMessage);
    IJN_REGISTER_MESSAGES(example, example::PacketType::imageRequest, ImageRequest);
    IJN_REGISTER_MESSAGES(example, example::PacketType::imageResponse, ImageResponse);
    
    ijoon::EmbodiedServer myServer(9190);
    myServer.start();
    getchar();
    
    myServer.stop();
    getchar();
    
    return 0;
}
