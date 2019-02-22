#include "rendezvous_server.h"
#include "rendezvous_message.h"
#include "message_header.h"
#include <cstring>
#include "ikcp.h"

extern char seperator;

IUINT32 iclock();

int udp_output(const char *buf, int len, ikcpcb *kcp, void *user) {
//    ijn_print(DP_DEBUG, "udp_output len : %d.", len);
//    ijn_print(DP_DEBUG, "udp_output buf : %s.", buf);
    
    auto kcpPeer = (ijoon::KcpPeer *)user;
    if(kcpPeer!= nullptr) {
        ijoon::Peer peer = kcpPeer->getPeer();
        kcpPeer->getClientSocket()->sendTo(&peer, const_cast<char *>(buf), len);
        return 0;
    }
    return -1;
}

void ijoon::RendezvousServer::start() {
    ijn_print(DP_INFO, "Rendezvous server start...");

    rawRecvThread = new ijoon::Thread(rawRecvThreadFunc, "raw recv thread");
    rawRecvThread->start(this);
    recvThread = new ijoon::Thread(recvThreadFunc, "recv thread");
    recvThread->start(this);
    checkThread = new ijoon::Thread(rendezvousCheckThread, "rendezvous check thread");
    checkThread->start(this);
    
    lastCheckTime = iclock();
}

bool connection(ijoon::RendezvousServer *server, ijoon::MessageHeader messageHeader) {
    auto connectionInfo = server->connectionInfoMap[messageHeader.connectionID];
    
    // SP/TP nat check
    auto spRendezvousPeer = server->rendezvousSessionMap[connectionInfo->sp.getKey()];
    auto tpRendezvousPeer = server->rendezvousSessionMap[connectionInfo->tp.getKey()];
    
    if(spRendezvousPeer == nullptr || tpRendezvousPeer == nullptr) {
        ijn_print(DP_ERROR, "[RELAY_SESSION_CREATED] No registered rendezvous peer detected");
        return false;
    }
    
    bool isSPPublic = spRendezvousPeer->isPublic();
    bool isTPPublic = tpRendezvousPeer->isPublic();
    
    if(isTPPublic) { // pub/pub, pri/pub
        std::string data = tpRendezvousPeer->getPublicKcpPeer()->getPeer().getIP() + seperator + std::to_string(tpRendezvousPeer->getPublicKcpPeer()->getPeer().getPort()); // tp public address
        ijoon::send(spRendezvousPeer->getPublicKcpPeer()->getKcp(), messageHeader.connectionID, ijoon::RENDEZVOUS_MSG::DIRECT_CONNECTION_AVAILABLE, (char *)data.c_str(), data.length());
    }
    else if(isSPPublic && !isTPPublic) { // pub/pri
        std::string data = spRendezvousPeer->getPublicKcpPeer()->getPeer().getIP() + seperator + std::to_string(spRendezvousPeer->getPublicKcpPeer()->getPeer().getPort()); // sp public address
        ijoon::send(tpRendezvousPeer->getPublicKcpPeer()->getKcp(), messageHeader.connectionID, ijoon::RENDEZVOUS_MSG::REVERSE_CONNECTION, (char *)data.c_str(), data.length());
    }
    else { // pri/pri
        std::string data;
        
        data = tpRendezvousPeer->getPublicKcpPeer()->getPeer().getIP(); // tp public address
        data += seperator;
        data += std::to_string(tpRendezvousPeer->getPublicKcpPeer()->getPeer().getPort());
        data += seperator;
        data += tpRendezvousPeer->getPrivateKcpPeer()->getPeer().getIP();
        data += seperator;
        data += std::to_string(tpRendezvousPeer->getPrivateKcpPeer()->getPeer().getPort());
        ijoon::send(spRendezvousPeer->getPublicKcpPeer()->getKcp(), messageHeader.connectionID, ijoon::RENDEZVOUS_MSG::UDP_HOLE_PUNCHING_AVAILABLE, (char *)data.c_str(), data.length());
        
        data = spRendezvousPeer->getPublicKcpPeer()->getPeer().getIP(); // sp public address
        data += seperator;
        data += std::to_string(spRendezvousPeer->getPublicKcpPeer()->getPeer().getPort());
        data += seperator;
        data += spRendezvousPeer->getPrivateKcpPeer()->getPeer().getIP();
        data += seperator;
        data += std::to_string(spRendezvousPeer->getPrivateKcpPeer()->getPeer().getPort());
        ijoon::send(tpRendezvousPeer->getPublicKcpPeer()->getKcp(), messageHeader.connectionID, ijoon::RENDEZVOUS_MSG::UDP_HOLE_PUNCHING_AVAILABLE, (char *)data.c_str(), data.length());
    }
    
    server->connectionInfoMap.erase(messageHeader.connectionID);
    
    return true;
}

