#include "rendezvous_server.h"
#include <sys/stat.h>
#include "registry.h"
#include "controller/rendezvous_database.h"

#include "packet_type.pb.h"
#include "packet.pb.h"

using namespace example;

std::string dbName, dbServAddr, dbUser, dbPwd;

void onCameraListRequest(ijoon::RendezvousSession *rendezvousSession, CameraListRequest *cameraListRequest) {
    auto db = std::shared_ptr<ijoon::RendezvousDatabase>(new ijoon::RendezvousDatabase(dbName, dbServAddr, dbUser, dbPwd));
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
    
    dbName = argv[2];
    dbServAddr = argv[3];
    dbUser = argv[4];
    dbPwd = argv[5];
    
    ijoon::RendezvousServer server(atoi(argv[1]));
    server.start();
    server.registerRendezvousClient = [](std::string serial, std::string publicIP, std::string publicPort, std::string privateIP, std::string privatePort) {
        auto db = std::shared_ptr<ijoon::RendezvousDatabase>(new ijoon::RendezvousDatabase(dbName, dbServAddr, dbUser, dbPwd));
        auto privatePeer = db->getRendezvousClient(publicIP, publicPort);
        if(privatePeer == nullptr) {
            db->registrationRendezvousClient(publicIP, publicPort, privateIP, privatePort, serial);
        }
        else {
            db->updateRendezvousClient(publicIP, publicPort);
        }
    };
    
    server.removeRendezvousClient = [](std::string ip, std::string port) {
        auto db = std::shared_ptr<ijoon::RendezvousDatabase>(new ijoon::RendezvousDatabase(dbName, dbServAddr, dbUser, dbPwd));
        db->removeRendezvousClient(ip, port);
    };
    
    server.getRendezvousClient = [](std::string ip, std::string port)->std::shared_ptr<ijoon::Peer> {
        auto db = std::shared_ptr<ijoon::RendezvousDatabase>(new ijoon::RendezvousDatabase(dbName, dbServAddr, dbUser, dbPwd));
        return db->getRendezvousClient(ip, port);
    };
    
    server.registerRelayServer = [](std::string name, std::string ip, std::string port, std::string version) {
        auto db = std::shared_ptr<ijoon::RendezvousDatabase>(new ijoon::RendezvousDatabase(dbName, dbServAddr, dbUser, dbPwd));
        auto relayPeer = db->getRelayServer(ip, port);
        if(relayPeer == nullptr) {
            db->registrationRelayServer(name, ip, port, version);
        }
        else {
            db->updateRelayServer(ip, port);
        }
    };
    
    server.removeRelayServer = [](std::string ip, std::string port) {
        auto db = std::shared_ptr<ijoon::RendezvousDatabase>(new ijoon::RendezvousDatabase(dbName, dbServAddr, dbUser, dbPwd));
        db->removeRelayServer(ip, port);
    };
    
    server.isExistRelayServer = [](std::string ip, std::string port)->bool {
        auto db = std::shared_ptr<ijoon::RendezvousDatabase>(new ijoon::RendezvousDatabase(dbName, dbServAddr, dbUser, dbPwd));
        if(db->getRelayServer(ip, port) == nullptr) {
            return false;
        }
        return true;
    };
    
    server.getRelayServerPeer = []()->std::shared_ptr<ijoon::Peer> {
        auto db = std::shared_ptr<ijoon::RendezvousDatabase>(new ijoon::RendezvousDatabase(dbName, dbServAddr, dbUser, dbPwd));
        auto relayServerList = db->getRelayServerList();
        
        if(relayServerList->size() == 0) return nullptr;
        
        int latestPing = relayServerList->at(0)->ping;
        int index = 0;
        for(int i = 1 ; i < relayServerList->size() ; i++) {
            if(relayServerList->at(i)->ping > latestPing) {
                index = i;
                latestPing = relayServerList->at(i)->ping;
            }
        }
        
        if( latestPing + 60 < ijoon::ComputableTime::getCurrentTimeSec()) {
            return nullptr;
        }
        
        auto relayServerModel = relayServerList->at(index);
        auto relayPeer = std::shared_ptr<ijoon::Peer>(new ijoon::Peer(relayServerModel->publicIP, relayServerModel->publicPort));
        return relayPeer;
    };
    
    getchar();
    
    return 0;
}
