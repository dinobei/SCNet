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
    ijoon::KcpPeer *kcpPeer = (ijoon::KcpPeer *)user;
    ijoon::Peer peer = kcpPeer->getPeer();
    kcpPeer->getClientSocket()->sendTo(&peer, const_cast<char *>(buf), len);
    return 0;
}

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

#ifdef __IJN_WINDOWS__
    closesocket(sock);
#else
    close(sock);
#endif
    
	return std::string(buffer);
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
}

void ijoon::RendezvousClient::connect(std::string ip, std::string port) {
    if(ip == serverIP && port == serverPort) {
        throw std::string("Can't connect to rendezvous server.");
    }
    
    auto serverKcpPeer = getKcpPeer(ijoon::Peer(serverIP, serverPort));
    if(serverKcpPeer->status != ijoon::PeerStatus::REGISTERED) {
        throw std::string("You must be connected to the rendezvous server to connect to client.");
    }
    
    ijoon::Peer peer(ip, port);
    auto key = peer.getKey();
    if(kcpPeerMap.count(key) != 0) {
        auto kcpPeer = kcpPeerMap.at(key);
    }
    
    const char seperator = ' ';
    std::string targetAddress;
    targetAddress = ip;
    targetAddress += seperator;
    targetAddress += port;
    serverKcpPeer->send(ijoon::CONNECTION_REQUEST, (char *)targetAddress.c_str(), targetAddress.length());
}

std::shared_ptr<ijoon::KcpPeer> ijoon::RendezvousClient::getKcpPeer(ijoon::Peer peer) {
    std::shared_ptr<ijoon::KcpPeer> kcpPeer;
    try {
        kcpPeer = this->kcpPeerMap.at(peer.getKey());
    } catch (std::exception e) {
        kcpPeer = std::shared_ptr<ijoon::KcpPeer>(new ijoon::KcpPeer(this->socket, peer.getIP(), std::to_string(peer.getPort()), udp_output));
        this->kcpPeerMap[peer.getKey()] = kcpPeer;
    }
    
    return kcpPeer;
}

std::shared_ptr<ijoon::KcpPeer> ijoon::RendezvousClient::getKcpPeer(int connectionID) {
    if(connFilterMap.count(connectionID) == 0) {
        return nullptr;
    }
    
    auto iter = --connFilterMap.upper_bound(connectionID);
    if(iter != connFilterMap.end()) {
        auto kcpPeer = getKcpPeer(iter->second);
        kcpPeer->connectionID = connectionID;
        return kcpPeer;
    }
    
    return nullptr;
}

