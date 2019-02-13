#include "rendezvous_server.h"
#include "rendezvous_message.h"
#include "message_header.h"
#include <cstring>

extern char seperator;

bool ijoon::RendezvousPeer::isPublic() {
    if( (this->publicPeer.getIP().compare(this->privatePeer.getIP()) == 0) && (this->publicPeer.getPort() == this->privatePeer.getPort()) ) {
        return true;
    }
    
    return false;
}

void ijoon::RendezvousServer::start() {
    ijn_print(DP_INFO, "Rendezvous server start...");
    
    thread = new ijoon::Thread(rendezvousThread, "rendezvous thread");
    thread->start(this);
    checkThread = new ijoon::Thread(rendezvousCheckThread, "rendezvous check thread");
    checkThread->start(this);
}

bool connection(ijoon::RendezvousServer *server, ijoon::MessageHeader messageHeader) {
    auto rendezvousPeerInfo = server->connectionInfoMap[messageHeader.connectionID];
    
    // SP/TP nat check
    auto spRendezvousPeer = server->rendezvousPeerMap[rendezvousPeerInfo->sp.getKey()];
    auto tpRendezvousPeer = server->rendezvousPeerMap[rendezvousPeerInfo->tp.getKey()];
    
    if(spRendezvousPeer == nullptr || tpRendezvousPeer == nullptr) {
        ijn_print(DP_ERROR, "[RELAY_SESSION_CREATED] No registered rendezvous peer detected");
        return false;
    }
    
    bool isSPPublic = spRendezvousPeer->isPublic();
    bool isTPPublic = tpRendezvousPeer->isPublic();
    
    if(isTPPublic) { // pub/pub, pri/pub
        std::string data = tpRendezvousPeer->publicPeer.getIP() + seperator + std::to_string(tpRendezvousPeer->publicPeer.getPort()); // tp public address
        ijoon::send(server->socket, spRendezvousPeer->publicPeer, messageHeader.connectionID, ijoon::RENDEZVOUS_MSG::DIRECT_CONNECTION_AVAILABLE, (char *)data.c_str(), data.length());
    }
    else if(isSPPublic && !isTPPublic) { // pub/pri
        std::string data = spRendezvousPeer->publicPeer.getIP() + seperator + std::to_string(spRendezvousPeer->publicPeer.getPort()); // sp public address
        ijoon::send(server->socket, tpRendezvousPeer->publicPeer, messageHeader.connectionID, ijoon::RENDEZVOUS_MSG::REVERSE_CONNECTION, (char *)data.c_str(), data.length());
    }
    else { // pri/pri
        std::string data;
        
        data = tpRendezvousPeer->publicPeer.getIP(); // tp public address
        data += seperator;
        data += std::to_string(tpRendezvousPeer->publicPeer.getPort());
        data += seperator;
        data += tpRendezvousPeer->privatePeer.getIP();
        data += seperator;
        data += std::to_string(tpRendezvousPeer->privatePeer.getPort());
        ijoon::send(server->socket, spRendezvousPeer->publicPeer, messageHeader.connectionID, ijoon::RENDEZVOUS_MSG::UDP_HOLE_PUNCHING_AVAILABLE, (char *)data.c_str(), data.length());
        
        data = spRendezvousPeer->publicPeer.getIP(); // sp public address
        data += seperator;
        data += std::to_string(spRendezvousPeer->publicPeer.getPort());
        data += seperator;
        data += spRendezvousPeer->privatePeer.getIP();
        data += seperator;
        data += std::to_string(spRendezvousPeer->privatePeer.getPort());
        ijoon::send(server->socket, tpRendezvousPeer->publicPeer, messageHeader.connectionID, ijoon::RENDEZVOUS_MSG::UDP_HOLE_PUNCHING_AVAILABLE, (char *)data.c_str(), data.length());
    }
    
    server->connectionInfoMap.erase(messageHeader.connectionID);
    
    return true;
}

