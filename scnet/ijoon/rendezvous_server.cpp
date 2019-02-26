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
    checkThread = new ijoon::Thread(rendezvousCheckThreadFunc, "rendezvous check thread");
    checkThread->start(this);
}

std::shared_ptr<ijoon::KcpPeer> getKcpPeer(ijoon::RendezvousServer *server, std::shared_ptr<ijoon::Peer> peer) {
    std::shared_ptr<ijoon::KcpPeer> kcpPeer;
    if(server->kcpPeerMap.count(peer->getKey()) == 0) {
        kcpPeer = std::shared_ptr<ijoon::KcpPeer>(new ijoon::KcpPeer(server->socket, peer->getIP(), std::to_string(peer->getPort()), udp_output));
        server->kcpPeerMap[peer->getKey()] = kcpPeer;
    }
    else {
        kcpPeer = server->kcpPeerMap.at(peer->getKey());
    }
    
    return kcpPeer;
}

bool connection(ijoon::RendezvousServer *server, ijoon::MessageHeader messageHeader) {
    auto connectionInfo = server->connectionInfoMap[messageHeader.connectionID];
    
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
    auto sourceKcpPeer = getKcpPeer(server, publicSP);
    auto targetKcpPeer = getKcpPeer(server, publicTP);
    
    if(isTPPublic) { // pub/pub, pri/pub
        std::string data = publicTP->getIP() + seperator + std::to_string(publicTP->getPort()); // tp public address
        ijoon::send(sourceKcpPeer->getKcp(), messageHeader.connectionID, ijoon::RENDEZVOUS_MSG::DIRECT_CONNECTION_AVAILABLE, (char *)data.c_str(), data.length());
    }
    else if(isSPPublic && !isTPPublic) { // pub/pri
        std::string data = publicSP->getIP() + seperator + std::to_string(publicSP->getPort()); // sp public address
        ijoon::send(targetKcpPeer->getKcp(), messageHeader.connectionID, ijoon::RENDEZVOUS_MSG::REVERSE_CONNECTION, (char *)data.c_str(), data.length());
    }
    else { // pri/pri
        std::string data;
        
        data = publicTP->getIP(); // tp public address
        data += seperator;
        data += std::to_string(publicTP->getPort());
        data += seperator;
        data += privateTP->getIP();
        data += seperator;
        data += std::to_string(privateTP->getPort());
        ijoon::send(sourceKcpPeer->getKcp(), messageHeader.connectionID, ijoon::RENDEZVOUS_MSG::UDP_HOLE_PUNCHING_AVAILABLE, (char *)data.c_str(), data.length());
        
        data = publicSP->getIP(); // sp public address
        data += seperator;
        data += std::to_string(publicSP->getPort());
        data += seperator;
        data += privateSP->getIP();
        data += seperator;
        data += std::to_string(privateSP->getPort());
        ijoon::send(targetKcpPeer->getKcp(), messageHeader.connectionID, ijoon::RENDEZVOUS_MSG::UDP_HOLE_PUNCHING_AVAILABLE, (char *)data.c_str(), data.length());
    }
    
    server->connectionInfoMap.erase(messageHeader.connectionID);
    
    return true;
}

