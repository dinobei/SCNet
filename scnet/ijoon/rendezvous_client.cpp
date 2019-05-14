#include "rendezvous_client.h"
#include "rendezvous_message.h"
#include "message_header.h"
#include "session.h"

#ifndef __IJN_WINDOWS__
#include <ifaddrs.h>
#endif

#include <cstring>
#include "registry.h"
#include <assert.h>
#include "utils.h"

extern char seperator;

ijoon::THREAD_RET THREAD_API registerThreadFunc(void *arg);
ijoon::THREAD_RET THREAD_API rawRecvThreadFunc(void *arg);
ijoon::THREAD_RET THREAD_API recvThreadFunc(void *param);

int udp_output(const char *buf, int len, ikcpcb *kcp, void *user) {
//    ijn_print(DP_DEBUG, "udp_output len : %d.", len);
//    ijn_print(DP_DEBUG, "udp_output buf : %s.", buf);
    
    ijoon::KcpPeer *kcpPeer = (ijoon::KcpPeer *)user;
    ijoon::Peer peer = kcpPeer->getPeer();
    kcpPeer->getClientSocket()->sendTo(&peer, const_cast<char *>(buf), len);
    return 0;
}

#ifdef __IJN_WINDOWS__
// reference: https://stackoverflow.com/a/3120382
std::string GetPrimaryIp()
{
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	assert(sock != -1);

	const char* kGoogleDnsIp = "8.8.8.8";
	uint16_t kDnsPort = 53;
	struct sockaddr_in serv;
	memset(&serv, 0, sizeof(serv));
	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = inet_addr(kGoogleDnsIp);
	serv.sin_port = htons(kDnsPort);

	int err = connect(sock, (const sockaddr*)&serv, sizeof(serv));
	assert(err != -1);

	sockaddr_in name;
	socklen_t namelen = sizeof(name);
	err = getsockname(sock, (sockaddr*)&name, &namelen);
	assert(err != -1);

	char buffer[255] = { 0, };
	const char* p = inet_ntop(AF_INET, &name.sin_addr, buffer, sizeof(buffer));
	assert(p);

	closesocket(sock);
	return std::string(buffer);
}
#endif

// reference: https://stackoverflow.com/a/265978
std::string getIPAddress(const char *ifname) {
#ifdef __IJN_WINDOWS__
	std::string ipAddress = GetPrimaryIp();
#else
	assert(ifname != nullptr);

	std::string ipAddress = "";
	struct ifaddrs * ifAddrStruct = NULL;
	struct ifaddrs * ifa = NULL;

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
        }
    }
    if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);
#endif
    return ipAddress;
}

int getPort(ijoon::RendezvousClient *client) {
    int localPort = -1;
    struct sockaddr_in sin;
    socklen_t addrlen = sizeof(sin);
    if(getsockname(client->socket->getSocketIdentifier(), (struct sockaddr *)&sin, &addrlen) == 0 &&
       sin.sin_family == AF_INET &&
       addrlen == sizeof(sin))
    {
        localPort = ntohs(sin.sin_port);
    }
    
    return localPort;
    
}

std::shared_ptr<ijoon::RendezvousSession> getRendezvousSessionSafety(ijoon::RendezvousClient *client, ijoon::MessageHeader messageHeader) {
    if(client->rendezvousSessionMap.count(messageHeader.connectionID) == 0) {
        auto rendezvousSession = std::shared_ptr<ijoon::RendezvousSession>(new ijoon::RendezvousSession(client->socket, messageHeader.connectionID));
        if(messageHeader.connectionID != 0) {
            client->rendezvousSessionMap[messageHeader.connectionID] = rendezvousSession;
        }
        return rendezvousSession;
    }
    
    return client->rendezvousSessionMap[messageHeader.connectionID];
}