ijoon::THREAD_RET THREAD_API registerThreadFunc(void *arg) {
    auto thread = static_cast<ijoon::Thread *>(arg);
    auto client = static_cast<ijoon::RendezvousClient *>(thread->getParam());
    
    const int timeoutSec = 15;//60;
    const int pingIntervalSec = 5;//20;
    const int loopIntervalMs = 5 * 1000;
    bool connFlag = false;
    
    while(!thread->isInterrupted()) {
        time_t currentTime = ijoon::ComputableTime::getCurrentTimeSec();
        
        client->mutexForKcpPeerMap.lock();
        auto iter = client->kcpPeerMap.begin();
        auto end = client->kcpPeerMap.end();
        while(iter != end) {
            auto kcpPeer = iter->second;
            
            switch (kcpPeer->type) {
                case ijoon::PeerType::RENDEZVOUS_SERVER:
                {
                    if(kcpPeer->status != ijoon::PeerStatus::REGISTERED) {
                        // send registration packet
                        if(connFlag) {
                            kcpPeer = std::shared_ptr<ijoon::KcpPeer>(new ijoon::KcpPeer(client->socket, client->serverIP, client->serverPort, udp_output));
                            kcpPeer->type = ijoon::PeerType::RENDEZVOUS_SERVER;
                            client->kcpPeerMap[kcpPeer->getPeer().getKey()] = kcpPeer;
                        }
                        else {
                            connFlag = true;
                        }
                        
                        std::string localIP = GetPrimaryIp();
                        int localPort = getPort(client);
                        std::string serial = client->serial;
                        std::string mac = client->mac;
                        std::string ver = client->version;
                        ijn_print(DP_INFO, "localAddress: %s:%d, serial: %s, mac: %s, version: %s", localIP.c_str(), localPort, serial.c_str(), mac.c_str(), ver.c_str());
                        std::string data = localIP + seperator + std::to_string(localPort) + seperator + client->serial + seperator + mac + seperator + ver;
                        
                        client->callback.onServerConnectingCallback();
                        kcpPeer->send(ijoon::REGISTRATION_RENDEZVOUS_CLIENT_REQUEST,
                                      (char *)data.c_str(),
                                      data.length());
                        
                    }
                    else if(kcpPeer->lastPing + timeoutSec < currentTime) {
                        // disconnected
                        kcpPeer->status = ijoon::PeerStatus::UNREGISTERED;
                        client->callback.onServerDisconnectedCallback();
                    }
                    else if(kcpPeer->lastPing + pingIntervalSec < currentTime) {
                        kcpPeer->send(ijoon::PING_REQUEST);
                    }
                }
                    break;
                default:
                {
                    if(kcpPeer->status == ijoon::PeerStatus::UNREGISTERED ||
                       kcpPeer->lastPing + timeoutSec < currentTime) {
                        ijn_print(DP_INFO, "removed kcpPeer: %s", kcpPeer->getPeer().getKey().c_str());
                        kcpPeer->status = ijoon::PeerStatus::UNREGISTERED;
                        auto removedPeerKey = iter->first;
                        iter = client->kcpPeerMap.erase(iter);
                        
                        // cascading update
                        for(auto iter = client->connFilterMap.begin() ; iter != client->connFilterMap.end() ; ) {
                            if(iter->second.getKey() == removedPeerKey) {
                                auto delConnectionID = iter->first;
                                auto delKcpPeer = client->getKcpPeer(iter->second);
                                delKcpPeer->connectionID = delConnectionID;
                                delKcpPeer->status = ijoon::PeerStatus::UNREGISTERED;
                                iter = client->connFilterMap.erase(iter);
                                client->callback.onDisconnectedCallback(delKcpPeer);
                                continue;
                            }
                            
                            ++iter;
                        }
                        
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
        client->mutexForKcpPeerMap.unlock();
        
        thread->sleep(loopIntervalMs);
    }
    
    ijn_print(DP_DEBUG, "registerThread finished");
    return THREAD_EXIT;
}

void onCallback(ijoon::RendezvousClient *client, const std::shared_ptr<ijoon::KcpPeer>& kcpPeer, char *packet, int recvSize) {
    ijoon::Peer peer = kcpPeer->getPeer();
    ijoon::MessageHeader messageHeader{};
    int cursor = 0;
    if(!ijoon::readHeader(packet, recvSize, messageHeader, cursor)) {
        return;
    }
    
    if((recvSize-cursor) != messageHeader.dataSize) {
        return;
    }
    
    kcpPeer->connectionID = messageHeader.connectionID;
    
    char *body = &packet[cursor];
    
    auto registry = Registry<int, google::protobuf::Message *>::Get();
    switch(messageHeader.messageType) {
        case ijoon::PROTOBUF:
        {
            if(kcpPeer->type == ijoon::PeerType::NONE) {
                ijn_print(DP_ERROR, "Unregistered peer (%s)", peer.getKey().c_str());
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
                    ijn_print(DP_ERROR, "Unregistered peer (%s), %d", peer.getKey().c_str(), messageHeader.packetType);
                    return;
                }
                
                kcpPeer->lastPing = ijoon::ComputableTime::getCurrentTimeSec();
            }
            else {
                kcpPeer->lastPing = ijoon::ComputableTime::getCurrentTimeSec();
            }

            auto callbackWrapper = registry->GetCallbackWrapper(messageHeader.messageType, messageHeader.packetType);
            if(callbackWrapper != nullptr) {
                callbackWrapper->callback(kcpPeer, body, messageHeader.dataSize);
            }
            else {
                ijn_print(DP_ERROR, "Not registered raw message received. mtype=%d, body=%s", messageHeader.messageType, body);
            }

            return;
        }
        default:
            ijn_print(DP_ERROR, "Unknown message_type(%d) received", messageHeader.messageType);
            return;
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

void ijoon::RendezvousClientLifeCycleCallback::onServerConnectingCallback() {
    if(onServerConnecting != nullptr) {
        onServerConnecting();
    }
}

void ijoon::RendezvousClientLifeCycleCallback::onServerConnectFailedCallback() {
    if(onServerConnectFailed != nullptr) {
        onServerConnectFailed();
    }
}

void ijoon::RendezvousClientLifeCycleCallback::onServerConnectedCallback(std::string extIP, std::string extPort) {
    if(onServerConnected != nullptr) {
        onServerConnected(extIP, extPort);
    }
}

void ijoon::RendezvousClientLifeCycleCallback::onServerDisconnectedCallback() {
    if(onServerDisconnected != nullptr) {
        onServerDisconnected();
    }
}

void ijoon::RendezvousClientLifeCycleCallback::onConnectingCallback(int connectionID) {
    if(onConnecting != nullptr) {
        onConnecting(connectionID);
    }
}

void ijoon::RendezvousClientLifeCycleCallback::onConnectedCallback(std::shared_ptr<ijoon::KcpPeer> rendezvousClient) {
    if(onConnected != nullptr) {
        onConnected(rendezvousClient);
    }
}

void ijoon::RendezvousClientLifeCycleCallback::onConnectFailedCallback(std::string ip, std::string port) {
    if(onConnectFailed != nullptr) {
        onConnectFailed(ip, port);
    }
}

void ijoon::RendezvousClientLifeCycleCallback::onDisconnectedCallback(std::shared_ptr<ijoon::KcpPeer> rendezvousClient) {
    if(onDisconnected != nullptr) {
        onDisconnected(rendezvousClient);
    }
}
