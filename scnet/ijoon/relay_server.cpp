#include "relay_server.h"
#include "randezvous_message.h"
#include "message_header.h"

extern char seperator;

void ijoon::RelayServer::start() {
    ijn_print(DP_INFO, "Relay server start...");

    mainThread = new ijoon::Thread(ijoon::mainThread, "relay thread");
    mainThread->start(this);
    registerThread = new ijoon::Thread(ijoon::registerThread, "relay register thread");
    registerThread->start(this);
}

ijoon::THREAD_RET THREAD_API ijoon::registerThread(void *arg) {
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::RelayServer *relayServer = (ijoon::RelayServer *)thread->getParam();
    
    while(!thread->isInterrupted()) {
        // 주기적으로 regist packet send
        
        ijn_print(DP_INFO, "sent REGISTER_RELAY_REQUEST");
        ijoon::send(relayServer->socket, relayServer->randezvousPeer, 0, REGISTER_RELAY_REQUEST);
        thread->sleep(30 * 1000);
    }
    
    return THREAD_EXIT;
}

bool validationPeer(std::map<int, std::shared_ptr<ijoon::RelayPeerInfo>> map, std::map<int, int> sessionCheckMap, uint connectionID, ijoon::Peer peer, bool &isSP) {
    
    // exist in map?
    if(map.count(connectionID) == 0) {
        return false;
    }
    // checked session?
    if(sessionCheckMap.count(connectionID) != 0) {
        return false;
    }
    auto relayPeerInfo = map[connectionID];
    
    if( (peer.getIP().compare(relayPeerInfo->sourcePeer.getIP()) == 0) && (peer.getPort() == relayPeerInfo->sourcePeer.getPort()) ) { // sender is SP
        isSP = true;
        return true;
    }
    if( (peer.getIP().compare(relayPeerInfo->targetPeer.getIP()) == 0) && (peer.getPort() == relayPeerInfo->targetPeer.getPort()) ) { // sender is TP
        isSP = false;
        return true;
    }
    
    return false;
}

