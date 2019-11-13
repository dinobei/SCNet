#include "rendezvous_client_callback.h"
#include "rendezvous_client.h"

extern char seperator;

void renc::onRegistrationRendezvousClientResponse(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
//    ijn_print(DP_DEBUG, "from %s", kcpPeer->getPeer().getKey().c_str());
    auto vec = ijoon::paramParser((char *)buffer, 3);
    if(vec == nullptr) return;

    bool isSuccess = (vec->at(0) == "1") ? true : false;
    std::string spPubIP = vec->at(1);
    std::string spPubPort = vec->at(2);

    if(isSuccess) {
        if(kcpPeer->status != ijoon::PeerStatus::REGISTERED) {
            kcpPeer->status = ijoon::PeerStatus::REGISTERED;

            ijoon::RendezvousClient::getInstance().callback.onServerConnectedCallback(spPubIP, spPubPort);
        }
    }
    else {
        kcpPeer->status = ijoon::PeerStatus::UNREGISTERED;
        ijoon::RendezvousClient::getInstance().callback.onServerConnectFailedCallback();
    }
}

void renc::onConnectionIdCreated(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
//    ijn_print(DP_DEBUG, "from %s", kcpPeer->getPeer().getKey().c_str());
    
    auto vec = ijoon::paramParser((char *)buffer, 3);
    if(vec == nullptr) return;
    
    auto connectionIDStr = vec->at(0);
    auto targetIP = vec->at(1);
    auto targetPort = vec->at(2);
    
    kcpPeer->send(ijoon::CONNECTION_ID_RECEIVED, (char *)connectionIDStr.c_str(), connectionIDStr.length());
}

void renc::onRelayServerInformation(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
    ijn_print(DP_DEBUG, "from %s", kcpPeer->getPeer().getKey().c_str());
    
    auto vec = ijoon::paramParser((char *)buffer, 4);
    if(vec == nullptr) return;
    auto connectionIDStr = vec->at(0);
    auto connectionID = static_cast<uint>(atoi(connectionIDStr.c_str()));
    auto relSIP = vec->at(1);
    auto relSPort = vec->at(2);
    auto isSPStr = vec->at(3);
        
    ijoon::RendezvousClient& client = ijoon::RendezvousClient::getInstance();
    
    client.callback.onConnectingCallback(connectionID);
        
    ijn_print(DP_INFO, "[RELAY_SERVER_INFORMATION] RelayServerAddress=%s:%s", relSIP.c_str(), relSPort.c_str());
    
    auto relayPeer = ijoon::Peer(relSIP, relSPort);
    auto relayKcpPeer = client.getKcpPeer(relayPeer);

    std::string data;
    data = connectionIDStr;
    data += seperator;
    data += isSPStr;
    relayKcpPeer->send(ijoon::REGISTRATION_RELAY_PEER_REQUEST,
                       (char *)data.c_str(),
                       data.length());
}

void renc::onConnectionRelayServiceResult(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
//    ijn_print(DP_DEBUG, "from %s", kcpPeer->getPeer().getKey().c_str());
    
    auto vec = ijoon::paramParser((char *)buffer, 4);
    if(vec == nullptr) return;
    auto isSuccess = (vec->at(0)=="1") ? true : false;
    auto connectionIDStr = vec->at(1);
    auto connectionID = static_cast<uint>(atoi(connectionIDStr.c_str()));
    auto relayServerIP = vec->at(2);
    auto relayServerPort = vec->at(3);

    if(isSuccess) {
        ijoon::RendezvousClient& client = ijoon::RendezvousClient::getInstance();
        
        auto relayPeer = ijoon::Peer(relayServerIP, relayServerPort);
        auto relayKcpPeer = client.getKcpPeer(relayPeer);
        relayKcpPeer->type = ijoon::PeerType::RELAY_SERVER;
        relayKcpPeer->status = ijoon::PeerStatus::REGISTERED;
        relayKcpPeer->connectionID = connectionID;

        client.addConnectionFilter(connectionID, relayPeer);

        printf("connected by a relay from %s:%s, connectionID: %u\n", relayServerIP.c_str(), relayServerPort.c_str(), connectionID);

        client.callback.onConnectedCallback(relayKcpPeer);
    }
    else {
        printf("relay connection failed, connectionID: %d\n", connectionID);
    }
}

