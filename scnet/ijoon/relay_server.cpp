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

std::map<std::string, std::shared_ptr<ijoon::KcpPeer>> ijoon::RelayServer::getKcpPeerMap() {
    std::map<std::string, std::shared_ptr<ijoon::KcpPeer>> cpyMap;
    
    mutexForKcpPeerMap.lock();
    if(0 < kcpPeerMap.size()) {
        cpyMap.insert(kcpPeerMap.begin(), kcpPeerMap.end());
    }
    mutexForKcpPeerMap.unlock();
    return cpyMap;
}

std::map<int, std::shared_ptr<ijoon::RelayPeerInfo>> ijoon::RelayServer::getRelayPeerInfoMap() {
    std::map<int, std::shared_ptr<ijoon::RelayPeerInfo>> cpyMap;
    
    mutexForMap.lock();
    if(0 < map.size()) {
        cpyMap.insert(map.begin(), map.end());
    }
    mutexForMap.unlock();
    return cpyMap;
}

std::map<int, int> ijoon::RelayServer::getSessionCheckMap() {
    std::map<int, int> cpyMap;
    
    mutexForSessionCheckMap.lock();
    if(0 < sessionCheckMap.size()) {
        cpyMap.insert(sessionCheckMap.begin(), sessionCheckMap.end());
    }
    mutexForSessionCheckMap.unlock();
    return cpyMap;
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

std::shared_ptr<ijoon::KcpPeer> ijoon::RelayServer::getKcpPeerWithLock(ijoon::Peer &peer) {
    std::shared_ptr<ijoon::KcpPeer> kcpPeer;
    
    mutexForKcpPeerMap.lock();
    kcpPeer = getKcpPeer(peer);
    mutexForKcpPeerMap.unlock();
    
    return kcpPeer;
}

void ijoon::RelayServer::checkKcpPeerMap(bool &connFlag, const int timeoutSec, const int pingIntervalSec, const int loopIntervalMs) {
    time_t currentTime = ijoon::ComputableTime::getCurrentTimeSec();
    
    // periodically check & erase peer
    mutexForKcpPeerMap.lock();
    auto iter = kcpPeerMap.begin();
    auto end = kcpPeerMap.end();
    while(iter != end) {
        auto kcpPeer = iter->second;
        switch(kcpPeer->type) {
            case ijoon::PeerType::RENDEZVOUS_SERVER:
            {
                if(kcpPeer->status != ijoon::PeerStatus::REGISTERED) {
                    // send registration packet
                    if(connFlag) {
                        kcpPeer = std::shared_ptr<ijoon::KcpPeer>(new ijoon::KcpPeer(socket, rensIP, rensPort, udp_output));
                        kcpPeer->type = ijoon::PeerType::RENDEZVOUS_SERVER;
                        kcpPeerMap[kcpPeer->getPeer().getKey()] = kcpPeer;
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
                    iter = kcpPeerMap.erase(iter);
                    continue;
                }
                else if(kcpPeer->status == ijoon::PeerStatus::UNREGISTERED) {
                    iter = kcpPeerMap.erase(iter);
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
    mutexForKcpPeerMap.unlock();
}

void ijoon::RelayServer::removeConnectionlessKcpPeer() {
    // remove connectionless kcpPeer in kcpPeerMap
    mutexForKcpPeerMap.lock();
    for(auto iter = kcpPeerMap.begin() ; iter != kcpPeerMap.end() ; ) {

        if(iter->second->type == ijoon::PeerType::RENDEZVOUS_SERVER) {
            iter++;
            continue;
        }

        bool isConnectionInfoExist = false;
        mutexForMap.lock();
        for(auto iterMap = map.begin() ; iterMap != map.end() ; ++iterMap) {
            auto key = iter->first;
            auto spKey = iterMap->second->sourceKcpPeer->getPeer().getKey();
            auto tpKey = iterMap->second->targetKcpPeer->getPeer().getKey();
            if(key == spKey || key == tpKey) {
                isConnectionInfoExist = true;
                break;
            }
        }
        mutexForMap.unlock();

        if(!isConnectionInfoExist) {
            iter->second->status = ijoon::PeerStatus::UNREGISTERED;
            // relayConnectionDisconnected() callback
            iter->second->send(ijoon::RENDEZVOUS_MSG::RELAY_SERVER_DISCONNECTED);
        }
        
        iter++;
    }
    mutexForKcpPeerMap.unlock();
}

void ijoon::RelayServer::removeUnregisteredRelayPeerInfo() {
    // remove connectionInfo containing unregistered peer in connectionInfoMap
    mutexForMap.lock();
    for(auto iter = map.begin() ; iter != map.end() ; ) {
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
            iter = map.erase(iter);
        }
        else {
            iter++;
        }
    }
    mutexForMap.unlock();
}

ijoon::THREAD_RET THREAD_API registerThreadFunc(void *arg) {
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::RelayServer *relayServer = (ijoon::RelayServer *)thread->getParam();
    
    const int timeoutSec = 15;
    const int pingIntervalSec = 5;
    const int loopIntervalMs = 5 * 1000;
    bool connFlag = false;
    while(!thread->isInterrupted()) {
        relayServer->checkKcpPeerMap(connFlag, timeoutSec, pingIntervalSec, loopIntervalMs);
        relayServer->removeUnregisteredRelayPeerInfo();
        relayServer->removeConnectionlessKcpPeer();
         
        
        
        
        
//        relayServer->mutexForKcpPeerMap.lock();
//        ijn_print(DP_ERROR, "-------KCP_PEER_MAP INFO START-------");
//        for(auto iter = relayServer->kcpPeerMap.begin() ; iter != relayServer->kcpPeerMap.end() ; ++iter) {
//            ijn_print(DP_ERROR, "%s", iter->first.c_str());
//        }
//        ijn_print(DP_ERROR, "-------KCP_PEER_MAP INFO END-------");
//        relayServer->mutexForKcpPeerMap.unlock();
//
//        relayServer->mutexForMap.lock();
//        ijn_print(DP_ERROR, "-------MAP INFO START-------");
//        for(auto iter = relayServer->map.begin() ; iter != relayServer->map.end() ; ++iter) {
//            ijn_print(DP_ERROR, "[%d] %s <-> %s", iter->first,
//                      iter->second->sourceKcpPeer->getPeer().getKey().c_str(),
//                      iter->second->targetKcpPeer->getPeer().getKey().c_str());
//        }
//        ijn_print(DP_ERROR, "-------MAP INFO END-------");
//        relayServer->mutexForMap.unlock();
        
        thread->sleep(loopIntervalMs);
    }
    
    return THREAD_EXIT;
}

bool validationPeer(ijoon::RelayServer *relayServer, uint connectionID, ijoon::Peer peer, bool &isSP) {
    // does it checked session
    if(relayServer->isExistCheckSession(connectionID)) {
        ijn_print(DP_ERROR, "validation failed, connection not checked");
        return false;
    }
    
    auto relayPeerInfo = relayServer->getRelayPeerInfo(connectionID);
    if(relayPeerInfo == nullptr) {
        ijn_print(DP_ERROR, "validation failed, connectionID not exist");
        return false;
    }
    
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
        if(!validationPeer(relayServer, messageHeader.connectionID, peer, isSP)) {
            ijn_print(DP_ERROR, "Invalid peer's relay packet");
            kcpPeer->send(ijoon::RELAY_SESSION_INVALID);
            return;
        }
        
        auto relayPeerInfo = relayServer->getRelayPeerInfo(messageHeader.connectionID);
        if(relayPeerInfo == nullptr) {
            ijn_print(DP_ERROR, "relay peer info not exist");
            return;
        }
        auto targetkcpPeer = isSP ? relayPeerInfo->targetKcpPeer : relayPeerInfo->sourceKcpPeer;
        if(!targetkcpPeer->send(messageHeader, body, messageHeader.dataSize)) {
            ijn_print(DP_ERROR, "Invalid peer's relay packet (exceed waitsnd)");
            
            relayServer->removeRelayPeerInfo(messageHeader.connectionID);
            ijn_print(DP_INFO, "removed connectionInfo, connectionID=%d", messageHeader.connectionID);
            kcpPeer->send(ijoon::RELAY_SESSION_INVALID);
        }
        return;
    }
    else {
        auto registry = Registry<int, google::protobuf::Message *>().Get();

        kcpPeer->lastPing = ijoon::ComputableTime::getCurrentTimeSec();
        
        auto callbackWrapper = registry->GetCallbackWrapper(messageHeader.messageType, messageHeader.packetType);
        if(callbackWrapper != nullptr) {
            callbackWrapper->callback(kcpPeer, body, messageHeader.dataSize);
        }
        else {
            printf("Not registered raw message received. mtype=%d, body=%s", messageHeader.messageType, body);
        }
    }
}

void ijoon::RelayServer::receivedDataDistToKcpPeer(char *buffer, IUINT32 current) {
    mutexForKcpPeerMap.lock();
    
    auto iter = kcpPeerMap.begin();
    for(; iter != kcpPeerMap.end() ; ++iter) {
        auto kcpPeer = iter->second;
        kcpPeer->mutex.lock();
        ikcpcb *kcp = kcpPeer->getKcp();
        int rcvSize = ikcp_recv(kcp, buffer, MAX_PACKET_SIZE);
        ikcp_update(kcp, current);
        kcpPeer->mutex.unlock();
        
        if(rcvSize > 0) {
            // callback to upper users
            buffer[rcvSize] = '\0';
            onCallback(this, kcpPeer, buffer, rcvSize);
        }
        
    }
    mutexForKcpPeerMap.unlock();
}

std::shared_ptr<ijoon::RelayPeerInfo> ijoon::RelayServer::getRelayPeerInfo(int connectionID) {
    std::shared_ptr<RelayPeerInfo> relayPeerInfo;
    mutexForMap.lock();
    if(map.find(connectionID) != map.end()) {
        relayPeerInfo = map[connectionID];
    }
    mutexForMap.unlock();
    return relayPeerInfo;
}

void ijoon::RelayServer::setRelayPeerInfo(int connectionID, std::shared_ptr<RelayPeerInfo> relayPeerInfo) {
    mutexForMap.lock();
    map[connectionID] = relayPeerInfo;
    mutexForMap.unlock();
}

void ijoon::RelayServer::removeRelayPeerInfo(int connectionID) {
    mutexForMap.lock();
    map.erase(connectionID);
    mutexForMap.unlock();
}

bool ijoon::RelayServer::isExistCheckSession(int connectionID) {
    mutexForSessionCheckMap.lock();
    bool isExist = sessionCheckMap.count(connectionID) > 0;
    mutexForSessionCheckMap.unlock();
    return isExist;
}

void ijoon::RelayServer::setCheckSession(int connectionID, int count) {
    mutexForSessionCheckMap.lock();
    sessionCheckMap[connectionID] = count;
    mutexForSessionCheckMap.unlock();
}
int ijoon::RelayServer::getCheckSession(int connectionID) {
    int count = -1;
    mutexForSessionCheckMap.lock();
    if(sessionCheckMap.find(connectionID) != sessionCheckMap.end()) {
        count = sessionCheckMap[connectionID];
    }
    mutexForSessionCheckMap.unlock();
    return count;
}
void ijoon::RelayServer::removeCheckSession(int connectionID) {
    mutexForSessionCheckMap.lock();
    sessionCheckMap.erase(connectionID);
    mutexForSessionCheckMap.unlock();
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
            auto kcpPeer = relayServer->getKcpPeerWithLock(peer);
            
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
        IUINT32 current = iclock();
        relayServer->receivedDataDistToKcpPeer(buffer, current);
        ijn_msleep(10);
    }
    
    ijn_print(DP_DEBUG, "recvThread finished");
    delete[] buffer;
    return THREAD_EXIT;
}
