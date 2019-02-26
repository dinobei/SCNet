#include "rendezvous_client.h"
#include "rendezvous_message.h"
#include "message_header.h"
#include "session.h"
#include <ifaddrs.h>
#include <cstring>
#include "registry.h"
#include <assert.h>

extern char seperator;

int udp_output(const char *buf, int len, ikcpcb *kcp, void *user) {
//    ijn_print(DP_DEBUG, "udp_output len : %d.", len);
//    ijn_print(DP_DEBUG, "udp_output buf : %s.", buf);
    
    ijoon::KcpPeer *kcpPeer = (ijoon::KcpPeer *)user;
    ijoon::Peer peer = kcpPeer->getPeer();
    kcpPeer->getClientSocket()->sendTo(&peer, const_cast<char *>(buf), len);
    return 0;
}

// reference: https://stackoverflow.com/a/265978
std::string getIPAddress(const char *ifname) {
    assert(ifname!=nullptr);
    
    std::string ipAddress;
    struct ifaddrs * ifAddrStruct=NULL;
    struct ifaddrs * ifa=NULL;
    
    getifaddrs(&ifAddrStruct);
    
    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }
        if (ifa->ifa_addr->sa_family == AF_INET) { // check it is IP4
            // is a valid IP4 Address
             if(strcmp(ifa->ifa_name, ifname)==0) {
                 void * tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
                 char addressBuffer[INET_ADDRSTRLEN];
                 inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
                 ipAddress = addressBuffer;
                 break;
             }
        } else if (ifa->ifa_addr->sa_family == AF_INET6) { // check it is IP6
            // is a valid IP6 Address
            if(strcmp(ifa->ifa_name, ifname)==0) {
                void * tmpAddrPtr =&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
                char addressBuffer[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
                ipAddress = addressBuffer;
                break;
            }
            
        }
    }
    if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);
    
    assert(!ipAddress.empty());
    return ipAddress;
}

std::shared_ptr<ijoon::RendezvousSession> getRendezvousSessionSafety(ijoon::RendezvousClient *client, ijoon::MessageHeader messageHeader) {
    if(client->rendezvousSessionMap.count(messageHeader.connectionID) == 0) {
        std::shared_ptr<ijoon::RendezvousSession> rendezvousSession = std::shared_ptr<ijoon::RendezvousSession>(new ijoon::RendezvousSession(client->socket, messageHeader.connectionID));
        client->rendezvousSessionMap[messageHeader.connectionID] = rendezvousSession;
        return rendezvousSession;
    }
    
    return client->rendezvousSessionMap[messageHeader.connectionID];
}

void ijoon::RendezvousClient::start() {
    ijn_print(DP_INFO, "Rendezvous client start...");
    registerThread = new ijoon::Thread(registerThreadFunc, "register thread");
    registerThread->start(this);
    recvThread = new ijoon::Thread(recvThreadFunc, "recv thread");
    recvThread->start(this);
    rawRecvThread = new ijoon::Thread(rawRecvThreadFunc, "raw recv thread");
    rawRecvThread->start(this);
}

std::shared_ptr<ijoon::KcpPeer> getKcpPeer(ijoon::RendezvousClient *client, std::shared_ptr<ijoon::Peer> peer) {
    std::shared_ptr<ijoon::KcpPeer> kcpPeer;
    if(client->kcpPeerMap.count(peer->getKey()) == 0) {
        kcpPeer = std::shared_ptr<ijoon::KcpPeer>(new ijoon::KcpPeer(client->socket, peer->getIP(), std::to_string(peer->getPort()), udp_output));
        client->kcpPeerMap[peer->getKey()] = kcpPeer;
    }
    else {
        kcpPeer = client->kcpPeerMap.at(peer->getKey());
    }
    
    return kcpPeer;
}