void renc::onRelaySessionInvalid(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
    ijn_print(DP_DEBUG, "from %s", kcpPeer->getPeer().getKey().c_str());
    ijoon::RendezvousClient& client = ijoon::RendezvousClient::getInstance();
    
    client.removeConnectionFilter(kcpPeer->connectionID, kcpPeer->getPeer().getKey());
}

void renc::onRelayServerDisconnected(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
    ijn_print(DP_DEBUG, "from %s", kcpPeer->getPeer().getKey().c_str());
    kcpPeer->status = ijoon::PeerStatus::UNREGISTERED;
}

void renc::onDirectConnectionAvailable(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
//    ijn_print(DP_DEBUG, "from %s", kcpPeer->getPeer().getKey().c_str());
                
    auto vec = ijoon::paramParser((char *)buffer, 3);
    if(vec == nullptr) return;
    auto connectionIDStr = vec->at(0);
    auto connectionID = static_cast<uint>(atoi(connectionIDStr.c_str()));
    auto targetIP = vec->at(1);
    auto targetPort = vec->at(2);
    
    ijoon::RendezvousClient& client = ijoon::RendezvousClient::getInstance();
    
    bool isConnected = false;
    if(client.getCountConnectionFilter(connectionID) > 0) {
        isConnected = true;
    }
    
    if(!isConnected) {
        client.callback.onConnectingCallback(connectionID);
    }
    
    auto targetPeer = ijoon::Peer(targetIP.c_str(), targetPort.c_str());
    auto targetKcpPeer = client.getKcpPeer(targetPeer);
    
    std::string data;
    data = connectionIDStr;
    targetKcpPeer->send(ijoon::DIRECT_CONNECTION_REQUEST,
                        (char *)data.c_str(),
                        data.length());
}

void renc::onDirectConnectionRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
//    ijn_print(DP_DEBUG, "from %s", kcpPeer->getPeer().getKey().c_str());
    
    auto vec = ijoon::paramParser((char *)buffer, 1);
    if(vec == nullptr) return;
    auto connectionIDStr = vec->at(0);
    auto connectionID = static_cast<uint>(atoi(connectionIDStr.c_str()));
    
    kcpPeer->type = ijoon::PeerType::RENDEZVOUS_CLIENT;
    kcpPeer->status = ijoon::PeerStatus::REGISTERED;
    kcpPeer->connectionID = connectionID;

    ijoon::RendezvousClient& client = ijoon::RendezvousClient::getInstance();
    
    bool isConnected = false;
    if(client.getCountConnectionFilter(connectionID) > 0) {
        isConnected = true;
    }

    client.addConnectionFilter(connectionID, kcpPeer->getPeer());

    if(!isConnected) {
        client.callback.onConnectingCallback(connectionID);
        printf("directly connected from %s:%d\n", kcpPeer->getPeer().getIP().c_str(), kcpPeer->getPeer().getPort());
        client.callback.onConnectedCallback(kcpPeer);
    }

    std::string data;
    data = connectionIDStr;
    kcpPeer->send(ijoon::DIRECT_CONNECTION_RESPONSE,
                  (char *)data.c_str(),
                  data.length());
}

void renc::onDirectConnectionResponse(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
//    ijn_print(DP_DEBUG, "from %s", kcpPeer->getPeer().getKey().c_str());
    
    auto vec = ijoon::paramParser((char *)buffer, 1);
    if(vec == nullptr) return;
    auto connectionIDStr = vec->at(0);
    auto connectionID = static_cast<uint>(atoi(connectionIDStr.c_str()));
    
    kcpPeer->type = ijoon::PeerType::RENDEZVOUS_CLIENT;
    kcpPeer->status = ijoon::PeerStatus::REGISTERED;
    kcpPeer->connectionID = connectionID;

    ijoon::RendezvousClient& client = ijoon::RendezvousClient::getInstance();
    
    bool isConnected = false;
    if(client.getCountConnectionFilter(connectionID) > 0) {
        isConnected = true;
    }

    client.addConnectionFilter(connectionID, kcpPeer->getPeer());
    printf("directly connected from %s\n", kcpPeer->getPeer().getKey().c_str());

    if(!isConnected) {
        client.callback.onConnectedCallback(kcpPeer);
    }
}

