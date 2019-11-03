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
    
    try {
        kcpPeer = this->kcpPeerMap.at(peer->getKey());
    } catch (std::exception e) {
        kcpPeer = std::shared_ptr<ijoon::KcpPeer>(new ijoon::KcpPeer(this->socket, peer->getIP(), std::to_string(peer->getPort()), udp_output));
        this->kcpPeerMap[peer->getKey()] = kcpPeer;
    }
    
    return kcpPeer;
}

bool connection(ijoon::RendezvousServer *server, uint connectionID) {
    
    server->mutexForConnectionInfoMap.lock();
    auto connectionInfo = server->connectionInfoMap[connectionID];
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
        std::string data = std::to_string(connectionID) +
                           seperator +
                           publicTP->getIP() +
                           seperator +
                           std::to_string(publicTP->getPort()); // tp public address
        sourceKcpPeer->send(ijoon::RENDEZVOUS_MSG::DIRECT_CONNECTION_AVAILABLE,
                            (char *)data.c_str(),
                            data.length());
    }
    else if(isSPPublic) { // pub/pri
        //TODO: REVERSE_CONNECTION_READY 프로토콜 삭제 검토
        
        std::string data;
        data = std::to_string(connectionID);
        sourceKcpPeer->send(ijoon::RENDEZVOUS_MSG::REVERSE_CONNECTION_READY,
        (char *)data.c_str(),
        data.length());
        
        data = std::to_string(connectionID) +
               seperator +
               publicSP->getIP() +
               seperator +
               std::to_string(publicSP->getPort()); // sp public address
        targetKcpPeer->send(ijoon::RENDEZVOUS_MSG::REVERSE_CONNECTION,
                            (char *)data.c_str(),
                            data.length());
    }
    else { // pri/pri
        std::string data;
        
        data = std::to_string(connectionID);
        data += seperator;
        data += publicTP->getIP(); // tp public address
        data += seperator;
        data += std::to_string(publicTP->getPort());
        data += seperator;
        data += privateTP->getIP();
        data += seperator;
        data += std::to_string(privateTP->getPort());
        sourceKcpPeer->send(ijoon::RENDEZVOUS_MSG::UDP_HOLE_PUNCHING_AVAILABLE,
                            (char *)data.c_str(),
                            data.length());
        
        data = std::to_string(connectionID);
        data += seperator;
        data += publicSP->getIP(); // sp public address
        data += seperator;
        data += std::to_string(publicSP->getPort());
        data += seperator;
        data += privateSP->getIP();
        data += seperator;
        data += std::to_string(privateSP->getPort());
        targetKcpPeer->send(ijoon::RENDEZVOUS_MSG::UDP_HOLE_PUNCHING_AVAILABLE,
                            (char *)data.c_str(),
                            data.length());
    }
    
    server->mutexForConnectionInfoMap.lock();
    server->connectionInfoMap.erase(connectionID);
    server->mutexForConnectionInfoMap.unlock();
    return true;
}