void ijoon::RendezvousClient::start() {
    ijn_print(DP_INFO, "Rendezvous client start...");
    registerThread = new ijoon::Thread(registerThreadFunc, "register thread");
    registerThread->start(this);
    rawRecvThread = new ijoon::Thread(rawRecvThreadFunc, "raw recv thread");
    rawRecvThread->start(this);
    recvThread = new ijoon::Thread(recvThreadFunc, "recv thread");
    recvThread->start(this);
}

void ijoon::RendezvousClient::stop() {
    ijn_print(DP_INFO, "Rendezvous client stop...");
    
    registerThread->interrupt();
    rawRecvThread->interrupt();
    recvThread->interrupt();
    
    registerThread->join();
    rawRecvThread->join();
    recvThread->join();
    
    kcpPeerMap.clear();
    rendezvousSessionMap.clear();
}

std::shared_ptr<ijoon::KcpPeer> ijoon::RendezvousClient::getKcpPeer(ijoon::Peer peer) {
    std::shared_ptr<ijoon::KcpPeer> kcpPeer;
    if(this->kcpPeerMap.count(peer.getKey()) == 0) {
        kcpPeer = std::shared_ptr<ijoon::KcpPeer>(new ijoon::KcpPeer(this->socket, peer.getIP(), std::to_string(peer.getPort()), udp_output));
        this->kcpPeerMap[peer.getKey()] = kcpPeer;
    }
    else {
        kcpPeer = this->kcpPeerMap.at(peer.getKey());
    }
    
    return kcpPeer;
}

