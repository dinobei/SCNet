#include "rendezvous_server_callback.h"
#include "rendezvous_server.h"

extern char seperator;

bool connection(ijoon::RendezvousServer &server, uint connectionID) {
    server.mutexForConnectionInfoMap.lock();
    auto connectionInfo = server.connectionInfoMap[connectionID];
    server.mutexForConnectionInfoMap.unlock();
    
    // SP/TP nat check
    bool isSPPublic = false;
    auto publicSP = connectionInfo->publicSP;
    auto privateSP = connectionInfo->privateSP;
    if(publicSP->getIP().compare(privateSP->getIP()) == 0 &&
       publicSP->getPort() == privateSP->getPort() ) {
        isSPPublic = true;
    }
    
    bool isTPPublic = false;
    auto publicTP = connectionInfo->publicTP;
    auto privateTP = connectionInfo->privateTP;
    if(publicTP->getIP().compare(privateTP->getIP()) == 0 &&
       publicTP->getPort() == privateTP->getPort() ) {
        isTPPublic = true;
    }
    
    // get sp/tp kcp peer
    auto sourceKcpPeer = server.getKcpPeer(publicSP);
    auto targetKcpPeer = server.getKcpPeer(publicTP);
    
    if(isTPPublic) { // pub/pub, pri/pub
        std::string data = std::to_string(connectionID) +
                           seperator +
                           publicTP->getIP() +
                           seperator +
                           std::to_string(publicTP->getPort()); // tp public address
        sourceKcpPeer->send(ijoon::RENDEZVOUS_MSG::DIRECT_CONNECTION_AVAILABLE,
                            (char *)data.c_str(),
                            data.length());
    }
    else if(isSPPublic) { // pub/pri
        //TODO: REVERSE_CONNECTION_READY 프로토콜 삭제 검토
        
        std::string data;
        data = std::to_string(connectionID);
        sourceKcpPeer->send(ijoon::RENDEZVOUS_MSG::REVERSE_CONNECTION_READY,
        (char *)data.c_str(),
        data.length());
        
        data = std::to_string(connectionID) +
               seperator +
               publicSP->getIP() +
               seperator +
               std::to_string(publicSP->getPort()); // sp public address
        targetKcpPeer->send(ijoon::RENDEZVOUS_MSG::REVERSE_CONNECTION,
                            (char *)data.c_str(),
                            data.length());
    }
    else { // pri/pri
        std::string data;
        
        data = std::to_string(connectionID);
        data += seperator;
        data += publicTP->getIP(); // tp public address
        data += seperator;
        data += std::to_string(publicTP->getPort());
        data += seperator;
        data += privateTP->getIP();
        data += seperator;
        data += std::to_string(privateTP->getPort());
        sourceKcpPeer->send(ijoon::RENDEZVOUS_MSG::UDP_HOLE_PUNCHING_AVAILABLE,
                            (char *)data.c_str(),
                            data.length());
        
        data = std::to_string(connectionID);
        data += seperator;
        data += publicSP->getIP(); // sp public address
        data += seperator;
        data += std::to_string(publicSP->getPort());
        data += seperator;
        data += privateSP->getIP();
        data += seperator;
        data += std::to_string(privateSP->getPort());
        targetKcpPeer->send(ijoon::RENDEZVOUS_MSG::UDP_HOLE_PUNCHING_AVAILABLE,
                            (char *)data.c_str(),
                            data.length());
    }
    
    server.mutexForConnectionInfoMap.lock();
    server.connectionInfoMap.erase(connectionID);
    server.mutexForConnectionInfoMap.unlock();
    return true;
}

void rens::onRegistrationRendezvousClientRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
    ijn_print(DP_INFO, "from %s", kcpPeer->getPeer().getKey().c_str());
    
    auto vec = ijoon::paramParser((char *)buffer, 5);
    if(vec == nullptr) return;
    
    // Callback to user (serial, public address, private address, MAC, version)
    
    std::string localIP = vec->at(0);
    std::string localPort = vec->at(1);
    std::string serial = vec->at(2);
    std::string mac = vec->at(3);
    std::string version = vec->at(4);
    
    ijoon::RendezvousServer& server = ijoon::RendezvousServer::getInstance();
    ijoon::Peer peer = kcpPeer->getPeer();
    
    if(server.callback.registerRendezvousClientCallback(serial, peer.getIP(), std::to_string(peer.getPort()), localIP, localPort, mac, version)) {
        ijn_print(DP_INFO, "Registered client info: private=%s:%s, public=%s, serial=%s, mac=%s, version=%s",
                  localIP.c_str(), localPort.c_str(), peer.getKey().c_str(),
                  serial.c_str(), mac.c_str(), version.c_str());
        
        kcpPeer->type = ijoon::PeerType::RENDEZVOUS_CLIENT;
        kcpPeer->status = ijoon::PeerStatus::REGISTERED;
        
        //TODO: 버전에 따라 status 할당 (0, -1, -2)
        
        std::string data;
        data = "1";
        data += seperator;
        data += peer.getIP();
        data += seperator;
        data += std::to_string(peer.getPort());
        kcpPeer->send(ijoon::REGISTRATION_RENDEZVOUS_CLIENT_RESPONSE,
                      (char *)data.c_str(),
                      data.length());
    }
    else {
        ijn_print(DP_INFO, "Failed registration rendezvous client");
        
        std::string data;
        data = "-3";
        data += seperator;
        data += peer.getIP();
        data += seperator;
        data += std::to_string(peer.getPort());
        kcpPeer->send(ijoon::REGISTRATION_RENDEZVOUS_CLIENT_RESPONSE,
                      (char *)data.c_str(),
                      data.length());
    }
}

void rens::onRegistrationRelayServerRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
    ijn_print(DP_INFO, "from %s", kcpPeer->getPeer().getKey().c_str());
    
    kcpPeer->type = ijoon::PeerType::RELAY_SERVER;
    kcpPeer->status = ijoon::PeerStatus::REGISTERED;
    
    ijoon::RendezvousServer& server = ijoon::RendezvousServer::getInstance();
    
    if(server.callback.registerRelayServerCallback("unknown", kcpPeer->getPeer().getIP(), std::to_string(kcpPeer->getPeer().getPort()), "0.1")) {
        std::string data;
        data = "1";
        kcpPeer->send(ijoon::REGISTRATION_RELAY_SERVER_RESPONSE,
                      (char *)data.c_str(),
                      data.length());
    }
    else {
        ijn_print(DP_INFO, "Failed registration relay server");
        
        std::string data;
        data = "-3";
        kcpPeer->send(ijoon::REGISTRATION_RELAY_SERVER_RESPONSE,
                      (char *)data.c_str(),
                      data.length());
    }
}

void rens::onRelaySessionReady(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
    ijn_print(DP_INFO, "from %s", kcpPeer->getPeer().getKey().c_str());
    
    auto vec = ijoon::paramParser((char *)buffer, 1);
    if(vec == nullptr) return;
    auto connectionIDStr = vec->at(0);
    auto connectionID = static_cast<uint>(atoi(connectionIDStr.c_str()));
    
    ijoon::RendezvousServer& server = ijoon::RendezvousServer::getInstance();
    ijoon::Peer peer = kcpPeer->getPeer();
    
    server.mutexForConnectionInfoMap.lock();
    if(server.connectionInfoMap.count(connectionID) == 0) {
        ijn_print(DP_ERROR, "[RELAY_SESSION_READY] invalid request from relay server");
        server.mutexForConnectionInfoMap.unlock();
        return;
    }
    
    auto connectionInfo = server.connectionInfoMap[connectionID];
    server.mutexForConnectionInfoMap.unlock();
    
    auto sourceKcpPeer = server.getKcpPeer(connectionInfo->publicSP);
    auto targetKcpPeer = server.getKcpPeer(connectionInfo->publicTP);
    
    std::string data;
    data = connectionIDStr + seperator + peer.getIP() + seperator + std::to_string(peer.getPort()) + seperator + "1";
    sourceKcpPeer->send(ijoon::RELAY_SERVER_INFORMATION,
                  (char *)data.c_str(),
                  data.length());
    
    data = connectionIDStr + seperator + peer.getIP() + seperator + std::to_string(peer.getPort()) + seperator + "0";
    targetKcpPeer->send(ijoon::RELAY_SERVER_INFORMATION,
                  (char *)data.c_str(),
                  data.length());
}

