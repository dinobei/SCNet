#include "relay_server_callback.h"
#include "relay_server.h"

extern char seperator;

void rels::onRelayServiceRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
    auto vec = ijoon::paramParser((char *)buffer, 3);
    if(vec == nullptr) return;
    auto connectionIDStr = vec->at(0);
    auto connectionID = static_cast<uint>(atoi(connectionIDStr.c_str()));
    
    ijoon::RelayServer& relayServer = ijoon::RelayServer::getInstance();
    
    if(relayServer.map.count(connectionID) != 0) {
        ijn_print(DP_ERROR, "[RELAY_SERVICE_REQUEST] already registered");
        return;
    }
    
    auto relayPeerInfo = std::shared_ptr<ijoon::RelayPeerInfo>(new ijoon::RelayPeerInfo());

    relayServer.map[connectionID] = relayPeerInfo;
    
    relayServer.sessionCheckMap[connectionID] = 0;
    
    kcpPeer->send(ijoon::RELAY_SESSION_READY, (char *)connectionIDStr.c_str(), connectionIDStr.length());
}

void rels::onRegistrationRelayServerResponse(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
    auto vec = ijoon::paramParser((char *)buffer, 1);
    auto errStr = vec->at(0);
    bool isSuccess = (errStr == "1") ? true : false;
    
    if(isSuccess) {
        ijn_print(DP_INFO, "REGISTRATION_RELAY_SERVER_RESPONSE success");
        kcpPeer->status = ijoon::PeerStatus::REGISTERED;
    }
    else {
        ijn_print(DP_INFO, "REGISTRATION_RELAY_SERVER_RESPONSE result : %s", errStr.c_str());
    }
}

void rels::onRegistrationRelayPeerRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
    auto vec = ijoon::paramParser((char *)buffer, 2);
    if(vec == nullptr) return;
    auto connectionIDStr = vec->at(0);
    auto connectionID = static_cast<uint>(atoi(connectionIDStr.c_str()));
    auto isSP = atoi(vec->at(1).c_str()) ? true : false;
    
    ijoon::RelayServer& relayServer = ijoon::RelayServer::getInstance();
    
    //TODO: mutex 사용
    if(relayServer.map.count(connectionID) == 0) {
        ijn_print(DP_ERROR, "[REGISTRATION_RELAY_PEER_REQUEST] invalid connection id");
        return;
    }
    
    if(relayServer.sessionCheckMap.count(connectionID) == 0) {
        ijn_print(DP_ERROR, "[REGISTRATION_RELAY_PEER_REQUEST] already checked peer");
        return;
    }
    
    kcpPeer->type = ijoon::PeerType::RENDEZVOUS_CLIENT;
    kcpPeer->status = ijoon::PeerStatus::REGISTERED;
    if(isSP) {
        relayServer.map[connectionID]->sourceKcpPeer = kcpPeer;
    }
    else {
        relayServer.map[connectionID]->targetKcpPeer = kcpPeer;
    }
    
    relayServer.sessionCheckMap[connectionID]++;
    
    if(relayServer.sessionCheckMap[connectionID] >= 2) {
        // successfully registerred
        relayServer.sessionCheckMap.erase(connectionID);
        
        auto serverKcpPeer = relayServer.getKcpPeer(relayServer.serverPeer);
        
        serverKcpPeer->send(ijoon::RELAY_SESSION_CREATED, (char *)connectionIDStr.c_str(), connectionIDStr.length());
    }
}

void rels::onPingRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
    kcpPeer->send(ijoon::PING_RESPONSE);
}

void rels::onPingResponse(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
}
