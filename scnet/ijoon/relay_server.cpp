#include "relay_server.h"
#include "rendezvous_message.h"
#include "message_header.h"
#include <cstring>
#include "registry.h"
#include <ijoon/coreutils.h>
#include "utils.h"

extern char seperator;

ijoon::THREAD_RET THREAD_API registerThreadFunc(void *arg);
ijoon::THREAD_RET THREAD_API recvThreadFunc(void *arg);
ijoon::THREAD_RET THREAD_API rawRecvThreadFunc(void *arg);

int udp_output(const char *buf, int len, ikcpcb *kcp, void *user) {
    ijoon::KcpPeer *kcpPeer = (ijoon::KcpPeer *)user;
    ijoon::Peer peer = kcpPeer->getPeer();
    kcpPeer->getClientSocket()->sendTo(&peer, const_cast<char *>(buf), len);
    return 0;
}

void ijoon::RelayServer::start() {
    ijn_print(DP_INFO, "Relay server start...");

    recvThread = new ijoon::Thread(recvThreadFunc, "recv thread");
    recvThread->start(this);
    rawRecvThread = new ijoon::Thread(rawRecvThreadFunc, "raw recv thread");
    rawRecvThread->start(this);
    registerThread = new ijoon::Thread(registerThreadFunc, "relay register thread");
    registerThread->start(this);
}

std::shared_ptr<ijoon::KcpPeer> ijoon::RelayServer::getKcpPeer(ijoon::Peer &peer) {
    std::shared_ptr<ijoon::KcpPeer> kcpPeer;
    
    try {
        kcpPeer = this->kcpPeerMap.at(peer.getKey());
    } catch (std::exception e) {
        kcpPeer = std::shared_ptr<ijoon::KcpPeer>(new ijoon::KcpPeer(this->socket, peer.getIP(), std::to_string(peer.getPort()), udp_output));
        this->kcpPeerMap[peer.getKey()] = kcpPeer;
    }
    
    return kcpPeer;
}

