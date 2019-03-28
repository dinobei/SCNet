#include "relay_server.h"
#include "rendezvous_message.h"
#include "message_header.h"
#include <cstring>
#include "registry.h"

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

    recvThread = new ijoon::Thread(ijoon::recvThreadFunc, "recv thread");
    recvThread->start(this);
    rawRecvThread = new ijoon::Thread(ijoon::rawRecvThreadFunc, "raw recv thread");
    rawRecvThread->start(this);
    registerThread = new ijoon::Thread(ijoon::registerThread, "relay register thread");
    registerThread->start(this);
}

std::shared_ptr<ijoon::KcpPeer> ijoon::RelayServer::getKcpPeer(std::shared_ptr<ijoon::Peer> peer) {
    std::shared_ptr<ijoon::KcpPeer> kcpPeer;
    if(this->kcpPeerMap.count(peer->getKey()) == 0) {
        kcpPeer = std::shared_ptr<ijoon::KcpPeer>(new ijoon::KcpPeer(this->socket, peer->getIP(), std::to_string(peer->getPort()), udp_output));
        this->kcpPeerMap[peer->getKey()] = kcpPeer;
    }
    else {
        kcpPeer = this->kcpPeerMap.at(peer->getKey());
    }
    
    return kcpPeer;
}

ijoon::THREAD_RET THREAD_API ijoon::registerThread(void *arg) {
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::RelayServer *relayServer = (ijoon::RelayServer *)thread->getParam();
    
    auto serverKcpPeer = relayServer->getKcpPeer(relayServer->serverPeer);
    ijoon::send(serverKcpPeer, 0, REGISTRATION_RELAY_SERVER_REQUEST, nullptr, 0);
    
    const int timeout = 60;
    const int pingIntervalSec = 30;
    const int loopIntervalMs = 5 * 1000;
    while(!thread->isInterrupted()) {
        thread->sleep(loopIntervalMs);
        
        time_t current = ijoon::ComputableTime::getCurrentTimeSec();
        
        // periodically send registration packet
        if(relayServer->lastRegistrationTime + pingIntervalSec < current) {
            ijoon::send(serverKcpPeer, 0, REGISTRATION_RELAY_SERVER_REQUEST, nullptr, 0);
        }
        
        // periodically check & erase peer
        {
            relayServer->mutexForKcpPeerMap.lock();
            auto iter = relayServer->kcpPeerMap.begin();
            auto end = relayServer->kcpPeerMap.end();
            while(iter != end) {
                auto kcpPeer = iter->second;
                if(kcpPeer->lastPing + timeout < current) {
                    ijn_print(DP_INFO, "removed kcpPeer: %s", iter->second->getPeer().getKey().c_str());
                    iter = relayServer->kcpPeerMap.erase(iter);
                    continue;
                }
                else if(kcpPeer->lastPing + pingIntervalSec < current) {
                    ijoon::send(iter->second, 0, PING_REQUEST, nullptr, 0);
                }
                
                ++iter;
            }
            relayServer->mutexForKcpPeerMap.unlock();
        }
        
        {
            auto iter = relayServer->map.begin();
            auto end = relayServer->map.end();
            while(iter != end) {
                if( (iter->second->sourceKcpPeer != nullptr && iter->second->sourceKcpPeer->lastPing + timeout < current) ||
                   (iter->second->targetKcpPeer != nullptr && iter->second->targetKcpPeer->lastPing + timeout < current) ||
                   (iter->second->sourceKcpPeer == nullptr && iter->second->targetKcpPeer == nullptr) ) {
                    int connectionID = iter->first;
                    iter = relayServer->map.erase(iter);
                    ijn_print(DP_INFO, "removed connectionInfo, connectionID=%d", connectionID);
                }
                else {
                    ++iter;
                }
            }
        }
    }
    
    return THREAD_EXIT;
}

