#include "rendezvous_server.h"
#include <sys/stat.h>
#include "registry.h"

#include "packet_type.pb.h"
#include "packet.pb.h"

using namespace example;

struct RendezvousClientInfo {
    std::string serial;
    std::string publicIP;
    std::string publicPort;
    std::string privateIP;
    std::string privatePort;
    std::string mac;
    std::string version;
    
};

struct RelayServerInfo {
    std::string name;
    std::string ip;
    std::string port;
    std::string version;
    
};

std::map<std::string, RendezvousClientInfo> rendezvousClientMap;
std::map<std::string, RelayServerInfo> relayServerMap;

void onCameraListRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, CameraListRequest *cameraListRequest) {
    ijn_print(DP_INFO, "called onCameraListRequest()");
}

int main(int argc, char** argv) {
    if(argc != 2) {
        printf("Usage : %s <rendezvous_server_port>\n", argv[0]);
        exit(-1);
    }
    
    SCNET_PROTOBUF_UDP_MESSAGE_REGISTRATION(example::PacketType::cameraListRequest, CameraListRequest, onCameraListRequest);
    SCNET_PROTOBUF_UDP_MESSAGE_REGISTRATION(example::PacketType::cameraListResponse, CameraListResponse, nullptr);
    
    ijoon::RendezvousServer::init(atoi(argv[1]));
    ijoon::RendezvousServer& server = ijoon::RendezvousServer::getInstance();
    server.start();
    server.callback.registerRendezvousClient = [](std::string serial, std::string publicIP, std::string publicPort, std::string privateIP, std::string privatePort, std::string mac, std::string version) -> bool {
        ijn_print(DP_INFO, "called registerRendezvousClient()");
        RendezvousClientInfo info;
        info.serial = serial;
        info.publicIP = publicIP;
        info.publicPort = publicPort;
        info.privateIP = privateIP;
        info.privatePort = privatePort;
        info.mac = mac;
        info.version = version;
        rendezvousClientMap[publicIP + ":" + publicPort] = info;
        return true;
    };
    
    server.callback.removeRendezvousClient = [](std::string ip, std::string port)->bool {
        try {
            rendezvousClientMap.erase(ip+":"+port);
            ijn_print(DP_INFO, "renCMap.size() : %d", rendezvousClientMap.size());
        }
        catch(std::exception e) {
            return false;
        }
        
        return true;
    };
    
    server.callback.getRendezvousClientPeer = [](std::string ip, std::string port)->std::shared_ptr<ijoon::Peer> {
        try {
            auto renC = rendezvousClientMap.at(ip+":"+port);
            return std::shared_ptr<ijoon::Peer>(new ijoon::Peer(renC.privateIP, renC.privatePort));
        }
        catch(std::exception e) {
            
        }
        
		return nullptr;
    };
    
    server.callback.registerRelayServer = [](std::string name, std::string ip, std::string port, std::string version) -> bool {
        RelayServerInfo info;
        info.name = name;
        info.ip = ip;
        info.port = port;
        info.version = version;
        relayServerMap[ip+":"+port] = info;
        return true;
    };
    
    server.callback.removeRelayServer = [](std::string ip, std::string port)->bool {
        try {
            relayServerMap.erase(ip+":"+port);
            ijn_print(DP_INFO, "relayServerMap.size() : %d", relayServerMap.size());
        }
        catch(std::exception e) {
            return false;
        }
        
        return true;
    };
    
    server.callback.isExistRelayServer = [](std::string ip, std::string port)->bool {
        try {
            auto relS = relayServerMap.at(ip+":"+port);
            int cnt = relayServerMap.count(ip+":"+port);
            if(cnt > 0) {
                return true;
            }
        }
        catch(std::exception e) {
            
        }
        
        return false;
    };
    
    server.callback.getRelayServerPeer = []()->std::shared_ptr<ijoon::Peer> {
        try {
            auto begin = relayServerMap.begin();
            if(begin != relayServerMap.end()) {
                return std::shared_ptr<ijoon::Peer>(new ijoon::Peer(begin->second.ip, begin->second.port));
            }
        }
        catch(std::exception e) {
            
        }
        return nullptr;
    };
    
    
    while(true) {
        getchar();
        
        ijn_print(DP_INFO, "----------KcpPeerMap---------");
        auto kcpPeerMap = server.getKcpPeerMap();
        for(auto iter : kcpPeerMap) {
            std::string type;
            switch (iter.second->type) {
                case ijoon::PeerType::RENDEZVOUS_CLIENT:
                    type="Rendezvous Client";
                    break;
                case ijoon::PeerType::RENDEZVOUS_SERVER:
                    type="Rendezvous Server";
                    break;
                case ijoon::PeerType::RELAY_SERVER:
                    type="Relay Server";
                    break;
                default:
                    type="None";
            }
            ijn_print(DP_INFO, "%s [%s]", iter.first.c_str(),
                      type.c_str());
        }
        
        ijn_print(DP_INFO, "------ConnectionInfoMap------");
        auto connectionInfoMap = server.getConnectionInfoMap();
        for(auto iter : connectionInfoMap) {
            ijn_print(DP_INFO, "[%d] Public:%s, Private:%s",
                      iter.first,
                      iter.second->publicSP->getKey().c_str(),
                      iter.second->privateSP->getKey().c_str());
        }
        ijn_print(DP_INFO, "-----------------------------");
    }
    
    getchar();
    
    return 0;
}
