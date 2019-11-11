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
            
            switch(kcpPeer->status) {
                case ijoon::PeerStatus::REGISTERED:
                case ijoon::PeerStatus::COMPATIBLE:
                    // Allowed
                    break;
                case ijoon::PeerStatus::NOT_COMPATIBLE:
                    // Not allowed but only ping update
                    kcpPeer->lastPing = ijoon::ComputableTime::getCurrentTimeSec();
                    ijn_print(DP_ERROR, "Not compatible peer(%s, status=%d)", kcpPeer->getPeer().getKey().c_str(), kcpPeer->status);
                    return;
                case ijoon::PeerStatus::NONE:
                case ijoon::PeerStatus::UNREGISTERED:
                    // Not allowed
                    ijn_print(DP_ERROR, "Not allowed peer(%s, status=%d)", kcpPeer->getPeer().getKey().c_str(), kcpPeer->status);
                    return;
                default:
                    ijn_print(DP_ERROR, "Unknown peer(%s, status=%d)", kcpPeer->getPeer().getKey().c_str(), kcpPeer->status);
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
            if(messageHeader.packetType < ijoon::REGISTRATION_RENDEZVOUS_CLIENT_REQUEST) {
                if(kcpPeer->type == ijoon::PeerType::NONE) {
                   ijn_print(DP_ERROR, "Not registered peer(%s, pakcetType=%d)", kcpPeer->getPeer().getKey().c_str(), messageHeader.packetType);
                    return;
                }
                
                switch(kcpPeer->status) {
                    case ijoon::PeerStatus::REGISTERED:
                    case ijoon::PeerStatus::COMPATIBLE:
                        // Allowed
                        break;
                    case ijoon::PeerStatus::NOT_COMPATIBLE:
                        // Not allowed but only ping update
                        kcpPeer->lastPing = ijoon::ComputableTime::getCurrentTimeSec();
                        ijn_print(DP_ERROR, "Not compatible peer(%s, status=%d)", kcpPeer->getPeer().getKey().c_str(), kcpPeer->status);
                        return;
                    case ijoon::PeerStatus::NONE:
                    case ijoon::PeerStatus::UNREGISTERED:
                        // Not allowed
                        ijn_print(DP_ERROR, "Not allowed peer(%s, status=%d)", kcpPeer->getPeer().getKey().c_str(), kcpPeer->status);
                        return;
                    default:
                        ijn_print(DP_ERROR, "Unknown peer(%s, status=%d)", kcpPeer->getPeer().getKey().c_str(), kcpPeer->status);
                        return;
                }
            }
            
            kcpPeer->lastPing = ijoon::ComputableTime::getCurrentTimeSec();
            
            auto callbackWrapper = registry->GetCallbackWrapper(messageHeader.messageType, messageHeader.packetType);
            if(callbackWrapper != nullptr) {
                callbackWrapper->callback(kcpPeer, body, messageHeader.dataSize);
            }
            else {
                printf("Not registered raw message received. mtype=%d, body=%s", messageHeader.messageType, body);
            }
            
            return;
        }
        default:
        {
            ijn_print(DP_ERROR, "Unknown message type received, type=%d", messageHeader.messageType);
            return;
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
