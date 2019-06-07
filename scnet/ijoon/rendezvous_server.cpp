#include "rendezvous_server.h"
#include "rendezvous_message.h"
#include "message_header.h"
#include <cstring>
#include "ikcp.h"
#include "registry.h"
#include "utils.h"

extern char seperator;

ijoon::THREAD_RET THREAD_API rendezvousCheckThreadFunc(void *arg);
ijoon::THREAD_RET THREAD_API recvThreadFunc(void *arg);

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

    recvThread = new ijoon::Thread(recvThreadFunc, "recv thread");
    recvThread->start(this);
    checkThread = new ijoon::Thread(rendezvousCheckThreadFunc, "rendezvous check thread");
    checkThread->start(this);
}

std::shared_ptr<ijoon::KcpPeer> ijoon::RendezvousServer::getKcpPeer(std::shared_ptr<ijoon::Peer> peer) {
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

bool connection(ijoon::RendezvousServer *server, ijoon::MessageHeader messageHeader) {
    server->mutexForConnectionInfoMap.lock();
    auto connectionInfo = server->connectionInfoMap[messageHeader.connectionID];
    server->mutexForConnectionInfoMap.unlock();
    
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
    auto sourceKcpPeer = server->getKcpPeer(publicSP);
    auto targetKcpPeer = server->getKcpPeer(publicTP);
    
    if(isTPPublic) { // pub/pub, pri/pub
        std::string data = publicTP->getIP() + seperator + std::to_string(publicTP->getPort()); // tp public address
        ijoon::send(sourceKcpPeer, messageHeader.connectionID, ijoon::RENDEZVOUS_MSG::DIRECT_CONNECTION_AVAILABLE, (char *)data.c_str(), data.length());
    }
    else if(isSPPublic && !isTPPublic) { // pub/pri
        std::string data = publicSP->getIP() + seperator + std::to_string(publicSP->getPort()); // sp public address
        ijoon::send(targetKcpPeer, messageHeader.connectionID, ijoon::RENDEZVOUS_MSG::REVERSE_CONNECTION, (char *)data.c_str(), data.length());
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
        ijoon::send(sourceKcpPeer, messageHeader.connectionID, ijoon::RENDEZVOUS_MSG::UDP_HOLE_PUNCHING_AVAILABLE, (char *)data.c_str(), data.length());
        
        data = publicSP->getIP(); // sp public address
        data += seperator;
        data += std::to_string(publicSP->getPort());
        data += seperator;
        data += privateSP->getIP();
        data += seperator;
        data += std::to_string(privateSP->getPort());
        ijoon::send(targetKcpPeer, messageHeader.connectionID, ijoon::RENDEZVOUS_MSG::UDP_HOLE_PUNCHING_AVAILABLE, (char *)data.c_str(), data.length());
    }
    
    server->mutexForConnectionInfoMap.lock();
    server->connectionInfoMap.erase(messageHeader.connectionID);
    server->mutexForConnectionInfoMap.unlock();
    return true;
}

ijoon::THREAD_RET THREAD_API rendezvousCheckThreadFunc(void *arg)
{
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::RendezvousServer *server = (ijoon::RendezvousServer *)thread->getParam();
    
    const int timeout = 180;
    const int checkIntervalMs = 5 * 1000;
    const int pingIntervalSec = 30;
    while(!thread->isInterrupted())
    {
        thread->sleep(checkIntervalMs);
        time_t currentTime = ijoon::ComputableTime::getCurrentTimeSec();
        
        server->mutexForKcpPeerMap.lock();
        auto iter = server->kcpPeerMap.begin();
        auto end = server->kcpPeerMap.end();
        while(iter != end) {
            std::string role = "";
            if(iter->second->lastPing + timeout < currentTime) {
                auto kcpPeer = iter->second;
                std::string key = kcpPeer->getPeer().getKey();
                auto privatePeer = server->getRendezvousClient(kcpPeer->getPeer().getIP(), std::to_string(kcpPeer->getPeer().getPort()));
                if(privatePeer != nullptr) {
                    server->removeRendezvousClient(kcpPeer->getPeer().getIP(), std::to_string(kcpPeer->getPeer().getPort()));
                    role = "(RendezvousClient)";
                }
                if(server->isExistRelayServer(kcpPeer->getPeer().getIP(), std::to_string(kcpPeer->getPeer().getPort()))) {
                    server->removeRelayServer(kcpPeer->getPeer().getIP(), std::to_string(kcpPeer->getPeer().getPort()));
                    role = "(RelayServer)";
                }
                iter = server->kcpPeerMap.erase(iter);
                ijn_print(DP_INFO, "kcpPeer%s removed, %s, (%u)", role.c_str(), key.c_str(), kcpPeer->lastPing);
                continue;
            }
            else if(iter->second->lastPing + pingIntervalSec < currentTime) {
                ijoon::send(iter->second, 0, ijoon::PING_REQUEST, nullptr, 0);
            }
            
            ++iter;
        }
        server->mutexForKcpPeerMap.unlock();
    }
    
    return THREAD_EXIT;
}

void onCallback(ijoon::RendezvousServer *server, std::shared_ptr<ijoon::KcpPeer> kcpPeer, char *packet, int recvSize) {
    auto peer = kcpPeer->getPeer();
    ijoon::MessageHeader messageHeader;
    int cursor = 0;
    if(!ijoon::readHeader(packet, recvSize, messageHeader, cursor)) {
        ijn_print(DP_ERROR, "invalid header");
        return;
    }
    
    if((recvSize-cursor) != messageHeader.dataSize) {
        ijn_print(DP_ERROR, "invalid data size");
        return;
    }
    
    char *body = &packet[cursor];
    
    kcpPeer->lastPing = ijoon::ComputableTime::getCurrentTimeSec();
    auto registry = Registry<int, google::protobuf::Message *>().Get();
    switch (messageHeader.messageType) {
        case ijoon::PROTOBUF:
        {
            google::protobuf::Message *message = registry->Create(messageHeader.packetType);
            if(message == nullptr) {
                ijn_print(DP_INFO, "Unknown protobuf packet_type(%d) reveiced", messageHeader.packetType);
                return;
            }
            message->ParseFromArray(body, messageHeader.dataSize);
            auto callbackWrapper = registry->GetCallbackWrapper(messageHeader.messageType, messageHeader.packetType);
            if(callbackWrapper != nullptr) {
                auto rendezvousSession = std::shared_ptr<ijoon::RendezvousSession>(new ijoon::RendezvousSession(server->socket, 0));
                rendezvousSession->setPublicKcpPeer(kcpPeer);
                callbackWrapper->callback(rendezvousSession, message);
            }
            else {
                ijn_print(DP_ERROR, "No callback wrapper");
            }
            
            delete message;
            return;
        }
        case ijoon::RAWBYTE:
        {
            if(messageHeader.packetType >= ijoon::REGISTRATION_RENDEZVOUS_CLIENT_REQUEST) {
                break;
            }
            
            auto callbackWrapper = registry->GetCallbackWrapper(messageHeader.messageType, messageHeader.packetType);
            if(callbackWrapper != nullptr) {
                auto rendezvousSession = std::shared_ptr<ijoon::RendezvousSession>(new ijoon::RendezvousSession(server->socket, 0));
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
        ijn_print(DP_ERROR, "Unknown message_type(%d) received", messageHeader.messageType);
        return;
    }
    
    switch (messageHeader.packetType) {
        case ijoon::REGISTRATION_RELAY_SERVER_REQUEST: // from RelS
        {
            ijn_print(DP_DEBUG, "received REGISTRATION_RELAY_SERVER_REQUEST");
            
            server->registerRelayServer("unknown", kcpPeer->getPeer().getIP(), std::to_string(kcpPeer->getPeer().getPort()), "0.1");
            
            ijoon::send(kcpPeer, 0, ijoon::REGISTRATION_RELAY_SERVER_SUCCESS, nullptr, 0);
            break;
        }
        case ijoon::RELAY_SESSION_READY: // from RelS
        {
            ijn_print(DP_DEBUG, "received RELAY_SESSION_READY");
            
            server->mutexForConnectionInfoMap.lock();
            if(server->connectionInfoMap.count(messageHeader.connectionID) == 0) {
                ijn_print(DP_ERROR, "[RELAY_SESSION_READY] invalid request from relay server");
                server->mutexForConnectionInfoMap.unlock();
                break;
            }
            
            auto connectionInfo = server->connectionInfoMap[messageHeader.connectionID];
            server->mutexForConnectionInfoMap.unlock();
            
            auto sourceKcpPeer = server->getKcpPeer(connectionInfo->publicSP);
            auto targetKcpPeer = server->getKcpPeer(connectionInfo->publicTP);
            
            std::string data;
            data = peer.getIP() + seperator + std::to_string(peer.getPort()) + seperator + "1";
            ijoon::send(sourceKcpPeer, messageHeader.connectionID, ijoon::RELAY_SERVER_INFORMATION, (char *)data.c_str(), data.length());
            
            data = peer.getIP() + seperator + std::to_string(peer.getPort()) + seperator + "0";
            ijoon::send(targetKcpPeer, messageHeader.connectionID, ijoon::RELAY_SERVER_INFORMATION, (char *)data.c_str(), data.length());
            
            break;
        }
        case ijoon::RELAY_SESSION_CREATED: // from RelS
        {
            ijn_print(DP_DEBUG, "received RELAY_SESSION_CREATED");
            
            server->mutexForConnectionInfoMap.lock();
            if(server->connectionInfoMap.count(messageHeader.connectionID) == 0) {
                ijn_print(DP_ERROR, "[RELAY_SESSION_CREATED] relay connection info not exist, %d", messageHeader.connectionID);
                server->mutexForConnectionInfoMap.unlock();
                break;
            }
            auto connectionInfo = server->connectionInfoMap[messageHeader.connectionID];
            server->mutexForConnectionInfoMap.unlock();
            
            auto sourceKcpPeer = server->getKcpPeer(connectionInfo->publicSP);
            auto targetKcpPeer = server->getKcpPeer(connectionInfo->publicTP);
            
            // send connected packet
            std::string data = peer.getIP() + seperator + std::to_string(peer.getPort());
            ijoon::send(sourceKcpPeer, messageHeader.connectionID, ijoon::CONNECTION_RELAY_SERVICE_SUCCESS, (char *)data.c_str(), data.length());
            ijoon::send(targetKcpPeer, messageHeader.connectionID, ijoon::CONNECTION_RELAY_SERVICE_SUCCESS, (char *)data.c_str(), data.length());
            
            connection(server, messageHeader);
            
            break;
        }
        case ijoon::REGISTRATION_RENDEZVOUS_CLIENT_REQUEST: // from SP, TP
        {
            ijn_print(DP_DEBUG, "received REGISTRATION_RENDEZVOUS_CLIENT_REQUEST");
            
            auto vec = ijoon::paramParser(body, 3);
            if(vec == nullptr) break;
            
            // Callback to user (serial, public address, private address)
            if(server->registerRendezvousClient != nullptr) {
                std::string localIP = vec->at(0);
                std::string localPort = vec->at(1);
                std::string serial = vec->at(2);
                server->registerRendezvousClient(serial, peer.getIP(), std::to_string(peer.getPort()), localIP, localPort);
            }
            
            ijn_print(DP_INFO, "Registered client info: private=%s:%s, public=%s", vec->at(0).c_str(), vec->at(1).c_str(), peer.getKey().c_str());
            
            std::string data;
            data = peer.getIP();
            data += seperator;
            data += std::to_string(peer.getPort());
            ijoon::send(kcpPeer, 0, ijoon::REGISTRATION_RENDEZVOUS_CLIENT_SUCCESS, (char *)data.c_str(), data.length());
            break;
        }
        case ijoon::CONNECTION_REQUEST: // from SP
        {
            ijn_print(DP_DEBUG, "received CONNECTION_REQUEST, body= %s", body);
        
            std::string bodyStr = body;
            auto vec = ijoon::paramParser(body, 2);
            if(vec == nullptr) break;
            
            auto sourcePrivatePeer = server->getRendezvousClient(peer.getIP(), std::to_string(peer.getPort()));
            auto targetPrivatePeer = server->getRendezvousClient(vec->at(0), vec->at(1));
            auto targetPeer = std::shared_ptr<ijoon::Peer>(new ijoon::Peer(vec->at(0), vec->at(1)));
            if(peer.getKey() == targetPeer->getKey() ||
               sourcePrivatePeer == nullptr ||
               targetPrivatePeer == nullptr) {
                ijoon::send(kcpPeer, 0, ijoon::CONNECTION_TARGET_INVALID, (char *)bodyStr.c_str(), bodyStr.size());
                break;
            }
            
            // create connectionID
            if(server->connectionIDCursor > 1000000000) {
                server->connectionIDCursor = 1;
            }
            int connectionID = server->connectionIDCursor++;
            
            std::shared_ptr<ijoon::ConnectionInfo> connectionInfo = std::shared_ptr<ijoon::ConnectionInfo>(new ijoon::ConnectionInfo());
            connectionInfo->connectionID = connectionID;
            connectionInfo->publicSP = std::shared_ptr<ijoon::Peer>(new ijoon::Peer(peer.getIP(), std::to_string(peer.getPort())));
            connectionInfo->privateSP = sourcePrivatePeer;
            connectionInfo->publicTP = targetPeer;
            connectionInfo->privateTP = targetPrivatePeer;
            server->mutexForConnectionInfoMap.lock();
            server->connectionInfoMap[connectionID] = connectionInfo;
            server->mutexForConnectionInfoMap.unlock();
            
            messageHeader.connectionID = connectionID;
            
            ijoon::send(kcpPeer, connectionID, ijoon::CONNECTION_ID_CREATED, (char *)bodyStr.c_str(), bodyStr.size());
            return;
        }
        case ijoon::CONNECTION_ID_RECEIVED:
        {
            auto relayServerPeer = server->getRelayServerPeer();
            if(relayServerPeer == nullptr) {
                ijn_print(DP_INFO, "[CONNECTION_ID_RECEIVED] no relay server");
                ijoon::send(kcpPeer, messageHeader.connectionID, ijoon::CONNECTION_RELAY_SERVICE_FAILED, nullptr, 0);
                connection(server, messageHeader);
                break;
            }
            
            server->mutexForConnectionInfoMap.lock();
            if(server->connectionInfoMap.count(messageHeader.connectionID) == 0) {
                ijn_print(DP_ERROR, "[CONNECTION_ID_RECEIVED] relay connection info not exist, %d", messageHeader.connectionID);
                server->mutexForConnectionInfoMap.unlock();
                break;
            }
            auto connectionInfo = server->connectionInfoMap[messageHeader.connectionID];
            server->mutexForConnectionInfoMap.unlock();
            
            auto relayKcpPeer = server->getKcpPeer(relayServerPeer);

            std::string data;
            data += peer.getIP();
            data += seperator;
            data += connectionInfo->publicTP->getIP().c_str();
            
            ijoon::send(relayKcpPeer, messageHeader.connectionID, ijoon::RELAY_SERVICE_REQUEST, (char *)data.c_str(), data.length());
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

ijoon::THREAD_RET THREAD_API recvThreadFunc(void *param) {
    auto thread = static_cast<ijoon::Thread *>(param);
    ijoon::RendezvousServer *server = (ijoon::RendezvousServer *)thread->getParam();
    
    auto peer = std::shared_ptr<ijoon::Peer>(new ijoon::Peer());
    
    char *rawBuffer = new char[MAX_PACKET_SIZE];
    char *buffer = new char[MAX_PACKET_SIZE];
    
    server->socket->option(ijoon::SocketOptionType::SOCK_RCVTIMEO_MS, 1);
    
    while(!thread->isInterrupted()) {
        int rcvSize = server->socket->recvFrom(peer.get(), rawBuffer, MAX_PACKET_SIZE);
        if(rcvSize > 0) {
            server->mutexForKcpPeerMap.lock();
            auto kcpPeer = server->getKcpPeer(peer);
            server->mutexForKcpPeerMap.unlock();
            
            kcpPeer->mutex.lock();
            ikcpcb *kcp = kcpPeer->getKcp();
            ikcp_input(kcp, rawBuffer, rcvSize);
            IUINT32 current = iclock();
            ikcp_update(kcp, current);
            kcpPeer->mutex.unlock();
        }
        
        server->mutexForKcpPeerMap.lock();
        
        IUINT32 current = iclock();
        auto iter = server->kcpPeerMap.begin();
        for(; iter != server->kcpPeerMap.end() ; ++iter) {
            auto kcpPeer = iter->second;
            kcpPeer->mutex.lock();
            ikcpcb *kcp = kcpPeer->getKcp();
            int rcvSize = ikcp_recv(kcp, buffer, MAX_PACKET_SIZE);
            ikcp_update(kcp, current);
            kcpPeer->mutex.unlock();
            
            if(rcvSize > 0) {
                // callback to upper user
                buffer[rcvSize] = '\0';
                onCallback(server, kcpPeer, buffer, rcvSize);
            }
        }
        
        server->mutexForKcpPeerMap.unlock();
        
        ijn_msleep(10);
    }
    
    return THREAD_EXIT;
}