bool validationPeer(std::map<int, std::shared_ptr<ijoon::RelayPeerInfo>> map, std::map<int, int> sessionCheckMap, uint connectionID, ijoon::Peer peer, bool &isSP) {
    
    // does it exist in map
    if(map.count(connectionID) == 0) {
        ijn_print(DP_ERROR, "validation failed, connectionID not exist");
        return false;
    }
    
    // does it checked session
    if(sessionCheckMap.count(connectionID) != 0) {
        ijn_print(DP_ERROR, "validation failed, connection not checked");
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
    
    ijn_print(DP_ERROR, "validation failed, invalid target peer info");
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
    
    kcpPeer->lastPing = ijoon::ComputableTime::getCurrentTimeSec();
    if(messageHeader.messageType == ijoon::RAWBYTE_RELAY || messageHeader.messageType == ijoon::PROTOBUF_RELAY) {
        bool isSP = false;
        if(!validationPeer(relayServer->map, relayServer->sessionCheckMap, messageHeader.connectionID, peer, isSP)) {
            ijn_print(DP_ERROR, "Invalid peer's relay packet");
            ijoon::send(kcpPeer, messageHeader.connectionID, ijoon::RELAY_SESSION_INVALID, nullptr, 0);
            return;
        }
        
        auto relayPeerInfo = relayServer->map[messageHeader.connectionID];
        messageHeader.messageType = messageHeader.messageType == ijoon::RAWBYTE_RELAY ? ijoon::RAWBYTE : ijoon::PROTOBUF;
        if(!ijoon::sendRelayPacket(isSP ? relayPeerInfo->targetKcpPeer : relayPeerInfo->sourceKcpPeer,
                    messageHeader.connectionID,
                    messageHeader.messageType,
                    messageHeader.packetType,
                                   body, messageHeader.dataSize)) {
            ijn_print(DP_ERROR, "Invalid peer's relay packet (exceed waitsnd)");
            
            relayServer->map.erase(messageHeader.connectionID);
            ijn_print(DP_INFO, "removed connectionInfo, connectionID=%d", messageHeader.connectionID);
            
            ijoon::send(kcpPeer, messageHeader.connectionID, ijoon::RELAY_SESSION_INVALID, nullptr, 0);
        }
        return;
    }
    
    switch (messageHeader.messageType) {
        case ijoon::PROTOBUF:
        {
            ijn_print(DP_INFO, "User message received, PROTOBUF");
            google::protobuf::Message *message = BaseMessageRegistry->Create(messageHeader.packetType);
            if(message == nullptr) {
                ijn_print(DP_INFO, "Unknown protobuf packet_type(%d) reveiced", messageHeader.packetType);
                return;
            }
            message->ParseFromArray(body, messageHeader.dataSize);
            auto callbackWrapper = BaseMessageRegistry->GetCallbackWrapper(messageHeader.messageType, messageHeader.packetType);
            if(callbackWrapper != nullptr) {
                auto rendezvousSession = std::shared_ptr<ijoon::RendezvousSession>(new ijoon::RendezvousSession(relayServer->socket, 0));
                rendezvousSession->setPublicKcpPeer(kcpPeer);
                callbackWrapper->callback(rendezvousSession, message);
            }
            else {
                ijn_print(DP_ERROR, "No callback wrapper");
            }
            
            return;
        }
        case ijoon::RAWBYTE:
        {
            if(messageHeader.packetType >= ijoon::REGISTRATION_RENDEZVOUS_CLIENT_REQUEST) {
                break;
            }
            
            ijn_print(DP_INFO, "User message received, RAWBYTE");
            
            auto callbackWrapper = BaseMessageRegistry->GetCallbackWrapper(messageHeader.messageType, messageHeader.packetType);
            if(callbackWrapper != nullptr) {
                auto rendezvousSession = std::shared_ptr<ijoon::RendezvousSession>(new ijoon::RendezvousSession(relayServer->socket, 0));
                rendezvousSession->setPublicKcpPeer(kcpPeer);
                callbackWrapper->callback(rendezvousSession, body, messageHeader.dataSize);
            }
            else {
                printf("unregistered raw message received. body=%s", body);
            }
            
            return;
        }
        default:
            ijn_print(DP_ERROR, "Unknown message type received, type=%d", messageHeader.messageType);
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
            
            auto vec = ijoon::paramParser(body, 2);
            if(vec == nullptr) break;
            
            int connectionID = messageHeader.connectionID;
            if(relayServer->map.count(connectionID) != 0) {
                ijn_print(DP_ERROR, "[RELAY_SERVICE_REQUEST] already registered");
                return;
            }
            
            auto relayPeerInfo = std::shared_ptr<ijoon::RelayPeerInfo>(new ijoon::RelayPeerInfo());

            relayServer->map[connectionID] = relayPeerInfo;
            
            relayServer->sessionCheckMap[connectionID] = 0;
            
            ijoon::send(kcpPeer, connectionID, ijoon::RELAY_SESSION_READY, nullptr, 0);
            
            return;
        }
        case ijoon::REGISTRATION_RELAY_SERVER_SUCCESS: // from RanS
        {
            ijn_print(DP_DEBUG, "received REGISTRATION_RELAY_SERVER_SUCCESS");
            relayServer->lastRegistrationTime = ijoon::ComputableTime::getCurrentTimeSec();
            return;
        }
        case ijoon::REGISTRATION_RELAY_PEER_REQUEST: // from SP, TP
        {
            ijn_print(DP_DEBUG, "received REGISTRATION_RELAY_PEER_REQUEST from %s", peer.getKey().c_str());
            
            auto vec = ijoon::paramParser(body, 1);
            if(vec == nullptr) break;
            
            std::string peerIP = peer.getIP();
            std::string peerPort = std::to_string(peer.getPort());
            
            if(relayServer->map.count(messageHeader.connectionID) == 0) {
                ijn_print(DP_ERROR, "[REGISTRATION_RELAY_PEER_REQUEST] invalid connection id");
                ijoon::send(kcpPeer, messageHeader.connectionID, ijoon::REGISTRATION_RELAY_PEER_FAILED, nullptr, 0);
                return;
            }
            
            if(relayServer->sessionCheckMap.count(messageHeader.connectionID) == 0) {
                ijn_print(DP_ERROR, "[REGISTRATION_RELAY_PEER_REQUEST] already checked peer");
                ijoon::send(kcpPeer, messageHeader.connectionID, ijoon::REGISTRATION_RELAY_PEER_FAILED, nullptr, 0);
                return;
            }
            
            bool isSP = atoi(vec->at(0).c_str()) ? true : false;
            if(isSP) {
                relayServer->map[messageHeader.connectionID]->sourceKcpPeer = kcpPeer;
                relayServer->sessionCheckMap[messageHeader.connectionID]++;
            }
            else {
                relayServer->map[messageHeader.connectionID]->targetKcpPeer = kcpPeer;
                relayServer->sessionCheckMap[messageHeader.connectionID]++;
            }
            
            ijoon::send(kcpPeer, messageHeader.connectionID, ijoon::REGISTRATION_RELAY_PEER_SUCCESS, nullptr, 0);
            
            if(relayServer->sessionCheckMap[messageHeader.connectionID] >= 2) {
                // successfully registerred
                relayServer->sessionCheckMap.erase(messageHeader.connectionID);
                
                auto serverKcpPeer = relayServer->getKcpPeer(relayServer->serverPeer);
                
                ijoon::send(serverKcpPeer, messageHeader.connectionID, ijoon::RELAY_SESSION_CREATED, nullptr, 0);
            }
            
            return;
        }
        case ijoon::PING_REQUEST:
        {
            ijn_print(DP_DEBUG, "received PING_REQUEST, from %s", peer.getKey().c_str());
            ijoon::send(kcpPeer, 0, ijoon::PING_RESPONSE, nullptr, 0);
            return;
        }
        case ijoon::PING_RESPONSE:
        {
            ijn_print(DP_DEBUG, "received PING_RESPONSE, from %s", peer.getKey().c_str());
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
    
    auto peer = std::shared_ptr<ijoon::Peer>(new ijoon::Peer());
    
    char *rawBuffer = new char[MAX_PACKET_SIZE];
    
    relayServer->socket->option(SocketOptionType::SOCK_RCVTIMEO_MS, 1000);
    
    while(!thread->isInterrupted()) {
        int rcvSize = relayServer->socket->recvFrom(peer.get(), rawBuffer, MAX_PACKET_SIZE);
        if(rcvSize > 0) {
            relayServer->mutexForKcpPeerMap.lock();
            auto kcpPeer = relayServer->getKcpPeer(peer);
            relayServer->mutexForKcpPeerMap.unlock();
            
            kcpPeer->mutex.lock();
            ikcpcb *kcp = kcpPeer->getKcp();
            ikcp_input(kcp, rawBuffer, rcvSize);
            IUINT32 current = iclock();
            ikcp_update(kcp, current);
            kcpPeer->mutex.unlock();
        }
    }
    
    delete []rawBuffer;
    return THREAD_EXIT;
}

ijoon::THREAD_RET THREAD_API ijoon::recvThreadFunc(void *arg) {
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::RelayServer *relayServer = (ijoon::RelayServer *)thread->getParam();
    
    char *buffer = new char[MAX_PACKET_SIZE];
    
    while(!thread->isInterrupted()) {
        relayServer->mutexForKcpPeerMap.lock();
        
        IUINT32 current = iclock();
        auto iter = relayServer->kcpPeerMap.begin();
        for(; iter != relayServer->kcpPeerMap.end() ; ++iter) {
            auto kcpPeer = iter->second;
            kcpPeer->mutex.lock();
            ikcpcb *kcp = kcpPeer->getKcp();
            int rcvSize = ikcp_recv(kcp, buffer, MAX_PACKET_SIZE);
            ikcp_update(kcp, current);
            kcpPeer->mutex.unlock();
            
            if(rcvSize > 0) {
                // callback to upper users
                buffer[rcvSize] = '\0';
                onCallback(relayServer, kcpPeer, buffer, rcvSize);
            }
            
        }
        relayServer->mutexForKcpPeerMap.unlock();
        
        ijn_msleep(10);
    }
    
    ijn_print(DP_DEBUG, "recvThread finished");
    delete[] buffer;
    return THREAD_EXIT;
}
