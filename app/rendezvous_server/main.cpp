#include "rendezvous_server.h"
#include <sys/stat.h>
#include "registry.h"

#include "packet_type.pb.h"
#include "packet.pb.h"

using namespace example;

void onCameraListRequest(ijoon::RendezvousSession *rendezvousSession, CameraListRequest *cameraListRequest) {
    ijn_print(DP_INFO, "called onCameraListRequest()");
}

int main(int argc, char** argv) {
    if(argc != 2) {
        printf("Usage : %s <rendezvous_server_port>\n", argv[0]);
        exit(-1);
    }
    
    ijoon::initGlobalVariables();
    SCNET_PROTOBUF_UDP_MESSAGE_REGISTRATION(example, example::PacketType::cameraListRequest, CameraListRequest, onCameraListRequest);
    SCNET_PROTOBUF_UDP_MESSAGE_REGISTRATION(example, example::PacketType::cameraListResponse, CameraListResponse, nullptr);
    
    ijoon::RendezvousServer server(atoi(argv[1]));
    server.start();
    server.registerRendezvousClient = [](std::string serial, std::string publicIP, std::string publicPort, std::string privateIP, std::string privatePort) {
        ijn_print(DP_INFO, "called registerRendezvousClient()");
    };
    
    server.removeRendezvousClient = [](std::string ip, std::string port) {
        ijn_print(DP_INFO, "called removeRendezvousClient()");
    };
    
    server.getRendezvousClient = [](std::string ip, std::string port)->std::shared_ptr<ijoon::Peer> {
        ijn_print(DP_INFO, "called getRendezvousClient()");
    };
    
    server.registerRelayServer = [](std::string name, std::string ip, std::string port, std::string version) {
        ijn_print(DP_INFO, "called registerRelayServer()");
    };
    
    server.removeRelayServer = [](std::string ip, std::string port) {
        ijn_print(DP_INFO, "called removeRelayServer()");
    };
    
    server.isExistRelayServer = [](std::string ip, std::string port)->bool {
        ijn_print(DP_INFO, "called isExistRelayServer()");
        return false;
    };
    
    server.getRelayServerPeer = []()->std::shared_ptr<ijoon::Peer> {
        ijn_print(DP_INFO, "called getRelayServerPeer()");
        return nullptr;
    };
    
    getchar();
    
    return 0;
}