void renc::onReverseConnectionReady(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
//    ijn_print(DP_DEBUG, "from %s", kcpPeer->getPeer().getKey().c_str());
    
    auto vec = ijoon::paramParser((char *)buffer, 1);
    if(vec == nullptr) return;
    auto connectionIDStr = vec->at(0);
    auto connectionID = static_cast<uint>(atoi(connectionIDStr.c_str()));
}

void renc::onReverseConnection(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
//    ijn_print(DP_DEBUG, "from %s", kcpPeer->getPeer().getKey().c_str());
    
    auto vec = ijoon::paramParser((char *)buffer, 3);
    if(vec == nullptr) return;
    auto connectionIDStr = vec->at(0);
    std::string spPublicIP = vec->at(1);
    std::string spPublicPort = vec->at(2);
    
    ijoon::RendezvousClient& client = ijoon::RendezvousClient::getInstance();
    
    auto sourcePeer = ijoon::Peer(spPublicIP.c_str(), spPublicPort.c_str());
    auto sourceKcpPeer = client.getKcpPeer(sourcePeer);
    
    std::string data;
    data = connectionIDStr;
    sourceKcpPeer->send(ijoon::REVERSE_CONNECTION_REQUEST,
                        (char *)data.c_str(),
                        data.length());
}



void renc::onReverseConnectionRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
//    ijn_print(DP_DEBUG, "from %s", kcpPeer->getPeer().getKey().c_str());
    
    auto vec = ijoon::paramParser((char *)buffer, 1);
    if(vec == nullptr) return;
    auto connectionIDStr = vec->at(0);
    auto connectionID = static_cast<uint>(atoi(connectionIDStr.c_str()));
    
    kcpPeer->type = ijoon::PeerType::RENDEZVOUS_CLIENT;
    kcpPeer->status = ijoon::PeerStatus::REGISTERED;
    kcpPeer->connectionID = connectionID;

    std::string data;
    data = connectionIDStr;
    kcpPeer->send(ijoon::REVERSE_CONNECTION_RESPONSE,
                  (char *)data.c_str(),
                  data.length());

    ijoon::RendezvousClient& client = ijoon::RendezvousClient::getInstance();
    
    bool isConnected = false;
    if(client.getCountConnectionFilter(connectionID) > 0) {
        isConnected = true;
    }

    client.addConnectionFilter(connectionID, kcpPeer->getPeer());
    printf("reversely connected from %s\n", kcpPeer->getPeer().getKey().c_str());

    if(!isConnected) {
        client.callback.onConnectedCallback(kcpPeer);
    }

}

void renc::onReverseConnectionResponse(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
//    ijn_print(DP_DEBUG, "from %s", kcpPeer->getPeer().getKey().c_str());
    
    auto vec = ijoon::paramParser((char *)buffer, 1);
    if(vec == nullptr) return;
    auto connectionIDStr = vec->at(0);
    auto connectionID = static_cast<uint>(atoi(connectionIDStr.c_str()));
    
    kcpPeer->type = ijoon::PeerType::RENDEZVOUS_CLIENT;
    kcpPeer->status = ijoon::PeerStatus::REGISTERED;
    kcpPeer->connectionID = connectionID;

    ijoon::RendezvousClient& client = ijoon::RendezvousClient::getInstance();
    
    bool isConnected = false;
    if(client.getCountConnectionFilter(connectionID) > 0) {
        isConnected = true;
    }

    client.addConnectionFilter(connectionID, kcpPeer->getPeer());
    printf("reversely connected from %s\n", kcpPeer->getPeer().getKey().c_str());

    if(!isConnected) {
        client.callback.onConnectedCallback(kcpPeer);
    }
}