ijoon::THREAD_RET THREAD_API ijoon::rendezvousThread(void *arg)
{
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::RendezvousServer *server = (ijoon::RendezvousServer *)thread->getParam();
    
    ijoon::Peer peer;

    char *packet = new char[MAX_PACKET_SIZE];
    
    while(!thread->isInterrupted())
    {
        memset(packet, 0, MAX_PACKET_SIZE);
        int recvSize = server->socket->recvFrom(&peer, packet, MAX_PACKET_SIZE);
        if(recvSize < 0) {
            ijn_print(DP_DEBUG, "timeout");
            continue;
        }
        
        if(server->relayServerMap.count(peer.getKey()) != 0) {
            auto relayPeer = server->relayServerMap[peer.getKey()];
            relayPeer->lastPing = ijoon::ComputableTime::getCurrentTimeSec();
        }
        
        if(server->rendezvousPeerMap.count(peer.getKey()) != 0) {
            auto rendezvousPeer = server->rendezvousPeerMap[peer.getKey()];
            rendezvousPeer->lastPing = ijoon::ComputableTime::getCurrentTimeSec();
        }
        
        MessageHeader messageHeader;
        int cursor = 0;
        if(!ijoon::readHeader(packet, recvSize, messageHeader, cursor)) {
            ijn_print(DP_ERROR, "invalid header");
            continue;
        }
        
        if(messageHeader.messageType != MESSAGE_TYPE::RAWBYTE) {
            ijn_print(DP_ERROR, "type error");
            continue;
        }
        
        if((recvSize-cursor) != messageHeader.dataSize) {
            ijn_print(DP_ERROR, "invalid data size");
            continue;
        }
        
        char *body = &packet[cursor];
        
        switch (messageHeader.packetType) {
            case REGISTRATION_RELAY_SERVER_REQUEST: // from RelS
            {
                ijn_print(DP_DEBUG, "received REGISTRATION_RELAY_SERVER_REQUEST");
                
                std::string key = peer.getKey();
                if(server->relayServerMap.count(key) == 0) {
                    server->relayServerMap[key] = std::shared_ptr<RendezvousPeer>(new ijoon::RendezvousPeer(server->socket, peer));
                    server->relayServerMap[key]->lastPing = ijoon::ComputableTime::getCurrentTimeSec();
                }
                
                ijoon::send(server->socket, peer, 0, REGISTRATION_RELAY_SERVER_SUCCESS);
                break;
            }
            case RELAY_SESSION_READY: // from RelS
            {
                ijn_print(DP_DEBUG, "received RELAY_SESSION_READY");
                
                if(server->connectionInfoMap.count(messageHeader.connectionID) == 0) {
                    ijn_print(DP_ERROR, "[RELAY_SESSION_READY] invalid request from relay server");
                    break;
                }
                
                auto rendezvousPeerInfo = server->connectionInfoMap[messageHeader.connectionID];
                
                auto sourcePeer = server->rendezvousPeerMap[rendezvousPeerInfo->sp.getKey()];
                auto targetPeer = server->rendezvousPeerMap[rendezvousPeerInfo->tp.getKey()];
                
                sourcePeer->relayPeer.setIP(peer.getIP());
                sourcePeer->relayPeer.setPort(std::to_string(peer.getPort()));
                targetPeer->relayPeer.setIP(peer.getIP());
                targetPeer->relayPeer.setPort(std::to_string(peer.getPort()));
                
                std::string data;
                data = peer.getIP() + seperator + std::to_string(peer.getPort()) + seperator + "1";
                ijoon::send(server->socket, rendezvousPeerInfo->sp, messageHeader.connectionID, RELAY_SERVER_INFORMATION, (char *)data.c_str(), data.length());
                data = peer.getIP() + seperator + std::to_string(peer.getPort()) + seperator + "0";
                ijoon::send(server->socket, rendezvousPeerInfo->tp, messageHeader.connectionID, RELAY_SERVER_INFORMATION, (char *)data.c_str(), data.length());
                
                break;
            }
            case RELAY_SESSION_CREATED: // from RelS
            {
                ijn_print(DP_DEBUG, "received RELAY_SESSION_CREATED");
                
                if(server->connectionInfoMap.count(messageHeader.connectionID) == 0) {
                    ijn_print(DP_ERROR, "[RELAY_SESSION_CREATED] relay connection info not exist");
                    break;
                }
                auto rendezvousPeerInfo = server->connectionInfoMap[messageHeader.connectionID];
                
                // send connected packet
                std::string data = peer.getIP() + seperator + std::to_string(peer.getPort());
                ijoon::send(server->socket, rendezvousPeerInfo->sp, messageHeader.connectionID, CONNECTION_RELAY_SERVICE_SUCCESS, (char *)data.c_str(), data.length());
                ijoon::send(server->socket, rendezvousPeerInfo->tp, messageHeader.connectionID, CONNECTION_RELAY_SERVICE_SUCCESS, (char *)data.c_str(), data.length());
                
                connection(server, messageHeader);
                
                break;
            }
            case RELAY_SESSION_CREATING_FAILED: // from RelS
            {
                ijn_print(DP_DEBUG, "received RELAY_SESSION_CREATING_FAILED");
                
                auto rendezvousPeerInfo = server->connectionInfoMap[messageHeader.connectionID];
                ijoon::send(server->socket, rendezvousPeerInfo->sp, messageHeader.connectionID, CONNECTION_RELAY_SERVICE_FAILED);
                connection(server, messageHeader);
                break;
            }
            case REGISTRATION_RENDEZVOUS_CLIENT_REQUEST: // from SP, TP
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
                if(server->rendezvousPeerMap.count(key) == 0) {
                    auto rendezvousPeer = std::shared_ptr<RendezvousPeer>(new RendezvousPeer(server->socket, peer));
                    rendezvousPeer->lastPing = ijoon::ComputableTime::getCurrentTimeSec();
                    server->rendezvousPeerMap[key] = rendezvousPeer;
                    ijn_print(DP_INFO, "[REGISTRATION_RENDEZVOUS_CLIENT_REQUEST] new rendezvous peer registered");
                }
                
                auto rendezvousPeer = server->rendezvousPeerMap[key];
                
                rendezvousPeer->setPrivatePeer(vec[0], vec[1]);
                
                ijn_print(DP_INFO, "Register local=%s:%d, public=%s:%d", rendezvousPeer->privatePeer.getIP().c_str(), rendezvousPeer->privatePeer.getPort(), rendezvousPeer->publicPeer.getIP().c_str(), rendezvousPeer->publicPeer.getPort());
                
                std::string data;
                data = peer.getIP();
                data += seperator;
                data += std::to_string(peer.getPort());
                ijoon::send(server->socket, rendezvousPeer->publicPeer, 0, REGISTRATION_RENDEZVOUS_CLIENT_SUCCESS, (char *)data.c_str(), data.length());
                break;
            }
            case CONNECTION_REQUEST: // from SP
            {
                ijn_print(DP_DEBUG, "received CONNECTION_REQUEST");
                
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
                
                auto sourcePeer = server->rendezvousPeerMap[peer.getKey()];
                auto targetPeer = server->rendezvousPeerMap[vec[0] + ":" + vec[1]];
                if(sourcePeer == nullptr || targetPeer == nullptr) {
                    ijoon::send(server->socket, peer, 0, CONNECTION_FAILED);
                    break;
                }
                
                // create connectionID
                if(server->connectionIDCursor > 1000000000) {
                    server->connectionIDCursor = 0;
                }
                int connectionID = server->connectionIDCursor++;
                
                std::shared_ptr<ijoon::RendezvousPeerInfo> connectionInfo = std::shared_ptr<ijoon::RendezvousPeerInfo>(new ijoon::RendezvousPeerInfo());
                connectionInfo->connectionID = connectionID;
                connectionInfo->sp = peer;
                connectionInfo->tp.setIP(vec[0]);
                connectionInfo->tp.setPort(vec[1]);
                server->connectionInfoMap[connectionID] = connectionInfo;
                
                sourcePeer->connectionID = connectionID;
                targetPeer->connectionID = connectionID;
                messageHeader.connectionID = connectionID;
                if(server->relayServerMap.size() == 0) {
                    ijn_print(DP_INFO, "[CONNECTION_REQUEST] no relay server");
                    ijoon::send(server->socket, peer, 0, CONNECTION_RELAY_SERVICE_FAILED);
                    connection(server, messageHeader);
                    break;
                }
                
                // note: implement this (relay server selection algorithm)
                std::shared_ptr<RendezvousPeer> relayPeer;
                std::map<std::string, std::shared_ptr<RendezvousPeer>>::iterator iter;
                for(iter = server->relayServerMap.begin(); iter != server->relayServerMap.end() ; ++iter ) {
                    relayPeer = iter->second;
                    break;
                }
                
                std::string data;
                data += sourcePeer->publicPeer.getIP();
                data += seperator;
                data += vec[0];
                
                ijoon::send(server->socket, relayPeer->publicPeer, connectionID, RELAY_SERVICE_REQUEST, (char *)data.c_str(), data.length());
                break;
            }
            default:
            {
                ijn_print(DP_ERROR, "Undefined message received");
                break;
            }
        }
    }
    
    ijn_print(DP_INFO, "rendezvousThread Finished.");
    delete[] packet;
    
    return THREAD_EXIT;
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
                std::string publicIP = iter->second->publicPeer.getIP();
                int publicPort = iter->second->publicPeer.getPort();
                
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
                std::string publicIP = iter->second->publicPeer.getIP();
                int publicPort = iter->second->publicPeer.getPort();
                
                server->relayServerMap.erase(iter);
                ijn_print(DP_INFO, "relay peer removed, %s:%d (%ud)", publicIP.c_str(), publicPort, lastPing);
            }
            break;
        }
        
    }
    
    return THREAD_EXIT;
}
