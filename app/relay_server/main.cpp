#include "relay_server.h"
#include <sys/stat.h>
#include "registry.h"

int main(int argc, char** argv) {
    if(argc != 4) {
        printf("Usage : %s <relay_server_port> <rendezvous_server_ip> <rendezvous_server_port>\n", argv[0]);
        exit(-1);
    }
    
    ijoon::RelayServer::init(atoi(argv[1]), argv[2], argv[3]);
    ijoon::RelayServer& server = ijoon::RelayServer::getInstance();
    server.socket->option(ijoon::SOCK_REUSE, 1);
    server.start();
    
    while(true) {
        getchar();
        
        ijn_print(DP_INFO, "---------KcpPeerMap---------");
        auto kpmap = server.getKcpPeerMap();
        for(auto iter = kpmap.begin() ; iter != kpmap.end() ; ++iter) {
            std::string type;
            switch (iter->second->type) {
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
            ijn_print(DP_INFO, "%s [%s]", iter->first.c_str(),
                      type.c_str());
        }
        
        auto relayPeerInfoMap = server.getRelayPeerInfoMap();
        
        ijn_print(DP_INFO, "------RelayPeerInfoMap------");
        for(auto iter : relayPeerInfoMap) {
            ijn_print(DP_INFO, "[%d] %s <-> %s",
                      iter.first,
                      iter.second->sourceKcpPeer->getPeer().getKey().c_str(),
                      iter.second->targetKcpPeer->getPeer().getKey().c_str());
        }
        
        ijn_print(DP_INFO, "-------SessionCheckMap------");
        auto sessionCheckMap = server.getSessionCheckMap();
        for(auto iter : sessionCheckMap) {
            ijn_print(DP_INFO, "[%d] %d",
                      iter.first, iter.second);
        }
        ijn_print(DP_INFO, "----------------------------");
    }
        
    return 0;
}