ijoon::THREAD_RET THREAD_API rendezvousCheckThreadFunc(void *arg)
{
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::RendezvousServer *server = (ijoon::RendezvousServer *)thread->getParam();
    
    const int timeout = 60;
    const int checkIntervalMs = 5 * 1000;
    while(!thread->isInterrupted())
    {
        thread->sleep(checkIntervalMs);
        time_t currentTime = ijoon::ComputableTime::getCurrentTimeSec();
        
        server->mutexForKcpPeerMap.lock();
        auto iter = server->kcpPeerMap.begin();
        auto end = server->kcpPeerMap.end();
        while(iter != end) {
            auto kcpPeer = iter->second;
            if(kcpPeer->lastPing + timeout < currentTime) {
                kcpPeer->mutex.lock();
                switch(kcpPeer->type) {
                    case ijoon::PeerType::RELAY_SERVER:
                    {
                        server->callback.removeRelayServerCallback(kcpPeer->getPeer().getIP(), std::to_string(kcpPeer->getPeer().getPort()));
                    }
                        break;
                    case ijoon::PeerType::RENDEZVOUS_CLIENT:
                    {
                        
                        server->callback.removeRendezvousClientCallback(kcpPeer->getPeer().getIP(), std::to_string(kcpPeer->getPeer().getPort()));
                    }
                        break;
                    default:
                        break;
                }
                
                kcpPeer->status = ijoon::PeerStatus::UNREGISTERED;
                iter = server->kcpPeerMap.erase(iter);
                ijn_print(DP_INFO, "kcpPeer(type=%d, %s) removed (%d)", kcpPeer->type, kcpPeer->getPeer().getKey().c_str(), server->kcpPeerMap.size());
                
                kcpPeer->mutex.unlock();
                continue;
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
    
    auto registry = Registry<int, google::protobuf::Message *>().Get();
    switch (messageHeader.messageType) {
        case ijoon::PROTOBUF:
        {
            if(kcpPeer->type == ijoon::PeerType::NONE) {
                ijn_print(DP_ERROR, "Not registered peer (%s)", kcpPeer->getPeer().getKey().c_str());
                return;
            }
            else if(kcpPeer->status == ijoon::PeerStatus::NOT_COMPATIBLE) {
                ijn_print(DP_ERROR, "Not compatible peer (%s), %d", kcpPeer->getPeer().getKey().c_str(), messageHeader.packetType);
                return;
            }
            
            kcpPeer->lastPing = ijoon::ComputableTime::getCurrentTimeSec();
            
            google::protobuf::Message *message = registry->Create(messageHeader.packetType);
            if(message == nullptr) {
                ijn_print(DP_INFO, "Unknown protobuf packet_type(%d) reveiced", messageHeader.packetType);
                return;
            }
            message->ParseFromArray(body, messageHeader.dataSize);
            auto callbackWrapper = registry->GetCallbackWrapper(messageHeader.messageType, messageHeader.packetType);
            if(callbackWrapper != nullptr) {
                callbackWrapper->callback(kcpPeer, message);
            }
            else {
                ijn_print(DP_ERROR, "No callback wrapper");
            }
            
            delete message;
            return;
        }
        case ijoon::RAWBYTE:
        {
            if(messageHeader.packetType == ijoon::REGISTRATION_RENDEZVOUS_CLIENT_REQUEST) {
                ijn_print(DP_DEBUG, "received REGISTRATION_RENDEZVOUS_CLIENT_REQUEST");
                
                auto vec = ijoon::paramParser(body, 5);
                if(vec == nullptr) break;
                
                // Callback to user (serial, public address, private address, MAC, version)
                
                std::string localIP = vec->at(0);
                std::string localPort = vec->at(1);
                std::string serial = vec->at(2);
                std::string mac = vec->at(3);
                std::string version = vec->at(4);
                if(server->callback.registerRendezvousClientCallback(serial, peer.getIP(), std::to_string(peer.getPort()), localIP, localPort, mac, version)) {
                    ijn_print(DP_INFO, "Registered client info: private=%s:%s, public=%s, serial=%s, mac=%s, version=%s",
                              localIP.c_str(), localPort.c_str(), peer.getKey().c_str(),
                              serial.c_str(), mac.c_str(), version.c_str());
                    
                    kcpPeer->type = ijoon::PeerType::RENDEZVOUS_CLIENT;
                    kcpPeer->status = ijoon::PeerStatus::REGISTERED;
                    
                    //TODO: 버전에 따라 status 할당 (0, -1, -2)
                    
                    std::string data;
                    data = "1";
                    data += seperator;
                    data += peer.getIP();
                    data += seperator;
                    data += std::to_string(peer.getPort());
                    kcpPeer->send(ijoon::REGISTRATION_RENDEZVOUS_CLIENT_RESPONSE,
                                  (char *)data.c_str(),
                                  data.length());
                }
                else {
                    ijn_print(DP_INFO, "Failed registration rendezvous client");
                    
                    std::string data;
                    data = "-3";
                    data += seperator;
                    data += peer.getIP();
                    data += seperator;
                    data += std::to_string(peer.getPort());
                    kcpPeer->send(ijoon::REGISTRATION_RENDEZVOUS_CLIENT_RESPONSE,
                                  (char *)data.c_str(),
                                  data.length());
                }
                
                return;
            }
            else if(messageHeader.packetType == ijoon::REGISTRATION_RELAY_SERVER_REQUEST) {
                ijn_print(DP_DEBUG, "received REGISTRATION_RELAY_SERVER_REQUEST");
                
                kcpPeer->type = ijoon::PeerType::RELAY_SERVER;
                kcpPeer->status = ijoon::PeerStatus::REGISTERED;
                
                if(server->callback.registerRelayServerCallback("unknown", kcpPeer->getPeer().getIP(), std::to_string(kcpPeer->getPeer().getPort()), "0.1")) {
                    std::string data;
                    data = "1";
                    kcpPeer->send(ijoon::REGISTRATION_RELAY_SERVER_RESPONSE,
                                  (char *)data.c_str(),
                                  data.length());
                }
                else {
                    ijn_print(DP_INFO, "Failed registration relay server");
                    
                    std::string data;
                    data = "-3";
                    kcpPeer->send(ijoon::REGISTRATION_RELAY_SERVER_RESPONSE,
                                  (char *)data.c_str(),
                                  data.length());
                }
                
                return;
            }
            
            if(kcpPeer->type == ijoon::PeerType::NONE) {
                ijn_print(DP_ERROR, "Not registered peer (%s), %d", kcpPeer->getPeer().getKey().c_str(), messageHeader.packetType);
                return;
            }
            
            switch(kcpPeer->status) {
                case ijoon::PeerStatus::REGISTERED:
                case ijoon::PeerStatus::COMPATIBLE:
                    // Allowed
                    break;
                case ijoon::PeerStatus::NOT_COMPATIBLE:
                    // Not allowed but ping update
                    kcpPeer->lastPing = ijoon::ComputableTime::getCurrentTimeSec();
                    return;
                case ijoon::PeerStatus::NONE:
                case ijoon::PeerStatus::UNREGISTERED:
                    // Not allowed
                    return;
                default:
                    ijn_print(DP_ERROR, "Unknown peer status (%d)", kcpPeer->status);
                    break;
            }
            
            kcpPeer->lastPing = ijoon::ComputableTime::getCurrentTimeSec();
            
            if(messageHeader.packetType >= ijoon::REGISTRATION_RENDEZVOUS_CLIENT_REQUEST) {
                break;
            }
            
            auto callbackWrapper = registry->GetCallbackWrapper(messageHeader.messageType, messageHeader.packetType);
            if(callbackWrapper != nullptr) {
                callbackWrapper->callback(kcpPeer, body, messageHeader.dataSize);
            }
            else {
                printf("Not registered raw message received. body=%s", body);
            }
            
            return;
        }
        default:
        {
            ijn_print(DP_ERROR, "Unknown message type received, type=%d", messageHeader.messageType);
            return;
        }
    }
    
    switch (messageHeader.packetType) {
        case ijoon::RELAY_SESSION_READY: // from RelS
        {
            ijn_print(DP_DEBUG, "received RELAY_SESSION_READY");
            
            auto vec = ijoon::paramParser(body, 1);
            if(vec == nullptr) break;
            auto connectionIDStr = vec->at(0);
            auto connectionID = static_cast<uint>(atoi(connectionIDStr.c_str()));
            
            server->mutexForConnectionInfoMap.lock();
            if(server->connectionInfoMap.count(connectionID) == 0) {
                ijn_print(DP_ERROR, "[RELAY_SESSION_READY] invalid request from relay server");
                server->mutexForConnectionInfoMap.unlock();
                break;
            }
            
            auto connectionInfo = server->connectionInfoMap[connectionID];
            server->mutexForConnectionInfoMap.unlock();
            
            auto sourceKcpPeer = server->getKcpPeer(connectionInfo->publicSP);
            auto targetKcpPeer = server->getKcpPeer(connectionInfo->publicTP);
            
            std::string data;
            data = connectionIDStr + seperator + peer.getIP() + seperator + std::to_string(peer.getPort()) + seperator + "1";
            sourceKcpPeer->send(ijoon::RELAY_SERVER_INFORMATION,
                          (char *)data.c_str(),
                          data.length());
            
            data = connectionIDStr + seperator + peer.getIP() + seperator + std::to_string(peer.getPort()) + seperator + "0";
            targetKcpPeer->send(ijoon::RELAY_SERVER_INFORMATION,
                          (char *)data.c_str(),
                          data.length());
            
            break;
        }
        case ijoon::RELAY_SESSION_CREATED: // from RelS
        {
            ijn_print(DP_DEBUG, "received RELAY_SESSION_CREATED");
            
            auto vec = ijoon::paramParser(body, 1);
            if(vec == nullptr) break;
            auto connectionIDStr = vec->at(0);
            auto connectionID = static_cast<uint>(atoi(connectionIDStr.c_str()));
            
            server->mutexForConnectionInfoMap.lock();
            if(server->connectionInfoMap.count(connectionID) == 0) {
                ijn_print(DP_ERROR, "[RELAY_SESSION_CREATED] relay connection info not exist, %d", connectionID);
                server->mutexForConnectionInfoMap.unlock();
                break;
            }
            auto connectionInfo = server->connectionInfoMap[connectionID];
            server->mutexForConnectionInfoMap.unlock();
            
            auto sourceKcpPeer = server->getKcpPeer(connectionInfo->publicSP);
            auto targetKcpPeer = server->getKcpPeer(connectionInfo->publicTP);
            
            // send connected packet
            std::string data = std::string("1") + seperator + connectionIDStr + seperator + peer.getIP() + seperator + std::to_string(peer.getPort());
            
            sourceKcpPeer->send(ijoon::CONNECTION_RELAY_SERVICE_RESULT,
                          (char *)data.c_str(),
                          data.length());
            targetKcpPeer->send(ijoon::CONNECTION_RELAY_SERVICE_RESULT,
                          (char *)data.c_str(),
                          data.length());
            
            connection(server, connectionID);
            
            break;
        }
        case ijoon::CONNECTION_REQUEST: // from SP
        {
            ijn_print(DP_DEBUG, "received CONNECTION_REQUEST, body= %s", body);
        
            std::string bodyStr = body;
            auto vec = ijoon::paramParser(body, 2);
            if(vec == nullptr) break;
            std::string targetIP = vec->at(0);
            std::string targetPort = vec->at(1);
            
            auto sourcePrivatePeer = server->callback.getRendezvousClientPeerCallback(peer.getIP(), std::to_string(peer.getPort()));
            auto targetPrivatePeer = server->callback.getRendezvousClientPeerCallback(targetIP, targetPort);
            auto targetPublicPeer = std::shared_ptr<ijoon::Peer>(new ijoon::Peer(targetIP, targetPort));
            if(peer.getKey() == targetPublicPeer->getKey() ||
               sourcePrivatePeer == nullptr ||
               targetPrivatePeer == nullptr) {
                kcpPeer->send(ijoon::CONNECTION_TARGET_INVALID,
                                    (char *)bodyStr.c_str(),
                                    bodyStr.size());
                break;
            }
            
            // create connectionID
            if(server->connectionIDCursor > 1000000000) {
                server->connectionIDCursor = 1;
            }
            int connectionID = server->connectionIDCursor++;
            
            auto connectionInfo = std::shared_ptr<ijoon::ConnectionInfo>(new ijoon::ConnectionInfo());
            connectionInfo->connectionID = connectionID;
            connectionInfo->publicSP = std::shared_ptr<ijoon::Peer>(new ijoon::Peer(peer.getIP(), std::to_string(peer.getPort())));
            connectionInfo->privateSP = sourcePrivatePeer;
            connectionInfo->publicTP = targetPublicPeer;
            connectionInfo->privateTP = targetPrivatePeer;
            server->mutexForConnectionInfoMap.lock();
            server->connectionInfoMap[connectionID] = connectionInfo;
            server->mutexForConnectionInfoMap.unlock();
            
            std::string data = std::to_string(connectionID) + seperator + bodyStr;
            kcpPeer->send(ijoon::CONNECTION_ID_CREATED,
                                (char *)data.c_str(),
                                data.size());
            return;
        }
        case ijoon::CONNECTION_ID_RECEIVED:
        {
            auto vec = ijoon::paramParser(body, 1);
            if(vec == nullptr) break;
            auto connectionIDStr = vec->at(0);
            auto connectionID = static_cast<uint>(atoi(connectionIDStr.c_str()));
            
            auto relayServerPeer = server->callback.getRelayServerPeerCallback();
            if(relayServerPeer == nullptr) {
                ijn_print(DP_INFO, "[CONNECTION_ID_RECEIVED] no relay server");
                std::string data;
                data = std::string("0") +
                       seperator +
                       connectionIDStr +
                       seperator +
                       "0.0.0.0" +
                       seperator +
                       "0";
                kcpPeer->send(ijoon::CONNECTION_RELAY_SERVICE_RESULT,
                              (char *)data.c_str(),
                              data.length());
                connection(server, connectionID);
                break;
            }
            
            server->mutexForConnectionInfoMap.lock();
            if(server->connectionInfoMap.count(connectionID) == 0) {
                ijn_print(DP_ERROR, "[CONNECTION_ID_RECEIVED] relay connection info not exist, %d", connectionID);
                server->mutexForConnectionInfoMap.unlock();
                break;
            }
            auto connectionInfo = server->connectionInfoMap[connectionID];
            server->mutexForConnectionInfoMap.unlock();
            
            auto relayKcpPeer = server->getKcpPeer(relayServerPeer);

            std::string data;
            data += connectionIDStr;
            data += seperator;
            data += peer.getIP();
            data += seperator;
            data += connectionInfo->publicTP->getIP().c_str();
            
            relayKcpPeer->send(ijoon::RELAY_SERVICE_REQUEST,
                                (char *)data.c_str(),
                                data.size());
            return;
        }
        case ijoon::PING_REQUEST:
        {
            kcpPeer->send(ijoon::PING_RESPONSE);
            return;
        }
        case ijoon::UNREGISTRATION_RENDEZVOUS_CLIENT_REQUEST:
        {
            server->mutexForKcpPeerMap.lock();
            server->kcpPeerMap.erase(peer.getKey());
            server->mutexForKcpPeerMap.unlock();
            return;
        }
        case ijoon::UNREGISTRATION_RELAY_SERVER_REQUEST:
        {
            server->mutexForKcpPeerMap.lock();
            server->kcpPeerMap.erase(peer.getKey());
            server->mutexForKcpPeerMap.unlock();
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
        IUINT32 current = iclock();
        int rcvSize = server->socket->recvFrom(peer.get(), rawBuffer, MAX_PACKET_SIZE);
        if(rcvSize > 0) {
            server->mutexForKcpPeerMap.lock();
            auto kcpPeer = server->getKcpPeer(peer);
            server->mutexForKcpPeerMap.unlock();
            
            kcpPeer->mutex.lock();
            ikcpcb *kcp = kcpPeer->getKcp();
            ikcp_input(kcp, rawBuffer, rcvSize);
            ikcp_update(kcp, current);
            kcpPeer->mutex.unlock();
        }
        
        server->mutexForKcpPeerMap.lock();
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

bool ijoon::RendezvousServerLocalCallback::registerRendezvousClientCallback(std::string serial, std::string publicIP, std::string publicPort, std::string privateIP, std::string privatePort, std::string mac, std::string version){
    if(registerRendezvousClient != nullptr) {
        return registerRendezvousClient(serial, publicIP, publicPort, privateIP, privatePort, mac, version);
    }
    
    return false;
}

bool ijoon::RendezvousServerLocalCallback::removeRendezvousClientCallback(std::string ip, std::string port){
    if(removeRendezvousClient != nullptr) {
        return removeRendezvousClient(ip, port);
    }
    
    return false;
}

std::shared_ptr<ijoon::Peer> ijoon::RendezvousServerLocalCallback::getRendezvousClientPeerCallback(std::string ip, std::string port){
    if(getRendezvousClientPeer != nullptr) {
        return getRendezvousClientPeer(ip, port);
    }
    
    return nullptr;
}

bool ijoon::RendezvousServerLocalCallback::registerRelayServerCallback(std::string name, std::string ip, std::string port, std::string version){
    if(registerRelayServer != nullptr) {
        return registerRelayServer(name, ip, port, version);
    }
    
    return false;
}

bool ijoon::RendezvousServerLocalCallback::removeRelayServerCallback(std::string ip, std::string port){
    if(removeRelayServer != nullptr) {
        return removeRelayServer(ip, port);
    }
    
    return false;
}

bool ijoon::RendezvousServerLocalCallback::isExistRelayServerCallback(std::string ip, std::string port){
    if(isExistRelayServer != nullptr) {
        return isExistRelayServer(ip, port);
    }
    
    return false;
}

std::shared_ptr<ijoon::Peer> ijoon::RendezvousServerLocalCallback::getRelayServerPeerCallback() {
    if(getRelayServerPeer != nullptr) {
        return getRelayServerPeer();
    }
    
    return nullptr;
}
