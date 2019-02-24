#include "relay_server.h"
#include "rendezvous_message.h"
#include "message_header.h"
#include <cstring>

extern char seperator;

int udp_output(const char *buf, int len, ikcpcb *kcp, void *user) {
//    ijn_print(DP_DEBUG, "udp_output len : %d.", len);
//    ijn_print(DP_DEBUG, "udp_output buf : %s.", buf);
    
    ijoon::KcpPeer *kcpPeer = (ijoon::KcpPeer *)user;
    ijoon::Peer peer = kcpPeer->getPeer();
    kcpPeer->getClientSocket()->sendTo(&peer, const_cast<char *>(buf), len);
    return 0;
}

void ijoon::RelayServer::start() {
    ijn_print(DP_INFO, "Relay server start...");

    rawRecvThread = new ijoon::Thread(ijoon::rawRecvThreadFunc, "raw recv thread");
    rawRecvThread->start(this);
    recvThread = new ijoon::Thread(ijoon::recvThreadFunc, "recv thread");
    recvThread->start(this);
    registerThread = new ijoon::Thread(ijoon::registerThread, "relay register thread");
    registerThread->start(this);
}

ijoon::THREAD_RET THREAD_API ijoon::registerThread(void *arg) {
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::RelayServer *relayServer = (ijoon::RelayServer *)thread->getParam();
    
    std::shared_ptr<ijoon::KcpPeer> serverKcpPeer = nullptr;
    if(relayServer->kcpPeerMap.count(relayServer->serverPeer.getKey()) == 0) {
        serverKcpPeer = std::shared_ptr<ijoon::KcpPeer>(
                                                        new ijoon::KcpPeer(relayServer->socket,
                                                                           relayServer->serverPeer.getIP(),
                                                                           std::to_string(relayServer->serverPeer.getPort()),
                                                                           udp_output)
                                                        );
        relayServer->kcpPeerMap[relayServer->serverPeer.getKey()] = serverKcpPeer;
    }
    else {
        serverKcpPeer = relayServer->kcpPeerMap.at(relayServer->serverPeer.getKey());
    }
    
    const int timeout = 3600;
    while(!thread->isInterrupted()) {
        // periodically send registration packet
        ijoon::send(serverKcpPeer->getKcp(), 0, REGISTRATION_RELAY_SERVER_REQUEST, nullptr, 0);
        thread->sleep(60 * 1000);
        
        // periodically check & erase peer
        time_t current = ijoon::ComputableTime::getCurrentTimeSec();
        auto iter = relayServer->kcpPeerMap.begin();
        auto end = relayServer->kcpPeerMap.end();
        while(iter != end) {
            int connectionID = iter->second->getConnectionID();
            if(connectionID != 0 && iter->second->lastPing + timeout < current) {
                if(relayServer->map.count(connectionID) != 0) {
                    relayServer->map.erase(connectionID);
                }
                iter = relayServer->kcpPeerMap.erase(iter);
                ijn_print(DP_INFO, "kcpPeers removed, connectionID=%d", connectionID);
            }
            else {
                ++iter;
            }
        }
    }
    
    return THREAD_EXIT;
}

bool validationPeer(std::map<int, std::shared_ptr<ijoon::RelayPeerInfo>> map, std::map<int, int> sessionCheckMap, uint connectionID, ijoon::Peer peer, bool &isSP) {
    
    // does it exist in map
    if(map.count(connectionID) == 0) {
        return false;
    }
    
    // does it checked session
    if(sessionCheckMap.count(connectionID) != 0) {
        return false;
    }
    
    auto relayPeerInfo = map[connectionID];
    
    if( (peer.getIP().compare(relayPeerInfo->sourceKcpPeer->getPeer().getIP()) == 0) && (peer.getPort() == relayPeerInfo->sourceKcpPeer->getPeer().getPort()) ) { // sender is SP
        isSP = true;
        return true;
    }
    if( (peer.getIP().compare(relayPeerInfo->targetKcpPeer->getPeer().getIP()) == 0) && (peer.getPort() == relayPeerInfo->targetKcpPeer->getPeer().getPort()) ) { // sender is TP
        isSP = false;
        return true;
    }
    
    return false;
}