ijoon::THREAD_RET THREAD_API ijoon::mainThread(void *arg) {
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::RelayServer *relayServer = (ijoon::RelayServer *)thread->getParam();
    
    char *packet = new char[MAX_PACKET_SIZE];
    ijoon::Peer peer;
    
    while(!thread->isInterrupted()) {
        memset(packet, 0, MAX_PACKET_SIZE);
        ssize_t recvSize = relayServer->socket->recvFrom(&peer, packet, MAX_PACKET_SIZE);
        if(recvSize < 0) {
            continue;
        }
        
        MessageHeader messageHeader;
        int cursor = 0;
        if(!ijoon::readHeader(packet, recvSize, messageHeader, cursor)) {
            continue;
        }
        
        if((recvSize-cursor) != messageHeader.dataSize) {
            continue;
        }
        
        char *body = &packet[cursor];
        
        if(messageHeader.messageType == RAWBYTE_RELAY) {
            // validation peer
            bool isSP = false;
            if(!validationPeer(relayServer->map, relayServer->sessionCheckMap, messageHeader.connectionID, peer, isSP)) {
                ijn_print(DP_ERROR, "Invalid peer's relay packet");
                continue;
            }
            
            auto relayPeerInfo = relayServer->map[messageHeader.connectionID];
            ijoon::send(relayServer->socket, isSP ? relayPeerInfo->targetPeer : relayPeerInfo->sourcePeer,
                        messageHeader.connectionID, messageHeader.packetType, body, messageHeader.dataSize);
            continue;
        }
        
        
        if(messageHeader.messageType != MESSAGE_TYPE::RAWBYTE) {
            ijn_print(DP_ERROR, "Randezvous server only accept raw packet for randezvous");
            continue;
        }
        
        switch (messageHeader.packetType) {
            case RELAY_SERVICE_REQUEST: // from RanS
            {
                ijn_print(DP_DEBUG, "received RELAY_SERVICE_REQUEST");

                std::vector<std::string> vec;
                char *token = std::strtok(body, &seperator);
                while (token != NULL) {
                    vec.push_back(token);
                    token = std::strtok(NULL, &seperator);
                }
                if(vec.size() != 4) {
                    ijn_print(DP_ERROR, "[RELAY_SERVICE_REQUEST] invalid parameters");
                    break;
                }
                
                int connectionID = messageHeader.connectionID;
                if(relayServer->map.count(connectionID) != 0) {
                    ijn_print(DP_ERROR, "[RELAY_SERVICE_REQUEST] already registered");
                    continue;
                }
                
                auto relayPeerInfo = std::shared_ptr<RelayPeerInfo>(new RelayPeerInfo);
                relayPeerInfo->sourcePeer.setIP(vec[0]);
                relayPeerInfo->sourcePeer.setPort(vec[1]);
                relayPeerInfo->targetPeer.setIP(vec[2]);
                relayPeerInfo->targetPeer.setPort(vec[3]);
                relayServer->map[connectionID] = relayPeerInfo;
                
                relayServer->sessionCheckMap[connectionID] = 0;
                
                std::string data;
                data += vec[0];
                data += seperator;
                data += vec[1];
                data += seperator;
                data += vec[2];
                data += seperator;
                data += vec[3];
                
                ijoon::send(relayServer->socket, peer, messageHeader.connectionID, RELAY_SERVICE_READY, (char *)data.c_str(), data.length());
                
                continue;
            }
            case REGISTER_RELAY_RESPONSE: // from RanS
            {
                ijn_print(DP_DEBUG, "received REGISTER_RELAY_RESPONSE");
                continue;
            }
            case REGISTER_RELAY_PEER_REQUEST: // from SP, TP
            {
                ijn_print(DP_DEBUG, "received REGISTER_RELAY_PEER_REQUEST");
                
                std::vector<std::string> vec;
                char *token = std::strtok(body, &seperator);
                while (token != NULL) {
                    vec.push_back(token);
                    token = std::strtok(NULL, &seperator);
                }
                if(vec.size() != 2) {
                    ijn_print(DP_ERROR, "[REGISTER_RELAY_PEER_REQUEST] invalid parameters");
                    ijoon::send(relayServer->socket, peer, messageHeader.connectionID, REGISTER_RELAY_PEER_RESPONSE_FAILED, body, messageHeader.dataSize);
                    break;
                }
                
                std::string peer1IP = peer.getIP();
                int peer1Port = peer.getPort();
                std::string peer2IP = vec[0];
                int peer2Port = atoi(vec[1].c_str());
                
                if(relayServer->map.count(messageHeader.connectionID) == 0) {
                    ijn_print(DP_ERROR, "[REGISTER_RELAY_PEER_REQUEST] invalid connection id");
                    ijoon::send(relayServer->socket, peer, messageHeader.connectionID, REGISTER_RELAY_PEER_RESPONSE_FAILED, body, messageHeader.dataSize);
                    continue;
                }
                
                if(relayServer->sessionCheckMap.count(messageHeader.connectionID) == 0) {
                    ijn_print(DP_ERROR, "[REGISTER_RELAY_PEER_REQUEST] already checked peer");
                    ijoon::send(relayServer->socket, peer, messageHeader.connectionID, REGISTER_RELAY_PEER_RESPONSE_FAILED, body, messageHeader.dataSize);
                    continue;
                }
                
                auto relayPeerInfo = relayServer->map[messageHeader.connectionID];
                if(relayPeerInfo->sourcePeer.getIP().compare(peer1IP) == 0 && relayPeerInfo->sourcePeer.getPort() == peer1Port &&
                   relayPeerInfo->targetPeer.getIP().compare(peer2IP) == 0 && relayPeerInfo->targetPeer.getPort() == peer2Port) {
                    // peer1 is SP
                    relayServer->sessionCheckMap[messageHeader.connectionID]++;
                    ijoon::send(relayServer->socket, peer, messageHeader.connectionID, REGISTER_RELAY_PEER_RESPONSE_SUCCESS, body, messageHeader.dataSize);
                    ijn_print(DP_INFO, "[CID=%d] SP registered", messageHeader.connectionID);
                }
                else if(relayPeerInfo->sourcePeer.getIP().compare(peer2IP) == 0 && relayPeerInfo->sourcePeer.getPort() == peer2Port &&
                        relayPeerInfo->targetPeer.getIP().compare(peer1IP) == 0 && relayPeerInfo->targetPeer.getPort() == peer1Port) {
                    // peer2 is SP
                    relayServer->sessionCheckMap[messageHeader.connectionID]++;
                    ijoon::send(relayServer->socket, peer, messageHeader.connectionID, REGISTER_RELAY_PEER_RESPONSE_SUCCESS, body, messageHeader.dataSize);
                    ijn_print(DP_INFO, "[CID=%d] TP registered", messageHeader.connectionID);
                }
                else {
                    // not registered relay peer
                    ijn_print(DP_ERROR, "[REGISTER_RELAY_PEER_REQUEST] relay peer mismatch");
                    ijoon::send(relayServer->socket, peer, messageHeader.connectionID, REGISTER_RELAY_PEER_RESPONSE_FAILED, body, messageHeader.dataSize);
                    continue;
                }
                
                if(relayServer->sessionCheckMap[messageHeader.connectionID] >= 2) {
                    // successfully registerred
                    relayServer->sessionCheckMap.erase(messageHeader.connectionID);
                    
                    std::string data;
                    data = relayPeerInfo->sourcePeer.getIP();
                    data += seperator;
                    data += std::to_string(relayPeerInfo->sourcePeer.getPort());
                    data += seperator;
                    data += relayPeerInfo->targetPeer.getIP();
                    data += seperator;
                    data += std::to_string(relayPeerInfo->targetPeer.getPort());
                    ijoon::send(relayServer->socket, relayServer->randezvousPeer, messageHeader.connectionID, RELAY_SERVICE_RESPONSE_SUCCESS, (char *)data.c_str(), data.length());
                }
                
                continue;
            }
            default:
            {
                ijn_print(DP_ERROR, "Undefined message received");
                break;
            }
        }
        
    }
    
    delete[] packet;
    
    return THREAD_EXIT;
}