ijoon::THREAD_RET THREAD_API ijoon::registerThreadFunc(void *arg) {
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::RendezvousClient *client = (ijoon::RendezvousClient *)thread->getParam();

    std::string localIP = getIPAddress(client->ifname.c_str());
    printf("localIP: %s\n", localIP.c_str());
    int localPort = 0;
    struct sockaddr_in sin;
    socklen_t addrlen = sizeof(sin);
    if(getsockname(client->socket->getSocketIdentifier(), (struct sockaddr *)&sin, &addrlen) == 0 &&
       sin.sin_family == AF_INET &&
       addrlen == sizeof(sin))
    {
        localPort = ntohs(sin.sin_port);
        printf("localPort: %d\n", localPort);
    }
    else
        printf("localPort: Error get local port\n"); // handle error
    
    std::string data = "";
    if(!client->serial.empty()) {
        data = client->serial + seperator;
    }
    data += localIP + seperator + std::to_string(localPort);
    
    auto serverKcpPeer = getKcpPeer(client, client->rendezvousServerPeer);
    
    const int timeoutSec = 180;
    const int pingIntervalSec = 30;
    const int checkMinIntervalSec = 25;
    const int loopIntervalMs = 5 * 1000;
    
    serverKcpPeer->lastPing = 0;
    while(!thread->isInterrupted()) {
        time_t currentTime = ijoon::ComputableTime::getCurrentTimeSec();
        
        // send and ping to rendezvous server
        if(serverKcpPeer->lastPing + pingIntervalSec < currentTime) {
            ijoon::send(serverKcpPeer->getKcp(), 0, ijoon::REGISTRATION_RENDEZVOUS_CLIENT_REQUEST, (char *)data.c_str(), data.length());
        }
        
        thread->sleep(loopIntervalMs);
        
        // send ping and check to relay server, connected peer
        auto iter = client->rendezvousSessionMap.begin();
        auto end = client->rendezvousSessionMap.end();
        while(iter != end) {
            if(iter->second == nullptr) continue;
            auto rendezvousSession = iter->second;
            
            if(rendezvousSession->getRelayKcpPeer() != nullptr) {
                auto relayKcpPeer = rendezvousSession->getRelayKcpPeer();
                if(relayKcpPeer->lastPing + timeoutSec < currentTime) {
                    ijn_print(DP_ERROR, "relay peer removed, %lu", relayKcpPeer->lastPing);
                    rendezvousSession->clearRelayKcpPeer();
                }
                else if(relayKcpPeer->lastPing < currentTime - checkMinIntervalSec) {
                    ijoon::send(relayKcpPeer->getKcp(), 0, PING_RELAY_PEER, nullptr, 0);
                }
            }
            
            if(rendezvousSession->getPublicKcpPeer() != nullptr) {
                auto publicKcpPeer = rendezvousSession->getPublicKcpPeer();
                if(publicKcpPeer->lastPing + timeoutSec < currentTime) {
                    ijn_print(DP_ERROR, "public peer removed, %lu", publicKcpPeer->lastPing);
                    rendezvousSession->clearPublicKcpPeer();
                }
                else if(publicKcpPeer->lastPing < currentTime - checkMinIntervalSec) {
                    ijoon::send(publicKcpPeer->getKcp(), 0, PING_CONNECTED_PEER, nullptr, 0);
                }
            }
            
            if(rendezvousSession->getPrivateKcpPeer() != nullptr) {
                auto privateKcpPeer = rendezvousSession->getPrivateKcpPeer();
                if(privateKcpPeer->lastPing + timeoutSec < currentTime) {
                    ijn_print(DP_ERROR, "private peer removed, %lu", privateKcpPeer->lastPing);
                    rendezvousSession->clearPrivateKcpPeer();
                }
                else if(privateKcpPeer->lastPing < currentTime - checkMinIntervalSec) {
                    ijoon::send(privateKcpPeer->getKcp(), 0, PING_CONNECTED_PEER, nullptr, 0);
                }
            }
            
            if(!rendezvousSession->isConnected()) {
                int connectionID = rendezvousSession->getConnectionID();
                iter = client->rendezvousSessionMap.erase(iter);
                
                // callback to user (disconnected)
                ijn_print(DP_INFO, "Disconnected, connectionID=%d", connectionID);
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

void onCallback(ijoon::RendezvousClient *client, std::shared_ptr<ijoon::KcpPeer> kcpPeer, char *packet, int recvSize) {
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
    switch(messageHeader.messageType) {
        case ijoon::PROTOBUF:
        {
            google::protobuf::Message *message = BaseMessageRegistry->Create(messageHeader.packetType);
            if(message == nullptr) {
                ijn_print(DP_INFO, "Unknown protobuf packet_type(%d) reveiced", messageHeader.packetType);
                return;
            }
            
            auto rendezvousSession = getRendezvousSessionSafety(client, messageHeader);
            message->ParseFromArray(body, messageHeader.dataSize);
            auto callbackWrapper = BaseMessageRegistry->GetCallbackWrapper(messageHeader.messageType, messageHeader.packetType);
            if(callbackWrapper != nullptr) {
                callbackWrapper->callback(rendezvousSession.get(), message);
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
            
            auto rendezvousSession = getRendezvousSessionSafety(client, messageHeader);
            auto callbackWrapper = BaseMessageRegistry->GetCallbackWrapper(messageHeader.messageType, messageHeader.packetType);
            if(callbackWrapper != nullptr) {
                callbackWrapper->callback(rendezvousSession.get(), body, messageHeader.dataSize);
            }
            else {
                printf("unregistered raw message received. body=%s", body);
            }
            
            return;
        }
        default:
            break;
    }
    
    if(messageHeader.messageType != ijoon::MESSAGE_TYPE::RAWBYTE) {
        ijn_print(DP_ERROR, "Unknown packet_type(%d) received", messageHeader.messageType);
        return;
    }
    
    switch (messageHeader.packetType) {
        case ijoon::REGISTRATION_RENDEZVOUS_CLIENT_SUCCESS:
        {
            ijn_print(DP_DEBUG, "received REGISTRATION_RENDEZVOUS_CLIENT_SUCCESS");
            
            auto vec = ijoon::paramParser(body, 2);
            if(vec == nullptr) break;
            
            ijn_print(DP_INFO, "[REGISTRATION_RENDEZVOUS_CLIENT_SUCCESS] MyPublicAddress=%s:%s", vec->at(0).c_str(), vec->at(1).c_str());
            return;
        }
        case ijoon::CONNECTION_FAILED:
        {
            ijn_print(DP_DEBUG, "received CONNECTION_FAILED");
            if(client->onConnectFailedCallback != nullptr) {
                auto rendezvousSession = getRendezvousSessionSafety(client, messageHeader);
                client->onConnectFailedCallback(rendezvousSession);
            }
            return;
        }
        case ijoon::DIRECT_CONNECTION_AVAILABLE: // SP only
        {
            ijn_print(DP_DEBUG, "received DIRECT_CONNECTION_AVAILABLE");
            
            auto vec = ijoon::paramParser(body, 2);
            if(vec == nullptr) break;
            
            if(client->onConnectingCallback != nullptr) {
                auto rendezvousSession = getRendezvousSessionSafety(client, messageHeader);
                if(!rendezvousSession->isConnected()) {
                    client->onConnectingCallback(rendezvousSession);
                }
            }
            
            auto targetPeer = std::shared_ptr<ijoon::Peer>(new ijoon::Peer(vec->at(0), vec->at(1)));
            auto targetKcpPeer = getKcpPeer(client, targetPeer);

            client->send(targetKcpPeer, messageHeader.connectionID, ijoon::DIRECT_CONNECTION_REQUEST, nullptr, 0);
            return;
        }
        case ijoon::DIRECT_CONNECTION_REQUEST: // TP only
        {
            ijn_print(DP_DEBUG, "received DIRECT_CONNECTION_REQUEST");
            
            auto rendezvousSession = getRendezvousSessionSafety(client, messageHeader);
            bool isConnected = rendezvousSession->isConnected();
            
            rendezvousSession->setPublicKcpPeer(kcpPeer);
            printf("directly connected from %s:%d\n", peer.getIP().c_str(), peer.getPort());
            
            if(!isConnected) {
                if(client->onConnectedCallback != nullptr) {
                    client->onConnectedCallback(rendezvousSession);
                }
            }
            
            client->send(kcpPeer, messageHeader.connectionID, ijoon::DIRECT_CONNECTION_RESPONSE, nullptr, 0);
            return;
        }
        case ijoon::DIRECT_CONNECTION_RESPONSE: // SP only
        {
            ijn_print(DP_DEBUG, "received DIRECT_CONNECTION_RESPONSE");
            
            auto rendezvousSession = getRendezvousSessionSafety(client, messageHeader);
            bool isConnected = rendezvousSession->isConnected();
            rendezvousSession->setPublicKcpPeer(kcpPeer);
            printf("directly connected from %s:%d\n", peer.getIP().c_str(), peer.getPort());
            
            if(!isConnected) {
                if(client->onConnectedCallback != nullptr) {
                    client->onConnectedCallback(rendezvousSession);
                }
            }
            return;
        }
        case ijoon::REVERSE_CONNECTION_READY:
        {
            auto rendezvousSession = getRendezvousSessionSafety(client, messageHeader);
            if(!rendezvousSession->isConnected()) {
                if(client->onConnectingCallback != nullptr) {
                    client->onConnectingCallback(rendezvousSession);
                }
            }
            
            return;
        }
        case ijoon::REVERSE_CONNECTION: // TP only
        {
            ijn_print(DP_DEBUG, "received REVERSE_CONNECTION");
            
            auto vec = ijoon::paramParser(body, 2);
            if(vec == nullptr) break;
            
            auto sourcePeer = std::shared_ptr<ijoon::Peer>(new ijoon::Peer(vec->at(0), vec->at(1)));
            auto sourceKcpPeer = getKcpPeer(client, sourcePeer);
            
            client->send(sourceKcpPeer, messageHeader.connectionID, ijoon::REVERSE_CONNECTION_REQUEST, nullptr, 0);
            return;
        }
        case ijoon::REVERSE_CONNECTION_REQUEST: // SP only
        {
            ijn_print(DP_DEBUG, "received REVERSE_CONNECTION_REQUEST");
            
            client->send(kcpPeer, messageHeader.connectionID, ijoon::REVERSE_CONNECTION_RESPONSE, nullptr, 0);
            
            auto rendezvousSession = getRendezvousSessionSafety(client, messageHeader);
            bool isConnected = rendezvousSession->isConnected();
            rendezvousSession->setPublicKcpPeer(kcpPeer);
            printf("reversely connected from %s:%d\n", peer.getIP().c_str(), peer.getPort());
            
            if(!isConnected) {
                if(client->onConnectedCallback != nullptr) {
                    client->onConnectedCallback(rendezvousSession);
                }
            }
            
            return;
        }
        case ijoon::REVERSE_CONNECTION_RESPONSE: // TP only
        {
            ijn_print(DP_DEBUG, "received REVERSE_CONNECTION_RESPONSE");
            
            auto rendezvousSession = getRendezvousSessionSafety(client, messageHeader);
            bool isConnected = rendezvousSession->isConnected();
            rendezvousSession->setPublicKcpPeer(kcpPeer);
            printf("reversely connected from %s:%d\n", peer.getIP().c_str(), peer.getPort());
            
            if(!isConnected) {
                if(client->onConnectedCallback != nullptr) {
                    client->onConnectedCallback(rendezvousSession);
                }
            }
            
            return;
        }
            
        case ijoon::UDP_HOLE_PUNCHING_AVAILABLE:
        {
            ijn_print(DP_DEBUG, "received UDP_HOLE_PUNCHING_AVAILABLE");
            
            auto vec = ijoon::paramParser(body, 4);
            if(vec == nullptr) break;
            
            auto rendezvousSession = getRendezvousSessionSafety(client, messageHeader);
            if(!rendezvousSession->isConnected()) {
                if(client->onConnectingCallback != nullptr) {
                    client->onConnectingCallback(rendezvousSession);
                }
            }
            
            std::string data;
            
            auto publicPeer = std::shared_ptr<ijoon::Peer>(new ijoon::Peer(vec->at(0), vec->at(1)));
            auto publicKcpPeer = getKcpPeer(client, publicPeer);
            
            data = "1";
            client->send(publicKcpPeer, messageHeader.connectionID, ijoon::UDP_HOLE_PUNCHING_REQUEST, (char *)data.c_str(), data.length());
            
            auto privatePeer = std::shared_ptr<ijoon::Peer>(new ijoon::Peer(vec->at(2), vec->at(3)));
            auto privateKcpPeer = getKcpPeer(client, privatePeer);
            
            data = "0";
            client->send(privateKcpPeer, messageHeader.connectionID, ijoon::UDP_HOLE_PUNCHING_REQUEST, (char *)data.c_str(), data.length());
            
            return;
        }
        case ijoon::UDP_HOLE_PUNCHING_REQUEST:
        {
            ijn_print(DP_DEBUG, "received UDP_HOLE_PUNCHING_REQUEST");
            
            auto vec = ijoon::paramParser(body, 1);
            if(vec == nullptr) break;
            
            std::string data = vec->at(0);
            client->send(kcpPeer, messageHeader.connectionID, ijoon::UDP_HOLE_PUNCHING_RESPONSE, (char *)data.c_str(), data.length());
            return;
        }
        case ijoon::UDP_HOLE_PUNCHING_RESPONSE:
        {
            ijn_print(DP_DEBUG, "received UDP_HOLE_PUNCHING_RESPONSE");
            
            // Connected by hole punching or local
            
            auto vec = ijoon::paramParser(body, 1);
            if(vec == nullptr) break;
            
            const bool isPublic = atoi(vec->at(0).c_str()) ? true : false;
            
            auto rendezvousSession = getRendezvousSessionSafety(client, messageHeader);
            bool isConnected = rendezvousSession->isConnected();
            if(isPublic) {
                // public connection (hole punching)
                rendezvousSession->setPublicKcpPeer(kcpPeer);
                printf("udp hole punching success from %s:%d (connectionID=%d)\n", peer.getIP().c_str(), peer.getPort(), messageHeader.connectionID);
            }
            else {
                // private connection (equal net)
                rendezvousSession->setPrivateKcpPeer(kcpPeer);
                printf("connected under equal nat from %s:%d (connectionID=%d)\n", peer.getIP().c_str(), peer.getPort(), messageHeader.connectionID);
            }
            
            if(!isConnected) {
                if(client->onConnectedCallback != nullptr) {
                    client->onConnectedCallback(rendezvousSession);
                }
            }
            
            return;
        }
        case ijoon::RELAY_SERVER_INFORMATION:
        {
            ijn_print(DP_DEBUG, "received RELAY_SERVER_INFORMATION, connectionID=%d", messageHeader.connectionID);
            
            auto vec = ijoon::paramParser(body, 3);
            if(vec == nullptr) break;
            
            auto rendezvousSession = getRendezvousSessionSafety(client, messageHeader);
            if(client->onConnectingCallback != nullptr) {
                client->onConnectingCallback(rendezvousSession);
            }
            
            ijn_print(DP_INFO, "[RELAY_SERVER_INFORMATION] RelayServerAddress=%s:%s", vec->at(0).c_str(), vec->at(1).c_str());
            
            auto relayPeer = std::shared_ptr<ijoon::Peer>(new ijoon::Peer(vec->at(0), vec->at(1)));
            auto relayKcpPeer = getKcpPeer(client, relayPeer);
            
            client->send(relayKcpPeer, messageHeader.connectionID, ijoon::REGISTRATION_RELAY_PEER_REQUEST, (char *)vec->at(2).c_str(), vec->at(2).length());
            
            return;
        }
        case ijoon::CONNECTION_RELAY_SERVICE_SUCCESS:
        {
            ijn_print(DP_DEBUG, "received CONNECTION_RELAY_SERVICE_SUCCESS");
            
            auto vec = ijoon::paramParser(body, 2);
            if(vec == nullptr) break;
            
            
            auto relayPeer = std::shared_ptr<ijoon::Peer>(new ijoon::Peer(vec->at(0), vec->at(1)));
            auto relayKcpPeer = getKcpPeer(client, relayPeer);
            
            auto rendezvousSession = getRendezvousSessionSafety(client, messageHeader);
            rendezvousSession->setRelayKcpPeer(relayKcpPeer);
            printf("connected by a relay from %s:%s\n", vec->at(0).c_str(), vec->at(1).c_str());
            
            if(client->onConnectedCallback != nullptr) {
                client->onConnectedCallback(rendezvousSession);
            }
            return;
        }
        case ijoon::CONNECTION_RELAY_SERVICE_FAILED:
        {
            ijn_print(DP_DEBUG, "received CONNECTION_RELAY_SERVICE_FAILED");
            return;
        }
        case ijoon::REGISTRATION_RELAY_PEER_SUCCESS:
        {
            ijn_print(DP_DEBUG, "received REGISTRATION_RELAY_PEER_SUCCESS");
            return;
        }
        case ijoon::REGISTRATION_RELAY_PEER_FAILED:
        {
            ijn_print(DP_DEBUG, "received REGISTRATION_RELAY_PEER_FAILED");
            return;
        }
        case ijoon::PING_CONNECTED_PEER:
        {
            ijn_print(DP_DEBUG, "received PING_CONNECTED_PEER");
            ijoon::send(kcpPeer->getKcp(), messageHeader.connectionID, ijoon::PING_CONNECTED_PEER_RESPONSE, nullptr, 0);
            return;
        }
        case ijoon::PING_CONNECTED_PEER_RESPONSE:
        {
            ijn_print(DP_DEBUG, "received PING_CONNECTED_PEER_RESPONSE");
            return;
        }
        case ijoon::PING_RELAY_PEER:
        {
            ijn_print(DP_DEBUG, "received PING_RELAY_PEER");
            return;
        }
        default:
        {
            ijn_print(DP_ERROR, "Undefined message received");
            break;
        }
    }
}

void updateKcpObject(ijoon::RendezvousClient *client, std::shared_ptr<ijoon::KcpPeer> kcpPeer) {
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
            onCallback(client, kcpPeer, buffer, rcvSize);
        }
    }
    
    delete []buffer;
}

ijoon::THREAD_RET ijoon::recvThreadFunc(void *param) {
    auto thread = static_cast<ijoon::Thread *>(param);
    ijoon::RendezvousClient *client = (ijoon::RendezvousClient *)thread->getParam();
    
    while(!thread->isInterrupted()) {
        
        auto iter = client->kcpPeerMap.begin();
        for(; iter != client->kcpPeerMap.end() ; ++iter) {
            updateKcpObject(client, iter->second);
        }
        
        ijn_msleep(10);
    }
    
    return THREAD_EXIT;
}

ijoon::THREAD_RET ijoon::rawRecvThreadFunc(void *param) {
    auto thread = static_cast<ijoon::Thread *>(param);
    ijoon::RendezvousClient *client = (ijoon::RendezvousClient *)thread->getParam();
    
    auto peer = std::shared_ptr<ijoon::Peer>(new Peer());
    
    char *buffer = new char[MAX_PACKET_SIZE];
    
    while(!thread->isInterrupted()) {
        int rcvSize = client->socket->recvFrom(peer.get(), buffer, MAX_PACKET_SIZE);
        if(rcvSize < 0) continue;
        
        auto kcpPeer = getKcpPeer(client, peer);
        
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

void ijoon::RendezvousClient::send(std::shared_ptr<ijoon::KcpPeer> kcpPeer, int connectionID, int packetType, char *message, unsigned int length) {
    int size = MAGIC_PACKET_LENGTH + MAX_PACKET_HEADER_SIZE + length;
    char *buffer = new char[size];
    google::protobuf::io::ArrayOutputStream aos(buffer, size);
    google::protobuf::io::CodedOutputStream coded_output(&aos);
    coded_output.WriteRaw(MAGIC_PACKET, MAGIC_PACKET_LENGTH);
    coded_output.WriteVarint32(length); // data size
    coded_output.WriteVarint32(packetType); // packet type
    coded_output.WriteVarint32(ijoon::MESSAGE_TYPE::RAWBYTE); // message type
    coded_output.WriteVarint32(0); // crypt type
    coded_output.WriteVarint32(connectionID); // connection id

    if(length != 0)
        coded_output.WriteRaw(message, length);

    ikcp_send(kcpPeer->getKcp(), buffer, coded_output.ByteCount());
    
    delete []buffer;
}

void ijoon::RendezvousClient::send(std::shared_ptr<ijoon::KcpPeer> kcpPeer, int connectionID, std::shared_ptr<google::protobuf::Message> message) {
    int typeInt = BaseMessageRegistry->GetType(message->GetTypeName());
    if(typeInt < 0) {
        ijn_print(DP_ERROR, "You must regist protobuf-message before send(), [%s]", message->GetTypeName().c_str());
        exit(-1);
    }
    
    int size = MAGIC_PACKET_LENGTH + MAX_PACKET_HEADER_SIZE + message->ByteSize();
    char *buffer = new char[size];
    google::protobuf::io::ArrayOutputStream aos(buffer,size);
    google::protobuf::io::CodedOutputStream coded_output(&aos);
    coded_output.WriteRaw(ijoon::MAGIC_PACKET, MAGIC_PACKET_LENGTH);
    coded_output.WriteVarint32(message->ByteSize()); // data size
    coded_output.WriteVarint32(typeInt); // packet type
    coded_output.WriteVarint32(ijoon::MESSAGE_TYPE::PROTOBUF); // message type
    coded_output.WriteVarint32(0); // crypt type
    coded_output.WriteVarint32(connectionID); // connection id
    
    message->SerializeToCodedStream(&coded_output);
    
    ikcp_send(kcpPeer->getKcp(), buffer, coded_output.ByteCount());
    
    delete []buffer;
}

void ijoon::RendezvousClient::send(std::shared_ptr<ijoon::Peer> peer, int connectionID, int packetType, char *message, unsigned int length) {
    auto kcpPeer = getKcpPeer(this, peer);
    send(kcpPeer, connectionID, packetType, message, length);
}

void ijoon::RendezvousClient::send(std::shared_ptr<ijoon::Peer> peer, int connectionID, std::shared_ptr<google::protobuf::Message> message) {
    auto kcpPeer = getKcpPeer(this, peer);
    send(kcpPeer, connectionID, message);
}