/* get system time */
void itimeofday(long *sec, long *usec)
{
    struct timeval time;
    gettimeofday(&time, NULL);
    if (sec) *sec = time.tv_sec;
    if (usec) *usec = time.tv_usec;
}

/* get clock in millisecond 64 */
IINT64 iclock64(void)
{
    long s, u;
    IINT64 value;
    itimeofday(&s, &u);
    value = ((IINT64)s) * 1000 + (u / 1000);
    return value;
}

IUINT32 iclock()
{
    return (IUINT32)(iclock64() & 0xfffffffful);
}

void onCallback(ijoon::RelayServer *relayServer, std::shared_ptr<ijoon::KcpPeer> kcpPeer, char *packet, int recvSize) {
    ijoon::Peer peer = kcpPeer->getPeer();
    ijoon::MessageHeader messageHeader;
    int cursor = 0;
    if(!ijoon::readHeader(packet, recvSize, messageHeader, cursor)) {
        return;
    }
    
    if((recvSize-cursor) != messageHeader.dataSize) {
        return;
    }
    
    char *body = &packet[cursor];
    kcpPeer->setConnectionID(messageHeader.connectionID);
    kcpPeer->lastPing = ijoon::ComputableTime::getCurrentTimeSec();
    
    if(messageHeader.messageType == ijoon::RAWBYTE_RELAY || messageHeader.messageType == ijoon::PROTOBUF_RELAY) {
        bool isSP = false;
        if(!validationPeer(relayServer->map, relayServer->sessionCheckMap, messageHeader.connectionID, peer, isSP)) {
            ijn_print(DP_ERROR, "Invalid peer's relay packet");
            return;
        }
        
        auto relayPeerInfo = relayServer->map[messageHeader.connectionID];
        messageHeader.messageType = messageHeader.messageType == ijoon::RAWBYTE_RELAY ? ijoon::RAWBYTE : ijoon::PROTOBUF;
        ijoon::send(isSP ? relayPeerInfo->targetKcpPeer->getKcp() : relayPeerInfo->sourceKcpPeer->getKcp(),
                    messageHeader.connectionID,
                    messageHeader.packetType,
                    packet, recvSize);
        return;
    }
    
    
    if(messageHeader.messageType != ijoon::MESSAGE_TYPE::RAWBYTE) {
        ijn_print(DP_ERROR, "Rendezvous server only accept raw packet for rendezvous");
        return;
    }
    
    switch (messageHeader.packetType) {
        case ijoon::RELAY_SERVICE_REQUEST: // from RanS
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
                return;
            }
            
            auto relayPeerInfo = std::shared_ptr<ijoon::RelayPeerInfo>(new ijoon::RelayPeerInfo());

            relayServer->map[connectionID] = relayPeerInfo;
            
            relayServer->sessionCheckMap[connectionID] = 0;
            
            ijoon::send(kcpPeer->getKcp(), connectionID, ijoon::RELAY_SESSION_READY, nullptr, 0);
            
            return;
        }
        case ijoon::REGISTRATION_RELAY_SERVER_SUCCESS: // from RanS
        {
            ijn_print(DP_DEBUG, "received REGISTRATION_RELAY_SERVER_SUCCESS");
            return;
        }
        case ijoon::REGISTRATION_RELAY_PEER_REQUEST: // from SP, TP
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
                ijoon::send(kcpPeer->getKcp(), kcpPeer->getConnectionID(), ijoon::REGISTRATION_RELAY_PEER_FAILED, nullptr, 0);
                return;
            }
            
            if(relayServer->sessionCheckMap.count(messageHeader.connectionID) == 0) {
                ijn_print(DP_ERROR, "[REGISTRATION_RELAY_PEER_REQUEST] already checked peer");
                ijoon::send(kcpPeer->getKcp(), kcpPeer->getConnectionID(), ijoon::REGISTRATION_RELAY_PEER_FAILED, nullptr, 0);
                return;
            }
            
            bool isSP = atoi(vec[0].c_str()) ? true : false;
            if(isSP) {
                relayServer->map[messageHeader.connectionID]->sourceKcpPeer = kcpPeer;
                relayServer->sessionCheckMap[messageHeader.connectionID]++;
            }
            else {
                relayServer->map[messageHeader.connectionID]->targetKcpPeer = kcpPeer;
                relayServer->sessionCheckMap[messageHeader.connectionID]++;
            }
            
            if(relayServer->sessionCheckMap[messageHeader.connectionID] >= 2) {
                // successfully registerred
                relayServer->sessionCheckMap.erase(messageHeader.connectionID);
                
                std::shared_ptr<ijoon::KcpPeer> serverKcpPeer;
                if(relayServer->kcpPeerMap.count(relayServer->serverPeer.getKey()) == 0) {
                    serverKcpPeer = std::shared_ptr<ijoon::KcpPeer>(new ijoon::KcpPeer(relayServer->socket,
                                                                                       relayServer->serverPeer.getIP(),
                                                                                       std::to_string(relayServer->serverPeer.getPort()), udp_output)
                                                                    );
                    relayServer->kcpPeerMap[relayServer->serverPeer.getKey()] = serverKcpPeer;
                }
                else {
                    serverKcpPeer = relayServer->kcpPeerMap[relayServer->serverPeer.getKey()];
                }
                serverKcpPeer->setConnectionID(messageHeader.connectionID);
                
                ijoon::send(serverKcpPeer->getKcp(), serverKcpPeer->getConnectionID(), ijoon::RELAY_SESSION_CREATED, nullptr, 0);
            }
            
            return;
        }
        default:
        {
            ijn_print(DP_ERROR, "Undefined message received");
            break;
        }
    }
}