void renc::onUdpHolePunchingAvailable(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
//    ijn_print(DP_DEBUG, "from %s", kcpPeer->getPeer().getKey().c_str());
    
    auto vec = ijoon::paramParser((char *)buffer, 5);
    if(vec == nullptr) return;
    auto connectionIDStr = vec->at(0);
    auto connectionID = static_cast<uint>(atoi(connectionIDStr.c_str()));
    auto publicIP = vec->at(1);
    auto publicPort = vec->at(2);
    auto privateIP = vec->at(3);
    auto privatePort = vec->at(4);
    
    ijoon::RendezvousClient& client = ijoon::RendezvousClient::getInstance();
    
    bool isConnected = false;
    if(client.getCountConnectionFilter(connectionID) > 0) {
        isConnected = true;
    }
    
    if(!isConnected) {
        client.callback.onConnectingCallback(connectionID);
    }
    
    std::string data;
    
    auto publicPeer = ijoon::Peer(publicIP, publicPort);
    auto publicKcpPeer = client.getKcpPeer(publicPeer);
    
    data = connectionIDStr;
    data += seperator;
    data += "1";
    publicKcpPeer->send(ijoon::UDP_HOLE_PUNCHING_REQUEST,
                        (char *)data.c_str(),
                        data.length());
    
    auto privatePeer = ijoon::Peer(privateIP, privatePort);
    auto privateKcpPeer = client.getKcpPeer(privatePeer);
    
    data = connectionIDStr;
    data += seperator;
    data += "0";
    privateKcpPeer->send(ijoon::UDP_HOLE_PUNCHING_REQUEST, (char *)data.c_str(), data.length());
}



void renc::onUdpHolePunchingRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
//    ijn_print(DP_DEBUG, "from %s", kcpPeer->getPeer().getKey().c_str());

    std::string bodyStr = (char *)buffer;
    auto vec = ijoon::paramParser((char *)buffer, 2);
    if(vec == nullptr) return;
    //auto connectionIDStr = vec->at(0);
    //auto connectionID = static_cast<uint>(atoi(connectionIDStr.c_str()));
    //auto isPublicStr = vec->at(1);

    kcpPeer->send(ijoon::UDP_HOLE_PUNCHING_RESPONSE,
                  (char *)bodyStr.c_str(),
                  bodyStr.length());
}

void renc::onUdpHolePunchingResponse(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
//    ijn_print(DP_DEBUG, "from %s", kcpPeer->getPeer().getKey().c_str());

    // Connected by hole punching or local

    auto vec = ijoon::paramParser((char *)buffer, 2);
    if(vec == nullptr) return;
    auto connectionID = static_cast<uint>(atoi(vec->at(0).c_str()));
    const bool isPublic = atoi(vec->at(1).c_str()) ? true : false;

    kcpPeer->type = ijoon::PeerType::RENDEZVOUS_CLIENT;
    kcpPeer->status = ijoon::PeerStatus::REGISTERED;
    kcpPeer->connectionID = connectionID;

    ijoon::RendezvousClient& client = ijoon::RendezvousClient::getInstance();
    
    bool isConnected = false;
    if(client.getCountConnectionFilter(connectionID) > 0) {
        isConnected = true;
    }

    client.addConnectionFilter(connectionID, kcpPeer->getPeer());
    
    if(isPublic) {
        // public connection (hole punching)
        printf("connected by udp hole punching from %s (connectionID=%d)\n", kcpPeer->getPeer().getKey().c_str(), connectionID);
    }
    else {
        // private connection (equal net)
        printf("connected under equal nat from %s (connectionID=%d)\n", kcpPeer->getPeer().getKey().c_str(), connectionID);
    }

    if(!isConnected) {
        client.callback.onConnectedCallback(kcpPeer);
    }
    //TODO: onConnectionUpdate() 추가
}

void renc::onConnectionTargetInvalid(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
//    ijn_print(DP_DEBUG, "from %s", kcpPeer->getPeer().getKey().c_str());
    
    auto vec = ijoon::paramParser((char *)buffer, 2);
    if(vec == nullptr) return;
    std::string targetIP = vec->at(0);
    std::string targetPort = vec->at(1);
    
    ijoon::RendezvousClient& client = ijoon::RendezvousClient::getInstance();
    client.callback.onConnectFailedCallback(targetIP, targetPort);
}

void renc::onPingRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
//    ijn_print(DP_INFO, "from %s", kcpPeer->getPeer().getKey().c_str());
    
    kcpPeer->send(ijoon::PING_RESPONSE);
}

void renc::onPingResponse(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
//    ijn_print(DP_INFO, "from %s", kcpPeer->getPeer().getKey().c_str());
}