void rens::onRelaySessionCreated(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
    ijn_print(DP_INFO, "from %s", kcpPeer->getPeer().getKey().c_str());
    
    auto vec = ijoon::paramParser((char *)buffer, 1);
    if(vec == nullptr) return;
    auto connectionIDStr = vec->at(0);
    auto connectionID = static_cast<uint>(atoi(connectionIDStr.c_str()));
    
    ijoon::RendezvousServer& server = ijoon::RendezvousServer::getInstance();
    ijoon::Peer peer = kcpPeer->getPeer();
    
    server.mutexForConnectionInfoMap.lock();
    if(server.connectionInfoMap.count(connectionID) == 0) {
        ijn_print(DP_ERROR, "[RELAY_SESSION_CREATED] relay connection info not exist, %d", connectionID);
        server.mutexForConnectionInfoMap.unlock();
        return;
    }
    auto connectionInfo = server.connectionInfoMap[connectionID];
    server.mutexForConnectionInfoMap.unlock();
    
    auto sourceKcpPeer = server.getKcpPeer(connectionInfo->publicSP);
    auto targetKcpPeer = server.getKcpPeer(connectionInfo->publicTP);
    
    // send connected packet
    std::string data = std::string("1") + seperator + connectionIDStr + seperator + peer.getIP() + seperator + std::to_string(peer.getPort());
    
    sourceKcpPeer->send(ijoon::CONNECTION_RELAY_SERVICE_RESULT,
                  (char *)data.c_str(),
                  data.length());
    targetKcpPeer->send(ijoon::CONNECTION_RELAY_SERVICE_RESULT,
                  (char *)data.c_str(),
                  data.length());
    
    connection(server, connectionID);
}

void rens::onConnectionRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
    ijn_print(DP_INFO, "from %s", kcpPeer->getPeer().getKey().c_str());

    std::string bodyStr = (char *)buffer;
    auto vec = ijoon::paramParser((char *)buffer, 2);
    if(vec == nullptr) return;
    std::string targetIP = vec->at(0);
    std::string targetPort = vec->at(1);
    
    ijoon::RendezvousServer& server = ijoon::RendezvousServer::getInstance();
    ijoon::Peer peer = kcpPeer->getPeer();
    
    auto sourcePrivatePeer = server.callback.getRendezvousClientPeerCallback(peer.getIP(), std::to_string(peer.getPort()));
    auto targetPrivatePeer = server.callback.getRendezvousClientPeerCallback(targetIP, targetPort);
    auto targetPublicPeer = std::shared_ptr<ijoon::Peer>(new ijoon::Peer(targetIP, targetPort));
    if(peer.getKey() == targetPublicPeer->getKey() ||
       sourcePrivatePeer == nullptr ||
       targetPrivatePeer == nullptr) {
        kcpPeer->send(ijoon::CONNECTION_TARGET_INVALID,
                            (char *)bodyStr.c_str(),
                            bodyStr.size());
        return;
    }
    
    // create connectionID
    if(server.connectionIDCursor > 1000000000) {
        server.connectionIDCursor = 1;
    }
    int connectionID = server.connectionIDCursor++;
    
    auto connectionInfo = std::shared_ptr<ijoon::ConnectionInfo>(new ijoon::ConnectionInfo());
    connectionInfo->connectionID = connectionID;
    connectionInfo->publicSP = std::shared_ptr<ijoon::Peer>(new ijoon::Peer(peer.getIP(), std::to_string(peer.getPort())));
    connectionInfo->privateSP = sourcePrivatePeer;
    connectionInfo->publicTP = targetPublicPeer;
    connectionInfo->privateTP = targetPrivatePeer;
    server.mutexForConnectionInfoMap.lock();
    server.connectionInfoMap[connectionID] = connectionInfo;
    server.mutexForConnectionInfoMap.unlock();
    
    std::string data = std::to_string(connectionID) + seperator + bodyStr;
    kcpPeer->send(ijoon::CONNECTION_ID_CREATED,
                        (char *)data.c_str(),
                        data.size());
}