ijoon::THREAD_RET THREAD_API registerThreadFunc(void *arg) {
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::RelayServer *relayServer = (ijoon::RelayServer *)thread->getParam();
    
    const int timeoutSec = 15;
    const int pingIntervalSec = 5;
    const int loopIntervalMs = 5 * 1000;
    bool connFlag = false;
    while(!thread->isInterrupted()) {
        time_t currentTime = ijoon::ComputableTime::getCurrentTimeSec();
        
        // periodically check & erase peer
        relayServer->mutexForKcpPeerMap.lock();
        auto iter = relayServer->kcpPeerMap.begin();
        auto end = relayServer->kcpPeerMap.end();
        while(iter != end) {
            auto kcpPeer = iter->second;
            switch(kcpPeer->type) {
                case ijoon::PeerType::RENDEZVOUS_SERVER:
                {
                    if(kcpPeer->status != ijoon::PeerStatus::REGISTERED) {
                        // send registration packet
                        if(connFlag) {
                            kcpPeer = std::shared_ptr<ijoon::KcpPeer>(new ijoon::KcpPeer(relayServer->socket, relayServer->rensIP, relayServer->rensPort, udp_output));
                            kcpPeer->type = ijoon::PeerType::RENDEZVOUS_SERVER;
                            relayServer->kcpPeerMap[kcpPeer->getPeer().getKey()] = kcpPeer;
                        }
                        else {
                            connFlag = true;
                        }
                        
                        ijn_print(DP_INFO, "Connecting to rendezvous server");
                        kcpPeer->send(ijoon::REGISTRATION_RELAY_SERVER_REQUEST);
                    }
                    else if(kcpPeer->lastPing + timeoutSec < currentTime) {
                        // disconnected
                        kcpPeer->status = ijoon::PeerStatus::UNREGISTERED;
                        ijn_print(DP_INFO, "Disconnected from rendezvous server");
                    }
                    else if(kcpPeer->lastPing + pingIntervalSec < currentTime) {
                        kcpPeer->send(ijoon::PING_REQUEST);
                    }
                }
                    break;
                default:
                {
                    if(kcpPeer->lastPing + timeoutSec < currentTime) {
                        ijn_print(DP_INFO, "removed kcpPeer: %s", kcpPeer->getPeer().getKey().c_str());
                        kcpPeer->status = ijoon::PeerStatus::UNREGISTERED;
                        iter = relayServer->kcpPeerMap.erase(iter);
                        continue;
                    }
                    else if(kcpPeer->status == ijoon::PeerStatus::UNREGISTERED) {
                        iter = relayServer->kcpPeerMap.erase(iter);
                        continue;
                    }
                    else if(kcpPeer->type != ijoon::PeerType::NONE &&
                            kcpPeer->lastPing + pingIntervalSec < currentTime) {
                        kcpPeer->send(ijoon::PING_REQUEST);
                    }
                }
                    break;
            }
            
            ++iter;
        }
        relayServer->mutexForKcpPeerMap.unlock();
         
        // remove connectionInfo containing unregistered peer in connectionInfoMap
        relayServer->mutexForMap.lock();
        for(auto iter = relayServer->map.begin() ; iter != relayServer->map.end() ; ) {
            auto spUnregistered = iter->second->sourceKcpPeer->status == ijoon::PeerStatus::UNREGISTERED;
            auto tpUnregistered = iter->second->targetKcpPeer->status == ijoon::PeerStatus::UNREGISTERED;
            if(spUnregistered || tpUnregistered) {
                ijn_print(DP_INFO, "removed connectionInfo, connectionID=%d", iter->first);
                if(!spUnregistered) {
                    ijn_print(DP_INFO, "invalid packet sent to sp");
                    auto sp = iter->second->sourceKcpPeer;
                    sp->connectionID = iter->first;
                    sp->send(ijoon::RENDEZVOUS_MSG::RELAY_SESSION_INVALID);
                }
                if(!tpUnregistered) {
                    ijn_print(DP_INFO, "invalid packet sent to tp");
                    auto tp = iter->second->targetKcpPeer;
                    tp->connectionID = iter->first;
                    tp->send(ijoon::RENDEZVOUS_MSG::RELAY_SESSION_INVALID);
                }
                iter = relayServer->map.erase(iter);
            }
            else {
                iter++;
            }
        }
        relayServer->mutexForMap.unlock();
        
        // remove connectionless kcpPeer in kcpPeerMap
        relayServer->mutexForKcpPeerMap.lock();
        for(auto iter = relayServer->kcpPeerMap.begin() ; iter != relayServer->kcpPeerMap.end() ; ) {

            if(iter->second->type == ijoon::PeerType::RENDEZVOUS_SERVER) {
                iter++;
                continue;
            }

            bool isConnectionInfoExist = false;
            relayServer->mutexForMap.lock();
            for(auto iterMap = relayServer->map.begin() ; iterMap != relayServer->map.end() ; ++iterMap) {
                auto key = iter->first;
                auto spKey = iterMap->second->sourceKcpPeer->getPeer().getKey();
                auto tpKey = iterMap->second->targetKcpPeer->getPeer().getKey();
                if(key == spKey || key == tpKey) {
                    isConnectionInfoExist = true;
                    break;
                }
            }
            relayServer->mutexForMap.unlock();

            if(!isConnectionInfoExist) {
                iter->second->status = ijoon::PeerStatus::UNREGISTERED;
                // relayConnectionDisconnected() callback
                iter->second->send(ijoon::RENDEZVOUS_MSG::RELAY_SERVER_DISCONNECTED);
            }
            
            iter++;
        }
        relayServer->mutexForKcpPeerMap.unlock();
        
        thread->sleep(loopIntervalMs);
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
    if(messageHeader.packetType < ijoon::RENDEZVOUS_MSG::REGISTRATION_RENDEZVOUS_CLIENT_REQUEST) {
        bool isSP = false;
        if(!validationPeer(relayServer->map, relayServer->sessionCheckMap, messageHeader.connectionID, peer, isSP)) {
            ijn_print(DP_ERROR, "Invalid peer's relay packet");
            kcpPeer->send(ijoon::RELAY_SESSION_INVALID);
            return;
        }
        
        auto relayPeerInfo = relayServer->map[messageHeader.connectionID];
        auto targetkcpPeer = isSP ? relayPeerInfo->targetKcpPeer : relayPeerInfo->sourceKcpPeer;
        if(!targetkcpPeer->send(messageHeader, body, messageHeader.dataSize)) {
            ijn_print(DP_ERROR, "Invalid peer's relay packet (exceed waitsnd)");
            
            relayServer->map.erase(messageHeader.connectionID);
            ijn_print(DP_INFO, "removed connectionInfo, connectionID=%d", messageHeader.connectionID);
            kcpPeer->send(ijoon::RELAY_SESSION_INVALID);
        }
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
            
            auto vec = ijoon::paramParser(body, 3);
            if(vec == nullptr) break;
            auto connectionIDStr = vec->at(0);
            auto connectionID = static_cast<uint>(atoi(connectionIDStr.c_str()));
            
            if(relayServer->map.count(connectionID) != 0) {
                ijn_print(DP_ERROR, "[RELAY_SERVICE_REQUEST] already registered");
                return;
            }
            
            auto relayPeerInfo = std::shared_ptr<ijoon::RelayPeerInfo>(new ijoon::RelayPeerInfo());

            relayServer->map[connectionID] = relayPeerInfo;
            
            relayServer->sessionCheckMap[connectionID] = 0;
            
            kcpPeer->send(ijoon::RELAY_SESSION_READY, (char *)connectionIDStr.c_str(), connectionIDStr.length());
            
            return;
        }
        case ijoon::REGISTRATION_RELAY_SERVER_RESPONSE: // from RanS
        {
            ijn_print(DP_DEBUG, "received REGISTRATION_RELAY_SERVER_RESPONSE");
            auto vec = ijoon::paramParser(body, 1);
            auto errStr = vec->at(0);
            bool isSuccess = (errStr == "1") ? true : false;
            
            if(isSuccess) {
                ijn_print(DP_INFO, "REGISTRATION_RELAY_SERVER_RESPONSE success");
                kcpPeer->status = ijoon::PeerStatus::REGISTERED;
            }
            else {
                ijn_print(DP_INFO, "REGISTRATION_RELAY_SERVER_RESPONSE result : %s", errStr.c_str());
            }
            
            relayServer->lastRegistrationTime = ijoon::ComputableTime::getCurrentTimeSec();
            return;
        }
        case ijoon::REGISTRATION_RELAY_PEER_REQUEST: // from SP, TP
        {
            ijn_print(DP_DEBUG, "received REGISTRATION_RELAY_PEER_REQUEST from %s", peer.getKey().c_str());
            
            auto vec = ijoon::paramParser(body, 2);
            if(vec == nullptr) break;
            auto connectionIDStr = vec->at(0);
            auto connectionID = static_cast<uint>(atoi(connectionIDStr.c_str()));
            auto isSP = atoi(vec->at(1).c_str()) ? true : false;
            
            //TODO: mutex 사용
            if(relayServer->map.count(connectionID) == 0) {
                ijn_print(DP_ERROR, "[REGISTRATION_RELAY_PEER_REQUEST] invalid connection id");
                return;
            }
            
            if(relayServer->sessionCheckMap.count(connectionID) == 0) {
                ijn_print(DP_ERROR, "[REGISTRATION_RELAY_PEER_REQUEST] already checked peer");
                return;
            }
            
            kcpPeer->type = ijoon::PeerType::RENDEZVOUS_CLIENT;
            kcpPeer->status = ijoon::PeerStatus::REGISTERED;
            if(isSP) {
                relayServer->map[connectionID]->sourceKcpPeer = kcpPeer;
            }
            else {
                relayServer->map[connectionID]->targetKcpPeer = kcpPeer;
            }
            
            relayServer->sessionCheckMap[connectionID]++;
            
            if(relayServer->sessionCheckMap[connectionID] >= 2) {
                // successfully registerred
                relayServer->sessionCheckMap.erase(connectionID);
                
                auto serverKcpPeer = relayServer->getKcpPeer(relayServer->serverPeer);
                
                serverKcpPeer->send(ijoon::RELAY_SESSION_CREATED, (char *)connectionIDStr.c_str(), connectionIDStr.length());
            }
            
            return;
        }
        case ijoon::PING_REQUEST:
        {
            kcpPeer->send(ijoon::PING_RESPONSE);
            return;
        }
        case ijoon::PING_RESPONSE:
        {
            return;
        }
        default:
        {
            ijn_print(DP_ERROR, "Undefined message received");
            break;
        }
    }
}

ijoon::THREAD_RET THREAD_API rawRecvThreadFunc(void *arg) {
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::RelayServer *relayServer = (ijoon::RelayServer *)thread->getParam();
    
    auto peer = ijoon::Peer();
    
    char *rawBuffer = new char[MAX_PACKET_SIZE];
    
    relayServer->socket->option(ijoon::SocketOptionType::SOCK_RCVTIMEO_MS, 1000);
    
    while(!thread->isInterrupted()) {
        int rcvSize = relayServer->socket->recvFrom(&peer, rawBuffer, MAX_PACKET_SIZE);
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

ijoon::THREAD_RET THREAD_API recvThreadFunc(void *arg) {
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
