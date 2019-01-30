#include "relay_server.h"
#include "rendezvous_message.h"
#include "message_header.h"
#include <cstring>

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
        ijoon::send(relayServer->socket, relayServer->rendezvousPeer, 0, REGISTRATION_RELAY_SERVER_REQUEST);
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
        
        if(messageHeader.messageType == RAWBYTE_RELAY || messageHeader.messageType == PROTOBUF_RELAY) {
            // validation peer
            bool isSP = false;
            if(!validationPeer(relayServer->map, relayServer->sessionCheckMap, messageHeader.connectionID, peer, isSP)) {
                ijn_print(DP_ERROR, "Invalid peer's relay packet");
                continue;
            }
            
            auto relayPeerInfo = relayServer->map[messageHeader.connectionID];
            messageHeader.messageType = messageHeader.messageType == RAWBYTE_RELAY ? RAWBYTE : PROTOBUF;
            ijoon::send(relayServer->socket, isSP ? relayPeerInfo->targetPeer : relayPeerInfo->sourcePeer, messageHeader, body);
            continue;
        }
        
        
        if(messageHeader.messageType != MESSAGE_TYPE::RAWBYTE) {
            ijn_print(DP_ERROR, "Rendezvous server only accept raw packet for rendezvous");
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
                if(vec.size() != 2) {
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
                relayPeerInfo->targetPeer.setIP(vec[1]);
                relayServer->map[connectionID] = relayPeerInfo;
                
                relayServer->sessionCheckMap[connectionID] = 0;
                
                ijoon::send(relayServer->socket, peer, messageHeader.connectionID, RELAY_SESSION_READY);
                
                continue;
            }
            case REGISTRATION_RELAY_SERVER_SUCCESS: // from RanS
            {
                ijn_print(DP_DEBUG, "received REGISTRATION_RELAY_SERVER_SUCCESS");
                continue;
            }
            case REGISTRATION_RELAY_PEER_REQUEST: // from SP, TP
            {
                ijn_print(DP_DEBUG, "received REGISTRATION_RELAY_PEER_REQUEST");
                
                std::vector<std::string> vec;
                char *token = std::strtok(body, &seperator);
                while (token != NULL) {
                    vec.push_back(token);
                    token = std::strtok(NULL, &seperator);
                }
                if(vec.size() != 1) {
                    ijn_print(DP_ERROR, "[REGISTRATION_RELAY_PEER_REQUEST] invalid parameters");
                    break;
                }
                
                std::string peerIP = peer.getIP();
                std::string peerPort = std::to_string(peer.getPort());
                
                if(relayServer->map.count(messageHeader.connectionID) == 0) {
                    ijn_print(DP_ERROR, "[REGISTRATION_RELAY_PEER_REQUEST] invalid connection id");
                    ijoon::send(relayServer->socket, peer, messageHeader.connectionID, REGISTRATION_RELAY_PEER_FAILED);
                    continue;
                }
                
                if(relayServer->sessionCheckMap.count(messageHeader.connectionID) == 0) {
                    ijn_print(DP_ERROR, "[REGISTRATION_RELAY_PEER_REQUEST] already checked peer");
                    ijoon::send(relayServer->socket, peer, messageHeader.connectionID, REGISTRATION_RELAY_PEER_FAILED);
                    continue;
                }
                
                bool isSP = atoi(vec[0].c_str()) ? true : false;
                auto relayPeerInfo = relayServer->map[messageHeader.connectionID];
                if(relayPeerInfo->sourcePeer.getIP().compare(peerIP) == 0 && isSP) {
                    // peer is SP
                    relayServer->map[messageHeader.connectionID]->sourcePeer.setPort(peerPort);
                    relayServer->sessionCheckMap[messageHeader.connectionID]++;
                    ijoon::send(relayServer->socket, peer, messageHeader.connectionID, REGISTRATION_RELAY_PEER_SUCCESS);
                    ijn_print(DP_INFO, "[CID=%d] SP registered", messageHeader.connectionID);
                }
                else if(relayPeerInfo->targetPeer.getIP().compare(peerIP) == 0 && !isSP) {
                    // peer is TP
                    relayServer->map[messageHeader.connectionID]->targetPeer.setPort(peerPort);
                    relayServer->sessionCheckMap[messageHeader.connectionID]++;
                    ijoon::send(relayServer->socket, peer, messageHeader.connectionID, REGISTRATION_RELAY_PEER_SUCCESS);
                    ijn_print(DP_INFO, "[CID=%d] TP registered", messageHeader.connectionID);
                }
                else {
                    // not registered relay peer
                    ijn_print(DP_ERROR, "[REGISTRATION_RELAY_PEER_REQUEST] relay peer mismatch");
                    ijoon::send(relayServer->socket, peer, messageHeader.connectionID, REGISTRATION_RELAY_PEER_FAILED);
                    continue;
                }
                
                if(relayServer->sessionCheckMap[messageHeader.connectionID] >= 2) {
                    // successfully registerred
                    relayServer->sessionCheckMap.erase(messageHeader.connectionID);
                    
                    ijoon::send(relayServer->socket, relayServer->rendezvousPeer, messageHeader.connectionID, RELAY_SESSION_CREATED);
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