void rens::onConnectionIdReceived(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
    ijn_print(DP_INFO, "from %s", kcpPeer->getPeer().getKey().c_str());
    
    auto vec = ijoon::paramParser((char *)buffer, 1);
    if(vec == nullptr) return;
    auto connectionIDStr = vec->at(0);
    auto connectionID = static_cast<uint>(atoi(connectionIDStr.c_str()));
    
    ijoon::RendezvousServer& server = ijoon::RendezvousServer::getInstance();
    ijoon::Peer peer = kcpPeer->getPeer();
    
    auto relayServerPeer = server.callback.getRelayServerPeerCallback();
    if(relayServerPeer == nullptr) {
        ijn_print(DP_INFO, "[CONNECTION_ID_RECEIVED] no relay server");
        std::string data;
        data = std::string("0") +
               seperator +
               connectionIDStr +
               seperator +
               "0.0.0.0" +
               seperator +
               "0";
        kcpPeer->send(ijoon::CONNECTION_RELAY_SERVICE_RESULT,
                      (char *)data.c_str(),
                      data.length());
        connection(server, connectionID);
        return;
    }
    
    server.mutexForConnectionInfoMap.lock();
    if(server.connectionInfoMap.count(connectionID) == 0) {
        ijn_print(DP_ERROR, "[CONNECTION_ID_RECEIVED] relay connection info not exist, %d", connectionID);
        server.mutexForConnectionInfoMap.unlock();
        return;
    }
    auto connectionInfo = server.connectionInfoMap[connectionID];
    server.mutexForConnectionInfoMap.unlock();
    
    auto relayKcpPeer = server.getKcpPeer(relayServerPeer);

    std::string data;
    data += connectionIDStr;
    data += seperator;
    data += peer.getIP();
    data += seperator;
    data += connectionInfo->publicTP->getIP().c_str();
    
    relayKcpPeer->send(ijoon::RELAY_SERVICE_REQUEST,
                        (char *)data.c_str(),
                        data.size());
}

void rens::onPingRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
//    ijn_print(DP_INFO, "from %s", kcpPeer->getPeer().getKey().c_str());
    
    kcpPeer->send(ijoon::PING_RESPONSE);
}

void rens::onUnregistrationRendezvousClientRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
    ijn_print(DP_INFO, "from %s", kcpPeer->getPeer().getKey().c_str());
    
    ijoon::RendezvousServer& server = ijoon::RendezvousServer::getInstance();
    ijoon::Peer peer = kcpPeer->getPeer();
    
    server.mutexForKcpPeerMap.lock();
    server.kcpPeerMap.erase(peer.getKey());
    server.mutexForKcpPeerMap.unlock();
    
    kcpPeer->status = ijoon::PeerStatus::UNREGISTERED;
}

void rens::onUnregistrationRelayServerRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
    ijn_print(DP_INFO, "from %s", kcpPeer->getPeer().getKey().c_str());
    
    ijoon::RendezvousServer& server = ijoon::RendezvousServer::getInstance();
    ijoon::Peer peer = kcpPeer->getPeer();
    
    server.mutexForKcpPeerMap.lock();
    server.kcpPeerMap.erase(peer.getKey());
    server.mutexForKcpPeerMap.unlock();
    
    kcpPeer->status = ijoon::PeerStatus::UNREGISTERED;
}