ijoon::THREAD_RET THREAD_API ijoon::rendezvousCheckThreadFunc(void *arg)
{
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::RendezvousServer *server = (ijoon::RendezvousServer *)thread->getParam();
    
    const int timeout = 180;
    const int checkIntervalMs = 5000;
    while(!thread->isInterrupted())
    {
        thread->sleep(checkIntervalMs);
        time_t currentTime = ijoon::ComputableTime::getCurrentTimeSec();
        
        auto iter = server->kcpPeerMap.begin();
        auto end = server->kcpPeerMap.end();
        while(iter != end) {
            std::string role = "";
            if(iter->second->lastPing + timeout < currentTime) {
                auto kcpPeer = iter->second;
                std::string key = kcpPeer->getPeer().getKey();
                if(server->rendezvousClientMap.count(key) != 0) {
                    server->rendezvousClientMap.erase(key);
                    role = "(RendezvousClient)";
                }
                if(server->relayServerMap.count(key) != 0) {
                    server->relayServerMap.erase(key);
                    role = "(RelayServer)";
                }
                iter = server->kcpPeerMap.erase(iter);
                ijn_print(DP_INFO, "kcpPeer%s removed, %s, (%u)", role.c_str(), key.c_str(), kcpPeer->lastPing);
            }
            else {
                ++iter;
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
    
    time_t currentTime = ijoon::ComputableTime::getCurrentTimeSec();
    kcpPeer->lastPing = currentTime;
    
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
            
            auto sourceKcpPeer = getKcpPeer(server, connectionInfo->publicSP);
            auto targetKcpPeer = getKcpPeer(server, connectionInfo->publicTP);
            
            std::string data;
            data = peer.getIP() + seperator + std::to_string(peer.getPort()) + seperator + "1";
            ijoon::send(sourceKcpPeer->getKcp(), messageHeader.connectionID, ijoon::RELAY_SERVER_INFORMATION, (char *)data.c_str(), data.length());
            data = peer.getIP() + seperator + std::to_string(peer.getPort()) + seperator + "0";
            ijoon::send(targetKcpPeer->getKcp(), messageHeader.connectionID, ijoon::RELAY_SERVER_INFORMATION, (char *)data.c_str(), data.length());
            
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
            
            auto sourceKcpPeer = getKcpPeer(server, connectionInfo->publicSP);
            auto targetKcpPeer = getKcpPeer(server, connectionInfo->publicTP);
            
            // send connected packet
            std::string data = peer.getIP() + seperator + std::to_string(peer.getPort());
            ijoon::send(sourceKcpPeer->getKcp(), messageHeader.connectionID, ijoon::CONNECTION_RELAY_SERVICE_SUCCESS, (char *)data.c_str(), data.length());
            ijoon::send(targetKcpPeer->getKcp(), messageHeader.connectionID, ijoon::CONNECTION_RELAY_SERVICE_SUCCESS, (char *)data.c_str(), data.length());
            
            connection(server, messageHeader);
            
            break;
        }
        case ijoon::RELAY_SESSION_CREATING_FAILED: // from RelS
        {
            ijn_print(DP_DEBUG, "received RELAY_SESSION_CREATING_FAILED");
            
            auto connectionInfo = server->connectionInfoMap[messageHeader.connectionID];
            
            auto sourceKcpPeer = getKcpPeer(server, connectionInfo->publicSP);
            
            ijoon::send(sourceKcpPeer->getKcp(), messageHeader.connectionID, ijoon::CONNECTION_RELAY_SERVICE_FAILED, nullptr, 0);
            connection(server, messageHeader);
            break;
        }
        case ijoon::REGISTRATION_RENDEZVOUS_CLIENT_REQUEST: // from SP, TP
        {
            ijn_print(DP_DEBUG, "received REGISTRATION_RENDEZVOUS_CLIENT_REQUEST");
            
            auto vec = ijoon::paramParser(body, 2);
            if(vec == nullptr) break;
            
            std::string key = peer.getKey();
            std::shared_ptr<ijoon::Peer> privatePeer;
            if(server->rendezvousClientMap.count(key) == 0) {
                privatePeer = std::shared_ptr<ijoon::Peer>(new ijoon::Peer(vec->at(0), vec->at(1)));
                server->rendezvousClientMap[key] = privatePeer;
            }
            else {
                privatePeer = server->rendezvousClientMap[key];
                if(privatePeer->getIP() != vec->at(0) || privatePeer->getPort() != atoi(vec->at(1).c_str())) {
                    privatePeer->setIP(vec->at(0));
                    privatePeer->setPort(vec->at(1));
                }
            }
            
            ijn_print(DP_INFO, "Registered client info: private=%s, public=%s", privatePeer->getKey().c_str(), peer.getKey().c_str());
            
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
            
            // create connectionID
            if(server->connectionIDCursor > 1000000000) {
                server->connectionIDCursor = 1;
            }
            int connectionID = server->connectionIDCursor++;
            
            auto targetPeer = std::shared_ptr<ijoon::Peer>(new ijoon::Peer(vec->at(0), vec->at(1)));
            if(peer.getKey() == targetPeer->getKey() ||
               server->rendezvousClientMap.count(peer.getKey()) == 0 ||
               server->rendezvousClientMap.count(targetPeer->getKey()) == 0) {
                ijoon::send(kcpPeer->getKcp(), connectionID, ijoon::CONNECTION_FAILED, nullptr, 0);
                break;
            }
            
            std::shared_ptr<ijoon::ConnectionInfo> connectionInfo = std::shared_ptr<ijoon::ConnectionInfo>(new ijoon::ConnectionInfo());
            connectionInfo->connectionID = connectionID;
            connectionInfo->publicSP = std::shared_ptr<ijoon::Peer>(new ijoon::Peer(peer.getIP(), std::to_string(peer.getPort())));
            connectionInfo->privateSP = server->rendezvousClientMap.at(peer.getKey());
            connectionInfo->publicTP = targetPeer;
            connectionInfo->privateTP = server->rendezvousClientMap.at(targetPeer->getKey());
            server->connectionInfoMap[connectionID] = connectionInfo;
            
            messageHeader.connectionID = connectionID;
            if(server->relayServerMap.size() == 0) {
                ijn_print(DP_INFO, "[CONNECTION_REQUEST] no relay server");
                ijoon::send(kcpPeer->getKcp(), connectionID, ijoon::CONNECTION_RELAY_SERVICE_FAILED, nullptr, 0);
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

            std::string data;
            data += peer.getIP();
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
    
    auto peer = std::shared_ptr<ijoon::Peer>(new ijoon::Peer());
    
    char *buffer = new char[MAX_PACKET_SIZE];
    
    while(true) {
        int rcvSize = server->socket->recvFrom(peer.get(), buffer, MAX_PACKET_SIZE);
        if(rcvSize < 0) continue;
        
        auto kcpPeer = getKcpPeer(server, peer);
        
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
