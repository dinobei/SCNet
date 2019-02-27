#include "rendezvous_server.h"
#include <sys/stat.h>
#include "registry.h"
#include "controller/rendezvous_database.h"

#include "packet_type.pb.h"
#include "packet.pb.h"

using namespace example;

std::shared_ptr<ijoon::RendezvousDatabase> db;

void onCameraListRequest(ijoon::RendezvousSession *rendezvousSession, CameraListRequest *cameraListRequest) {
    auto cameraList = db->getRendezvousClientList();
    rendezvousSession->send(cameraList);
}

int main(int argc, char** argv) {
    if(argc != 6) {
        printf("Usage : %s <rendezvous_server_port> <database_name> <database_server_addr> <user> <password>\n", argv[0]);
        exit(-1);
    }
    
    ijoon::initGlobalVariables();
    SCNET_PROTOBUF_UDP_MESSAGE_REGISTRATION(example, example::PacketType::cameraListRequest, CameraListRequest, onCameraListRequest);
    SCNET_PROTOBUF_UDP_MESSAGE_REGISTRATION(example, example::PacketType::cameraListResponse, CameraListResponse, nullptr);

    db = std::shared_ptr<ijoon::RendezvousDatabase>(new ijoon::RendezvousDatabase(argv[2], argv[3], argv[4], argv[5]));
    
    ijoon::RendezvousServer server(atoi(argv[1]));
    server.start();
    server.registerRendezvousClient = [](std::string serial, std::string publicIP, std::string publicPort, std::string privateIP, std::string privatePort) {
        
        auto privatePeer = db->getRendezvousClient(publicIP, publicPort);
        if(privatePeer == nullptr) {
            db->registrationRendezvousClient(publicIP, publicPort, privateIP, privatePort, "test_serial");
        }
        else {
            db->updateRendezvousClient(publicIP, publicPort);
        }
    };
    
    server.removeRendezvousClient = [](std::string ip, std::string port) {
        db->removeRendezvousClient(ip, port);
    };
    
    server.getRendezvousClient = [](std::string ip, std::string port)->std::shared_ptr<ijoon::Peer> {
        return db->getRendezvousClient(ip, port);
    };
    
    getchar();
    
    return 0;
}