ijoon::THREAD_RET THREAD_API ijoon::rawRecvThreadFunc(void *arg) {
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::RelayServer *relayServer = (ijoon::RelayServer *)thread->getParam();
    
    ijoon::Peer peer;
    
    char *buffer = new char[MAX_PACKET_SIZE];
    
    while(!thread->isInterrupted()) {
        int rcvSize = relayServer->socket->recvFrom(&peer, buffer, MAX_PACKET_SIZE);
        if(rcvSize < 0) continue;
        
        std::shared_ptr<ijoon::KcpPeer> kcpPeer = nullptr;
        if(relayServer->kcpPeerMap.count(peer.getKey()) == 0) {
            kcpPeer = std::shared_ptr<ijoon::KcpPeer>(new ijoon::KcpPeer(relayServer->socket, peer.getIP(), std::to_string(peer.getPort()), udp_output));
            relayServer->kcpPeerMap[peer.getKey()] = kcpPeer;
        }
        else {
            kcpPeer = relayServer->kcpPeerMap.at(peer.getKey());
        }
        
        kcpPeer->mutex.lock();
        ikcpcb *kcp = kcpPeer->getKcp();
        ikcp_input(kcp, buffer, rcvSize);
        IUINT32 current = iclock();
        ikcp_update(kcp, current);
        kcpPeer->next = ikcp_check(kcp, current);
        kcpPeer->mutex.unlock();
    }
    
    delete[] buffer;
    
    return THREAD_EXIT;
}

void updateKcpObject(ijoon::RelayServer *relayServer, std::shared_ptr<ijoon::KcpPeer> kcpPeer) {
    char *buffer = new char[MAX_PACKET_SIZE];
    
    IUINT32 current = iclock();
    if(current >= kcpPeer->next) {
        kcpPeer->mutex.lock();
        ikcpcb *kcp = kcpPeer->getKcp();
        int rcvSize = ikcp_recv(kcp, buffer, MAX_PACKET_SIZE);
        ikcp_update(kcp, current);
        kcpPeer->next = ikcp_check(kcp, current);
        kcpPeer->mutex.unlock();
        
        if(rcvSize > 0) {
            // callback to upper users
            buffer[rcvSize] = '\0';
            onCallback(relayServer, kcpPeer, buffer, rcvSize);
        }
    }
    
    delete []buffer;
}

ijoon::THREAD_RET THREAD_API ijoon::recvThreadFunc(void *arg) {
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::RelayServer *relayServer = (ijoon::RelayServer *)thread->getParam();
    
    while(!thread->isInterrupted()) {
        
        auto iter = relayServer->kcpPeerMap.begin();
        for(; iter != relayServer->kcpPeerMap.end() ; ++iter) {
            updateKcpObject(relayServer, iter->second);
        }
        
        ijn_msleep(10);
    }
    
    return THREAD_EXIT;
}