ijoon::THREAD_RET THREAD_API ijoon::rendezvousCheckThread(void *arg)
{
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::RendezvousServer *server = (ijoon::RendezvousServer *)thread->getParam();
    
    const int timeout = 180;
    const int checkIntervalMs = 5000;
    while(!thread->isInterrupted())
    {
        thread->sleep(checkIntervalMs);
        time_t currentTime = ijoon::ComputableTime::getCurrentTimeSec();
        
        {
            auto iter = server->kcpPeerMap.begin();
            auto end = server->kcpPeerMap.end();
            for(; iter != end ; ++iter) {
                if(iter->second->lastPing + timeout < currentTime) {
                    auto kcpPeer = iter->second;
                    int connectionID = kcpPeer->getConnectionID();
                    std::string key = kcpPeer->getPeer().getKey();
                    if(connectionID != 0 && server->rendezvousSessionMap.count(key) != 0) {
                        auto rendezvousSession = server->rendezvousSessionMap.at(key);
                        if(rendezvousSession->getPublicKcpPeer() == kcpPeer) {
                            rendezvousSession->clearPublicKcpPeer();
                        }
                        if(rendezvousSession->getPrivateKcpPeer() == kcpPeer) {
                            rendezvousSession->clearPrivateKcpPeer();
                        }
                        if(rendezvousSession->getRelayKcpPeer() == kcpPeer) {
                            rendezvousSession->clearRelayKcpPeer();
                        }
                    }
                    if(connectionID != 0 && server->relayServerMap.count(key) != 0) {
                        server->relayServerMap.erase(key);
                    }
                    server->kcpPeerMap.erase(key);
                    ijn_print(DP_INFO, "kcpPeer removed, %s, (%u)", key.c_str(), kcpPeer->lastPing);
                }
            }
        }
    }
    
    return THREAD_EXIT;
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

void onCallback(ijoon::RendezvousServer *server, std::shared_ptr<ijoon::KcpPeer> kcpPeer, char *packet, int recvSize) {
    auto peer = kcpPeer->getPeer();
    
    ijoon::MessageHeader messageHeader;
    int cursor = 0;
    if(!ijoon::readHeader(packet, recvSize, messageHeader, cursor)) {
        ijn_print(DP_ERROR, "invalid header");
        return;
    }
    
    if(messageHeader.messageType != ijoon::MESSAGE_TYPE::RAWBYTE) {
        ijn_print(DP_ERROR, "type error");
        return;
    }
    
    if((recvSize-cursor) != messageHeader.dataSize) {
        ijn_print(DP_ERROR, "invalid data size");
        return;
    }
    
    kcpPeer->setConnectionID(messageHeader.connectionID);
    
    char *body = &packet[cursor];
    
    switch (messageHeader.packetType) {
        case ijoon::REGISTRATION_RELAY_SERVER_REQUEST: // from RelS
        {
            ijn_print(DP_DEBUG, "received REGISTRATION_RELAY_SERVER_REQUEST");
            
            std::string key = peer.getKey();
            server->relayServerMap[key] = kcpPeer;
            
            ijoon::send(kcpPeer->getKcp(), 0, ijoon::REGISTRATION_RELAY_SERVER_SUCCESS, nullptr, 0);
            break;
        }
        case ijoon::RELAY_SESSION_READY: // from RelS
        {
            ijn_print(DP_DEBUG, "received RELAY_SESSION_READY");
            
            if(server->connectionInfoMap.count(messageHeader.connectionID) == 0) {
                ijn_print(DP_ERROR, "[RELAY_SESSION_READY] invalid request from relay server");
                break;
            }
            
            auto connectionInfo = server->connectionInfoMap[messageHeader.connectionID];
            
            auto sourcePeer = server->rendezvousSessionMap[connectionInfo->sp.getKey()];
            auto targetPeer = server->rendezvousSessionMap[connectionInfo->tp.getKey()];
            
            sourcePeer->getRelayKcpPeer()->getPeer().setIP(peer.getIP());
            sourcePeer->getRelayKcpPeer()->getPeer().setPort(std::to_string(peer.getPort()));
            targetPeer->getRelayKcpPeer()->getPeer().setIP(peer.getIP());
            targetPeer->getRelayKcpPeer()->getPeer().setPort(std::to_string(peer.getPort()));
            
            std::string data;
            data = peer.getIP() + seperator + std::to_string(peer.getPort()) + seperator + "1";
            ijoon::send(sourcePeer->getPublicKcpPeer()->getKcp(), messageHeader.connectionID, ijoon::RELAY_SERVER_INFORMATION, (char *)data.c_str(), data.length());
            data = peer.getIP() + seperator + std::to_string(peer.getPort()) + seperator + "0";
            ijoon::send(targetPeer->getPublicKcpPeer()->getKcp(), messageHeader.connectionID, ijoon::RELAY_SERVER_INFORMATION, (char *)data.c_str(), data.length());
            
            break;
        }
        case ijoon::RELAY_SESSION_CREATED: // from RelS
        {
            ijn_print(DP_DEBUG, "received RELAY_SESSION_CREATED");
            
            if(server->connectionInfoMap.count(messageHeader.connectionID) == 0) {
                ijn_print(DP_ERROR, "[RELAY_SESSION_CREATED] relay connection info not exist, %d", messageHeader.connectionID);
                break;
            }
            auto connectionInfo = server->connectionInfoMap[messageHeader.connectionID];
            auto sourcePeer = server->rendezvousSessionMap[connectionInfo->sp.getKey()];
            auto targetPeer = server->rendezvousSessionMap[connectionInfo->tp.getKey()];
            
            // send connected packet
            std::string data = peer.getIP() + seperator + std::to_string(peer.getPort());
            ijoon::send(sourcePeer->getPublicKcpPeer()->getKcp(), messageHeader.connectionID, ijoon::CONNECTION_RELAY_SERVICE_SUCCESS, (char *)data.c_str(), data.length());
            ijoon::send(targetPeer->getPublicKcpPeer()->getKcp(), messageHeader.connectionID, ijoon::CONNECTION_RELAY_SERVICE_SUCCESS, (char *)data.c_str(), data.length());
            
            connection(server, messageHeader);
            
            break;
        }
        case ijoon::RELAY_SESSION_CREATING_FAILED: // from RelS
        {
            ijn_print(DP_DEBUG, "received RELAY_SESSION_CREATING_FAILED");
            
            auto connectionInfo = server->connectionInfoMap[messageHeader.connectionID];
            auto sourcePeer = server->rendezvousSessionMap[connectionInfo->sp.getKey()];
            
            ijoon::send(sourcePeer->getPublicKcpPeer()->getKcp(), messageHeader.connectionID, ijoon::CONNECTION_RELAY_SERVICE_FAILED, nullptr, 0);
            connection(server, messageHeader);
            break;
        }
        case ijoon::REGISTRATION_RENDEZVOUS_CLIENT_REQUEST: // from SP, TP
        {
            ijn_print(DP_DEBUG, "received REGISTRATION_RENDEZVOUS_CLIENT_REQUEST");
            
            auto vec = ijoon::paramParser(body, 2);
            if(vec == nullptr) break;
            
            std::string key = peer.getKey();
            
            std::shared_ptr<ijoon::RendezvousSession> rendezvousSession;
            if(server->rendezvousSessionMap.count(key) == 0) {
                server->mutex.lock();
                rendezvousSession = std::shared_ptr<ijoon::RendezvousSession>(new ijoon::RendezvousSession(server->socket, messageHeader.connectionID));
                rendezvousSession->setPublicKcpPeer(kcpPeer);
                server->rendezvousSessionMap[key] = rendezvousSession;
                server->mutex.unlock();
                ijn_print(DP_INFO, "[REGISTRATION_RENDEZVOUS_CLIENT_REQUEST] new rendezvous peer registered");
            }
            else {
                rendezvousSession = server->rendezvousSessionMap.at(key);
            }
            
            rendezvousSession->setPrivateKcpPeer(vec->at(0), vec->at(1), udp_output);
            
            ijn_print(DP_INFO, "Register local=%s:%d, public=%s:%d", rendezvousSession->getPrivateKcpPeer()->getPeer().getIP().c_str(), rendezvousSession->getPrivateKcpPeer()->getPeer().getPort(), rendezvousSession->getPublicKcpPeer()->getPeer().getIP().c_str(), rendezvousSession->getPublicKcpPeer()->getPeer().getPort());
            
            std::string data;
            data = peer.getIP();
            data += seperator;
            data += std::to_string(peer.getPort());
            ijoon::send(kcpPeer->getKcp(), 0, ijoon::REGISTRATION_RENDEZVOUS_CLIENT_SUCCESS, (char *)data.c_str(), data.length());
            break;
        }
        case ijoon::CONNECTION_REQUEST: // from SP
        {
            ijn_print(DP_DEBUG, "received CONNECTION_REQUEST, body= %s", body);
        
            auto vec = ijoon::paramParser(body, 2);
            if(vec == nullptr) break;
            
            ijoon::Peer targetPeer(vec->at(0), vec->at(1));
            if(peer.getKey() == targetPeer.getKey() ||
               server->rendezvousSessionMap.count(peer.getKey()) == 0 ||
               server->rendezvousSessionMap.count(targetPeer.getKey()) == 0) {
                ijoon::send(kcpPeer->getKcp(), 0, ijoon::CONNECTION_FAILED, nullptr, 0);
                break;
            }
            
            auto sourceRendezvousSession = server->rendezvousSessionMap[peer.getKey()];
            auto targetRendezvousSession = server->rendezvousSessionMap[targetPeer.getKey()];
            
            // create connectionID
            if(server->connectionIDCursor > 1000000000) {
                server->connectionIDCursor = 1;
            }
            int connectionID = server->connectionIDCursor++;
            
            std::shared_ptr<ijoon::ConnectionInfo> connectionInfo = std::shared_ptr<ijoon::ConnectionInfo>(new ijoon::ConnectionInfo());
            connectionInfo->connectionID = connectionID;
            connectionInfo->sp.setIP(peer.getIP());
            connectionInfo->sp.setPort(std::to_string(peer.getPort()));
            connectionInfo->tp.setIP(targetPeer.getIP());
            connectionInfo->tp.setPort(std::to_string(targetPeer.getPort()));
            server->connectionInfoMap[connectionID] = connectionInfo;
            
            sourceRendezvousSession->setConnectionID(connectionID);
            targetRendezvousSession->setConnectionID(connectionID);
            messageHeader.connectionID = connectionID;
            if(server->relayServerMap.size() == 0) {
                ijn_print(DP_INFO, "[CONNECTION_REQUEST] no relay server");
                ijoon::send(kcpPeer->getKcp(), 0, ijoon::CONNECTION_RELAY_SERVICE_FAILED, nullptr, 0);
                connection(server, messageHeader);
                break;
            }
            
            // note: implement this (relay server selection algorithm)
            std::shared_ptr<ijoon::KcpPeer> relayKcpPeer;
            std::map<std::string, std::shared_ptr<ijoon::KcpPeer>>::iterator iter;
            for(iter = server->relayServerMap.begin(); iter != server->relayServerMap.end() ; ++iter ) {
                relayKcpPeer = iter->second;
                break;
            }
            
            sourceRendezvousSession->setRelayKcpPeer(relayKcpPeer);
            targetRendezvousSession->setRelayKcpPeer(relayKcpPeer);
            
            std::string data;
            data += sourceRendezvousSession->getPublicKcpPeer()->getPeer().getIP();
            data += seperator;
            data += vec->at(0);
            
            ijoon::send(relayKcpPeer->getKcp(), connectionID, ijoon::RELAY_SERVICE_REQUEST, (char *)data.c_str(), data.length());
            break;
        }
        default:
        {
            ijn_print(DP_ERROR, "Undefined message received");
            break;
        }
    }
}

void updateKcpObject(ijoon::RendezvousServer *server, std::shared_ptr<ijoon::KcpPeer> kcpPeer) {
    char *kcpBuffer = new char[MAX_PACKET_SIZE];
    
    IUINT32 current = iclock();
    if(current >= kcpPeer->next) {
        kcpPeer->mutex.lock();
        int rcvSize = ikcp_recv(kcpPeer->getKcp(), kcpBuffer, MAX_PACKET_SIZE);
        ikcp_update(kcpPeer->getKcp(), current);
        kcpPeer->next = ikcp_check(kcpPeer->getKcp(), current);
        kcpPeer->mutex.unlock();
        
        if(rcvSize > 0) {
             // callback to upper user
            kcpPeer->lastPing = current;
            kcpBuffer[rcvSize] = '\0';
            onCallback(server, kcpPeer, kcpBuffer, rcvSize);
        }
        
    }

    delete []kcpBuffer;
}

ijoon::THREAD_RET ijoon::recvThreadFunc(void *param) {
    auto thread = static_cast<ijoon::Thread *>(param);
    ijoon::RendezvousServer *server = (ijoon::RendezvousServer *)thread->getParam();
    
    while(!thread->isInterrupted()) {
        
        auto iter = server->kcpPeerMap.begin();
        for(; iter != server->kcpPeerMap.end() ; ++iter) {
            updateKcpObject(server, iter->second);
        }
        
        ijn_msleep(10);
    }
    
    return THREAD_EXIT;
}

ijoon::THREAD_RET ijoon::rawRecvThreadFunc(void *param) {
    auto thread = static_cast<ijoon::Thread *>(param);
    ijoon::RendezvousServer *server = (ijoon::RendezvousServer *)thread->getParam();
    
    ijoon::Peer peer;
    
    char *buffer = new char[MAX_PACKET_SIZE];
    
    while(true) {
        int rcvSize = server->socket->recvFrom(&peer, buffer, MAX_PACKET_SIZE);
        if(rcvSize < 0) continue;
        
        std::shared_ptr<ijoon::KcpPeer> kcpPeer;
        if(server->kcpPeerMap.count(peer.getKey()) != 0) {
            kcpPeer = server->kcpPeerMap.at(peer.getKey());
        }
        else {
            kcpPeer = std::shared_ptr<ijoon::KcpPeer>(new ijoon::KcpPeer(server->socket, peer.getIP(), std::to_string(peer.getPort()), udp_output));
            server->kcpPeerMap[peer.getKey()] = kcpPeer;
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