ijoon::THREAD_RET THREAD_API registerThreadFunc(void *arg) {
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::RendezvousClient *client = (ijoon::RendezvousClient *)thread->getParam();

    std::string localIP = getIPAddress(client->ifname.c_str());
    int localPort = getPort(client);
    printf("localAddress: %s:%d\n", localIP.c_str(), localPort);

    std::string data = localIP + seperator + std::to_string(localPort) + seperator + client->serial;
    
    const int timeoutSec = 60;
    const int pingIntervalSec = 30;
    const int loopIntervalMs = 5 * 1000;
    bool connFlag = false;
    
    auto serverPeer = ijoon::Peer(client->serverIP, client->serverPort);
    while(!thread->isInterrupted()) {
        time_t currentTime = ijoon::ComputableTime::getCurrentTimeSec();
        
        auto serverKcpPeer = client->getKcpPeer(serverPeer);
        
        if(client->serverPing == 0) {
            // send registration packet
            if(connFlag) {
                client->onServerConnectFailedCallback();
            }
            
            connFlag = true;
            client->onServerConnectingCallback();
            ijoon::send(serverKcpPeer,
                        0,
                        ijoon::REGISTRATION_RENDEZVOUS_CLIENT_REQUEST,
                        (char *)data.c_str(),
                        data.length());
        }
        else if(client->serverPing + timeoutSec < currentTime) {
            // disconnected
            connFlag = false;
            client->serverPing = 0;
            client->onServerDisconnectedCallback();
        }

        client->mutexForKcpPeerMap.lock();
        auto iter = client->kcpPeerMap.begin();
        auto end = client->kcpPeerMap.end();
        while(iter != end) {
            auto kcpPeer = iter->second;
            if(kcpPeer->lastPing + timeoutSec < currentTime) {
                ijn_print(DP_INFO, "removed kcpPeer: %s", kcpPeer->getPeer().getKey().c_str());
                iter = client->kcpPeerMap.erase(iter);
                continue;
            }
            else if(kcpPeer->lastPing + pingIntervalSec < currentTime) {
                ijoon::send(iter->second, 0, ijoon::PING_REQUEST, nullptr, 0);
            }

            ++iter;
        }
        client->mutexForKcpPeerMap.unlock();
        
        // send ping and check to relay server, connected peer
        client->mutexForRendezvousSessionMap.lock();
        auto iterSessionMap = client->rendezvousSessionMap.begin();
        auto endSessionMap = client->rendezvousSessionMap.end();
        while(iterSessionMap != endSessionMap) {
            if(iterSessionMap->second == nullptr) continue;
            auto rendezvousSession = iterSessionMap->second;
            
            if(rendezvousSession->getRelayKcpPeer() != nullptr) {
                auto relayKcpPeer = rendezvousSession->getRelayKcpPeer();
                if(relayKcpPeer->lastPing + timeoutSec < currentTime) {
                    ijn_print(DP_ERROR, "relay peer removed, %lu", relayKcpPeer->lastPing);
                    rendezvousSession->clearRelayKcpPeer();
                    relayKcpPeer->getPeer().getKey();
                }
            }
            
            if(rendezvousSession->getPublicKcpPeer() != nullptr) {
                auto publicKcpPeer = rendezvousSession->getPublicKcpPeer();
                if(publicKcpPeer->lastPing + timeoutSec < currentTime) {
                    ijn_print(DP_ERROR, "public peer removed, %lu", publicKcpPeer->lastPing);
                    rendezvousSession->clearPublicKcpPeer();
                }
            }
            
            if(rendezvousSession->getPrivateKcpPeer() != nullptr) {
                auto privateKcpPeer = rendezvousSession->getPrivateKcpPeer();
                if(privateKcpPeer->lastPing + timeoutSec < currentTime) {
                    ijn_print(DP_ERROR, "private peer removed, %lu", privateKcpPeer->lastPing);
                    rendezvousSession->clearPrivateKcpPeer();
                }
            }
            
            if(!rendezvousSession->isConnected()) {
                int connectionID = rendezvousSession->getConnectionID();
                iterSessionMap = client->rendezvousSessionMap.erase(iterSessionMap);
                
                // callback to user (disconnected)
                ijn_print(DP_INFO, "Disconnected, connectionID=%d", connectionID);
            }
            else {
                ++iterSessionMap;
            }
        }
        client->mutexForRendezvousSessionMap.unlock();
        
        thread->sleep(loopIntervalMs);
    }
    
    ijn_print(DP_DEBUG, "registerThread finished");
    return THREAD_EXIT;
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
    auto registry = Registry<int, google::protobuf::Message *>().Get();
    switch(messageHeader.messageType) {
        case ijoon::PROTOBUF:
        {
            google::protobuf::Message *message = registry->Create(messageHeader.packetType);
            if(message == nullptr) {
                ijn_print(DP_INFO, "Unknown protobuf packet_type(%d) reveiced", messageHeader.packetType);
                return;
            }
            
            auto rendezvousSession = getRendezvousSessionSafety(client, messageHeader);
            message->ParseFromArray(body, messageHeader.dataSize);
            auto callbackWrapper = registry->GetCallbackWrapper(messageHeader.messageType, messageHeader.packetType);
            if(callbackWrapper != nullptr) {
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
            
            auto rendezvousSession = getRendezvousSessionSafety(client, messageHeader);
            auto callbackWrapper = registry->GetCallbackWrapper(messageHeader.messageType, messageHeader.packetType);
            if(callbackWrapper != nullptr) {
                callbackWrapper->callback(rendezvousSession, body, messageHeader.dataSize);
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
        ijn_print(DP_ERROR, "Unknown message_type(%d) received", messageHeader.messageType);
        return;
    }
    
    switch (messageHeader.packetType) {
        case ijoon::REGISTRATION_RENDEZVOUS_CLIENT_SUCCESS:
        {
            auto vec = ijoon::paramParser(body, 2);
            if(vec == nullptr) break;
            
//            if(client->lastRegistrationTime == 0 && client->onServerConnected != nullptr) {
//                client->onServerConnected(vec->at(0), vec->at(1));
//            }
//
//            client->lastRegistrationTime = ijoon::ComputableTime::getCurrentTimeSec();
            ijn_print(DP_DEBUG, "received REGISTRATION_RENDEZVOUS_CLIENT_SUCCESS, MyPublicAddress=%s:%s", vec->at(0).c_str(), vec->at(1).c_str());
            
            return;
        }
        case ijoon::CONNECTION_TARGET_INVALID:
        {
            ijn_print(DP_DEBUG, "received CONNECTION_TARGET_INVALID");
            if(client->onConnectionTargetInvalid != nullptr) {
                auto rendezvousSession = getRendezvousSessionSafety(client, messageHeader);
                client->onConnectionTargetInvalid(rendezvousSession);
            }
            return;
        }
        case ijoon::CONNECTION_ID_CREATED:
        {
            auto vec = ijoon::paramParser(body, 2);
            if(vec == nullptr) break;
            
            if(client->onConnectionIDCreated != nullptr) {
                client->onConnectionIDCreated(messageHeader.connectionID, vec->at(0), vec->at(1));
            }
            
            ijoon::send(kcpPeer, messageHeader.connectionID, ijoon::CONNECTION_ID_RECEIVED, (char *)vec->at(0).c_str(), vec->at(0).size());
        }
        case ijoon::DIRECT_CONNECTION_AVAILABLE: // SP only
        {
            ijn_print(DP_DEBUG, "received DIRECT_CONNECTION_AVAILABLE");
            
            auto vec = ijoon::paramParser(body, 2);
            if(vec == nullptr) break;
            
            if(client->onConnecting != nullptr) {
                auto rendezvousSession = getRendezvousSessionSafety(client, messageHeader);
                if(!rendezvousSession->isConnected()) {
                    client->onConnecting(rendezvousSession);
                }
            }
            
            auto targetPeer = ijoon::Peer(vec->at(0), vec->at(1));
            auto targetKcpPeer = client->getKcpPeer(targetPeer);

            ijoon::send(targetKcpPeer, messageHeader.connectionID, ijoon::DIRECT_CONNECTION_REQUEST, nullptr, 0);
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
                if(client->onConnected != nullptr) {
                    client->onConnected(rendezvousSession);
                }
            }
            
            ijoon::send(kcpPeer, messageHeader.connectionID, ijoon::DIRECT_CONNECTION_RESPONSE, nullptr, 0);
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
                if(client->onConnected != nullptr) {
                    client->onConnected(rendezvousSession);
                }
            }
            return;
        }
        case ijoon::REVERSE_CONNECTION_READY:
        {
            auto rendezvousSession = getRendezvousSessionSafety(client, messageHeader);
            if(!rendezvousSession->isConnected()) {
                if(client->onConnecting != nullptr) {
                    client->onConnecting(rendezvousSession);
                }
            }
            
            return;
        }
        case ijoon::REVERSE_CONNECTION: // TP only
        {
            ijn_print(DP_DEBUG, "received REVERSE_CONNECTION");
            
            auto vec = ijoon::paramParser(body, 2);
            if(vec == nullptr) break;
            
            auto sourcePeer = ijoon::Peer(vec->at(0), vec->at(1));
            auto sourceKcpPeer = client->getKcpPeer(sourcePeer);
            
            ijoon::send(sourceKcpPeer, messageHeader.connectionID, ijoon::REVERSE_CONNECTION_REQUEST, nullptr, 0);
            return;
        }
        case ijoon::REVERSE_CONNECTION_REQUEST: // SP only
        {
            ijn_print(DP_DEBUG, "received REVERSE_CONNECTION_REQUEST");
            
            ijoon::send(kcpPeer, messageHeader.connectionID, ijoon::REVERSE_CONNECTION_RESPONSE, nullptr, 0);
            
            auto rendezvousSession = getRendezvousSessionSafety(client, messageHeader);
            bool isConnected = rendezvousSession->isConnected();
            rendezvousSession->setPublicKcpPeer(kcpPeer);
            printf("reversely connected from %s:%d\n", peer.getIP().c_str(), peer.getPort());
            
            if(!isConnected) {
                if(client->onConnected != nullptr) {
                    client->onConnected(rendezvousSession);
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
                if(client->onConnected != nullptr) {
                    client->onConnected(rendezvousSession);
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
                if(client->onConnecting != nullptr) {
                    client->onConnecting(rendezvousSession);
                }
            }
            
            std::string data;
            
            auto publicPeer = ijoon::Peer(vec->at(0), vec->at(1));
            auto publicKcpPeer = client->getKcpPeer(publicPeer);
            
            data = "1";
            ijoon::send(publicKcpPeer, messageHeader.connectionID, ijoon::UDP_HOLE_PUNCHING_REQUEST, (char *)data.c_str(), data.length());
            
            auto privatePeer = ijoon::Peer(vec->at(2), vec->at(3));
            auto privateKcpPeer = client->getKcpPeer(privatePeer);
            
            data = "0";
            ijoon::send(privateKcpPeer, messageHeader.connectionID, ijoon::UDP_HOLE_PUNCHING_REQUEST, (char *)data.c_str(), data.length());
            
            return;
        }
        case ijoon::UDP_HOLE_PUNCHING_REQUEST:
        {
            ijn_print(DP_DEBUG, "received UDP_HOLE_PUNCHING_REQUEST");
            
            auto vec = ijoon::paramParser(body, 1);
            if(vec == nullptr) break;
            
            std::string data = vec->at(0);
            ijoon::send(kcpPeer, messageHeader.connectionID, ijoon::UDP_HOLE_PUNCHING_RESPONSE, (char *)data.c_str(), data.length());
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
                if(client->onConnected != nullptr) {
                    client->onConnected(rendezvousSession);
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
            if(client->onConnecting != nullptr) {
                client->onConnecting(rendezvousSession);
            }
            
            ijn_print(DP_INFO, "[RELAY_SERVER_INFORMATION] RelayServerAddress=%s:%s", vec->at(0).c_str(), vec->at(1).c_str());
            
            auto relayPeer = ijoon::Peer(vec->at(0), vec->at(1));
            auto relayKcpPeer = client->getKcpPeer(relayPeer);

            ijoon::send(relayKcpPeer, messageHeader.connectionID, ijoon::REGISTRATION_RELAY_PEER_REQUEST, (char *)vec->at(2).c_str(), vec->at(2).length());
            
            return;
        }
        case ijoon::CONNECTION_RELAY_SERVICE_SUCCESS:
        {
            ijn_print(DP_DEBUG, "received CONNECTION_RELAY_SERVICE_SUCCESS");
            
            auto vec = ijoon::paramParser(body, 2);
            if(vec == nullptr) break;
            
            
            auto relayPeer = ijoon::Peer(vec->at(0), vec->at(1));
            auto relayKcpPeer = client->getKcpPeer(relayPeer);
            
            auto rendezvousSession = getRendezvousSessionSafety(client, messageHeader);
            rendezvousSession->setRelayKcpPeer(relayKcpPeer);
            printf("connected by a relay from %s:%s\n", vec->at(0).c_str(), vec->at(1).c_str());
            
            if(client->onConnected != nullptr) {
                client->onConnected(rendezvousSession);
            }
            return;
        }
        case ijoon::CONNECTION_RELAY_SERVICE_FAILED:
        {
            ijn_print(DP_DEBUG, "received CONNECTION_RELAY_SERVICE_FAILED");
            return;
        }
        case ijoon::RELAY_SESSION_INVALID:
        {
            ijn_print(DP_DEBUG, "received RELAY_SESSION_INVALID, %d", messageHeader.connectionID);
            // relay disconnect
            if(client->rendezvousSessionMap.count(messageHeader.connectionID) == 0) {
                return;
            }

            auto rendezvousSession = client->rendezvousSessionMap.at(messageHeader.connectionID);
            auto lastPing = rendezvousSession->getRelayKcpPeer()->lastPing;
            rendezvousSession->clearRelayKcpPeer();
            ijn_print(DP_ERROR, "relay peer removed, %lu (connectionID=%d)", lastPing, messageHeader.connectionID);
            
            if(!rendezvousSession->isConnected()) {
                client->rendezvousSessionMap.erase(messageHeader.connectionID);
                
                // callback to user (disconnected)
                ijn_print(DP_INFO, "Disconnected, connectionID=%d", messageHeader.connectionID);
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

ijoon::THREAD_RET THREAD_API rawRecvThreadFunc(void *arg) {
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::RendezvousClient *rendezvousClient = (ijoon::RendezvousClient *)thread->getParam();
    
    auto peer = ijoon::Peer();
    
    char *rawBuffer = new char[MAX_PACKET_SIZE];
    
    rendezvousClient->socket->option(ijoon::SocketOptionType::SOCK_RCVTIMEO_MS, 1000);
    
    while(!thread->isInterrupted()) {
        int rcvSize = rendezvousClient->socket->recvFrom(&peer, rawBuffer, MAX_PACKET_SIZE);
        if(rcvSize > 0) {
            rendezvousClient->mutexForKcpPeerMap.lock();
            auto kcpPeer = rendezvousClient->getKcpPeer(peer);
            rendezvousClient->mutexForKcpPeerMap.unlock();
            
            kcpPeer->mutex.lock();
            ikcpcb *kcp = kcpPeer->getKcp();
            ikcp_input(kcp, rawBuffer, rcvSize);
            IUINT32 current = iclock();
            ikcp_update(kcp, current);
            kcpPeer->mutex.unlock();
        }
    }
    
    ijn_print(DP_DEBUG, "rawRecvThread finished");
    delete []rawBuffer;
    return THREAD_EXIT;
}

ijoon::THREAD_RET THREAD_API recvThreadFunc(void *param) {
    auto thread = static_cast<ijoon::Thread *>(param);
    ijoon::RendezvousClient *client = (ijoon::RendezvousClient *)thread->getParam();
    
    char *buffer = new char[MAX_PACKET_SIZE];
    
    while(!thread->isInterrupted()) {
        client->mutexForKcpPeerMap.lock();

        IUINT32 current = iclock();
        auto iter = client->kcpPeerMap.begin();
        for(; iter != client->kcpPeerMap.end() ; ++iter) {
            auto kcpPeer = iter->second;
            kcpPeer->mutex.lock();
            ikcpcb *kcp = kcpPeer->getKcp();
            int rcvSize = ikcp_recv(kcp, buffer, MAX_PACKET_SIZE);
            ikcp_update(kcp, current);
            kcpPeer->mutex.unlock();
            
            if(rcvSize > 0) {
                // callback to upper users
                buffer[rcvSize] = '\0';
                onCallback(client, kcpPeer, buffer, rcvSize);
            }
        }

        client->mutexForKcpPeerMap.unlock();
        
        ijn_msleep(10);
    }
    
    ijn_print(DP_DEBUG, "recvThread finished");
    delete[] buffer;
    return THREAD_EXIT;
}

void ijoon::RendezvousClient::onServerConnectingCallback() {
    if(onServerConnecting != nullptr) {
        onServerConnecting();
    }
}

void ijoon::RendezvousClient::onServerConnectFailedCallback() {
    if(onServerConnectFailed != nullptr) {
        onServerConnectFailed();
    }
}
void ijoon::RendezvousClient::onServerConnectedCallback(std::string extIP, std::string extPort) {
    if(onServerConnected != nullptr) {
        onServerConnected(extIP, extPort);
    }
}

void ijoon::RendezvousClient::onServerDisconnectedCallback() {
    if(onServerDisconnected != nullptr) {
        onServerDisconnected();
    }
}
