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
    auto spRendezvousPeer = server->rendezvousPeerMap[connectionInfo->sp.getKey()];
    auto tpRendezvousPeer = server->rendezvousPeerMap[connectionInfo->tp.getKey()];
    
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
    const int checkCycleMs = 5000;
    while(!thread->isInterrupted())
    {
        thread->sleep(checkCycleMs);
        auto iter = server->rendezvousPeerMap.begin();
        for(; iter != server->rendezvousPeerMap.end() ; ++iter ) {
            time_t currentTime = ijoon::ComputableTime::getCurrentTimeSec();
            if(iter->second->lastPing + timeout < currentTime) {
                time_t lastPing = iter->second->lastPing;
                std::string publicIP = iter->second->getPublicKcpPeer()->getPeer().getIP();
                int publicPort = iter->second->getPublicKcpPeer()->getPeer().getPort();
                
                server->rendezvousPeerMap.erase(iter);
                ijn_print(DP_INFO, "rendezvous peer removed, %s:%d (%ud)", publicIP.c_str(), publicPort, lastPing);
            }
            break;
        }
        
        iter = server->relayServerMap.begin();
        for(; iter != server->relayServerMap.end() ; ++iter ) {
            time_t currentTime = ijoon::ComputableTime::getCurrentTimeSec();
            if(iter->second->lastPing + timeout < currentTime) {
                time_t lastPing = iter->second->lastPing;
                std::string publicIP = iter->second->getPublicKcpPeer()->getPeer().getIP();
                int publicPort = iter->second->getPublicKcpPeer()->getPeer().getPort();
                
                server->relayServerMap.erase(iter);
                ijn_print(DP_INFO, "relay peer removed, %s:%d (%ud)", publicIP.c_str(), publicPort, lastPing);
            }
            break;
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

void onCallback(ijoon::RendezvousServer *server, std::shared_ptr<ijoon::RendezvousSession> rendezvousSession, char *packet, int recvSize) {
    auto peer = rendezvousSession->getPublicKcpPeer()->getPeer();
    
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
    
    char *body = &packet[cursor];
    
    switch (messageHeader.packetType) {
        case ijoon::REGISTRATION_RELAY_SERVER_REQUEST: // from RelS
        {
            ijn_print(DP_DEBUG, "received REGISTRATION_RELAY_SERVER_REQUEST");
            
            std::string key = peer.getKey();
            if(server->anonymousPeerMap.count(key) != 0 && server->relayServerMap.count(key) == 0) {
                server->relayServerMap[key] = rendezvousSession;
//                server->anonymousPeerMap.erase(key);
            }
            
            ijoon::send(rendezvousSession->getPublicKcpPeer()->getKcp(), 0, ijoon::REGISTRATION_RELAY_SERVER_SUCCESS, nullptr, 0);
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
            
            auto sourcePeer = server->rendezvousPeerMap[connectionInfo->sp.getKey()];
            auto targetPeer = server->rendezvousPeerMap[connectionInfo->tp.getKey()];
            
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
                ijn_print(DP_ERROR, "[RELAY_SESSION_CREATED] relay connection info not exist");
                break;
            }
            auto connectionInfo = server->connectionInfoMap[messageHeader.connectionID];
            auto sourcePeer = server->rendezvousPeerMap[connectionInfo->sp.getKey()];
            auto targetPeer = server->rendezvousPeerMap[connectionInfo->tp.getKey()];
            
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
            auto sourcePeer = server->rendezvousPeerMap[connectionInfo->sp.getKey()];
            
            ijoon::send(sourcePeer->getPublicKcpPeer()->getKcp(), messageHeader.connectionID, ijoon::CONNECTION_RELAY_SERVICE_FAILED, nullptr, 0);
            connection(server, messageHeader);
            break;
        }
        case ijoon::REGISTRATION_RENDEZVOUS_CLIENT_REQUEST: // from SP, TP
        {
            ijn_print(DP_DEBUG, "received REGISTRATION_RENDEZVOUS_CLIENT_REQUEST");
            
            std::vector<std::string> vec;
            char *token = std::strtok((char *)body, &seperator);
            while (token != NULL) {
                vec.push_back(token);
                token = std::strtok(NULL, &seperator);
            }
            if(vec.size() != 2) {
                ijn_print(DP_ERROR, "[REGISTRATION_RENDEZVOUS_CLIENT_REQUEST] invalid parameters");
                break;
            }
            
            std::string key = peer.getKey();
            
            if( (server->anonymousPeerMap.count(key) != 0) && (server->rendezvousPeerMap.count(key) == 0)) {
                server->mutex.lock();
                server->rendezvousPeerMap[key] = rendezvousSession;
//                server->anonymousPeerMap.erase(key);
                ijn_print(DP_INFO, "[REGISTRATION_RENDEZVOUS_CLIENT_REQUEST] new rendezvous peer registered");
                
                server->mutex.unlock();
            }
            
            if(server->rendezvousPeerMap.count(key) == 0) {
                ijn_print(DP_INFO, "[REGISTRATION_RENDEZVOUS_CLIENT_REQUEST] invalid request");
                break;
            }
            
            rendezvousSession->setPrivateKcpPeer(vec[0], vec[1], udp_output);
            
            ijn_print(DP_INFO, "Register local=%s:%d, public=%s:%d", rendezvousSession->getPrivateKcpPeer()->getPeer().getIP().c_str(), rendezvousSession->getPrivateKcpPeer()->getPeer().getPort(), rendezvousSession->getPublicKcpPeer()->getPeer().getIP().c_str(), rendezvousSession->getPublicKcpPeer()->getPeer().getPort());
            
            std::string data;
            data = peer.getIP();
            data += seperator;
            data += std::to_string(peer.getPort());
            ijoon::send(rendezvousSession->getPublicKcpPeer()->getKcp(), 0, ijoon::REGISTRATION_RENDEZVOUS_CLIENT_SUCCESS, (char *)data.c_str(), data.length());
            rendezvousSession->send(ijoon::REGISTRATION_RENDEZVOUS_CLIENT_SUCCESS, (char *)data.c_str(), data.length());
            break;
        }
        case ijoon::CONNECTION_REQUEST: // from SP
        {
            ijn_print(DP_DEBUG, "received CONNECTION_REQUEST, body= %s", body);
            
            std::vector<std::string> vec;
            char *token = std::strtok((char *)body, &seperator);
            while (token != NULL) {
                vec.push_back(token);
                token = std::strtok(NULL, &seperator);
            }
            if(vec.size() != 2) {
                ijn_print(DP_ERROR, "[CONNECTION_REQUEST] invalid packet");
                break;
            }
            
            ijoon::Peer targetPeer(vec[0], vec[1]);
            auto sourceRendezvousPeer = server->rendezvousPeerMap[peer.getKey()];
            auto targetRendezvousPeer = server->rendezvousPeerMap[targetPeer.getKey()];
            if(targetPeer.getKey() == peer.getKey() || sourceRendezvousPeer == nullptr || targetRendezvousPeer == nullptr) {
                ijoon::send(rendezvousSession->getPublicKcpPeer()->getKcp(), 0, ijoon::CONNECTION_FAILED, nullptr, 0);
                break;
            }
            
            // create connectionID
            if(server->connectionIDCursor > 1000000000) {
                server->connectionIDCursor = 0;
            }
            int connectionID = server->connectionIDCursor++;
            
            std::shared_ptr<ijoon::ConnectionInfo> connectionInfo = std::shared_ptr<ijoon::ConnectionInfo>(new ijoon::ConnectionInfo());
            connectionInfo->connectionID = connectionID;
            connectionInfo->sp.setIP(peer.getIP());
            connectionInfo->sp.setPort(std::to_string(peer.getPort()));
            connectionInfo->tp.setIP(vec[0]);
            connectionInfo->tp.setPort(vec[1]);
            server->connectionInfoMap[connectionID] = connectionInfo;
            
            sourceRendezvousPeer->setConnectionID(connectionID);
            targetRendezvousPeer->setConnectionID(connectionID);
            messageHeader.connectionID = connectionID;
            if(server->relayServerMap.size() == 0) {
                ijn_print(DP_INFO, "[CONNECTION_REQUEST] no relay server");
                ijoon::send(rendezvousSession->getPublicKcpPeer()->getKcp(), 0, ijoon::CONNECTION_RELAY_SERVICE_FAILED, nullptr, 0);
                connection(server, messageHeader);
                break;
            }
            
            // note: implement this (relay server selection algorithm)
            std::shared_ptr<ijoon::RendezvousSession> relaySession;
            std::map<std::string, std::shared_ptr<ijoon::RendezvousSession>>::iterator iter;
            for(iter = server->relayServerMap.begin(); iter != server->relayServerMap.end() ; ++iter ) {
                relaySession = iter->second;
                break;
            }
            
            std::string data;
            data += sourceRendezvousPeer->getPublicKcpPeer()->getPeer().getIP();
            data += seperator;
            data += vec[0];
            
            ijoon::send(relaySession->getPublicKcpPeer()->getKcp(), connectionID, ijoon::RELAY_SERVICE_REQUEST, (char *)data.c_str(), data.length());
            break;
        }
        default:
        {
            ijn_print(DP_ERROR, "Undefined message received");
            break;
        }
    }
}

void updateKcpObject(ijoon::RendezvousServer *server, std::shared_ptr<ijoon::RendezvousSession> rendezvousSession) {
    char *kcpBuffer = new char[MAX_PACKET_SIZE];
    
    auto kcpPeer = rendezvousSession->getPublicKcpPeer();
    
    IUINT32 current = iclock();
    if(current >= kcpPeer->next) {
        
        kcpPeer->mutex.lock();
        int rcvSize = ikcp_recv(kcpPeer->getKcp(), kcpBuffer, MAX_PACKET_SIZE);
        ikcp_update(kcpPeer->getKcp(), current);
        kcpPeer->next = ikcp_check(kcpPeer->getKcp(), current);
        kcpPeer->mutex.unlock();
        
        if(rcvSize > 0) {
             // callback to upper user
            rendezvousSession->lastPing = ijoon::ComputableTime::getCurrentTimeSec();
            kcpBuffer[rcvSize] = '\0';
            onCallback(server, rendezvousSession, kcpBuffer, rcvSize);
        }
        
    }

    delete []kcpBuffer;
}

ijoon::THREAD_RET ijoon::recvThreadFunc(void *param) {
    auto thread = static_cast<ijoon::Thread *>(param);
    ijoon::RendezvousServer *server = (ijoon::RendezvousServer *)thread->getParam();
    
    while(!thread->isInterrupted()) {
        
        auto iter = server->anonymousPeerMap.begin();
        for(; iter != server->anonymousPeerMap.end() ; ++iter) {
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
        
        IUINT32 current = iclock();
        if(current > server->lastCheckTime + 3000) { // when dead peer check interval
            server->lastCheckTime = current;
            
            // peer's timeout check & remove
        }
        
        if(rcvSize < 0) continue;
        
        std::shared_ptr<ijoon::RendezvousSession> rendezvousSession = nullptr;
        if(server->anonymousPeerMap.count(peer.getKey()) != 0) {
            rendezvousSession = server->anonymousPeerMap.at(peer.getKey());
        }
        else {
            rendezvousSession = std::shared_ptr<ijoon::RendezvousSession>(new ijoon::RendezvousSession(server->socket, 0));
            rendezvousSession->setPublicKcpPeer(peer.getIP(), std::to_string(peer.getPort()), udp_output);
            rendezvousSession->lastPing = ijoon::ComputableTime::getCurrentTimeSec();
            server->anonymousPeerMap[peer.getKey()] = rendezvousSession;
        }
        
        rendezvousSession->getPublicKcpPeer()->mutex.lock();
        
        ikcpcb *kcp = rendezvousSession->getPublicKcpPeer()->getKcp();
        ikcp_input(kcp, buffer, rcvSize);
        current = iclock();
        ikcp_update(kcp, current);
        rendezvousSession->getPublicKcpPeer()->next = ikcp_check(kcp, current);
        
        rendezvousSession->getPublicKcpPeer()->mutex.unlock();
    }
    
    delete[] buffer;
    
    return THREAD_EXIT;
}
