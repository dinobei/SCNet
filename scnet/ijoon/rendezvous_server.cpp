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
    // socket을 생성하고, port를 바인딩
    // 스레드를 실행시킴
    // 스레드에서는 명령 수신 대기를 함
}

ijoon::THREAD_RET THREAD_API ijoon::rendezvousThread(void *arg)
{
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::RendezvousServer *server = (ijoon::RendezvousServer *)thread->getParam();
    
    ijoon::Peer peer;

    char *packet = new char[MAX_PACKET_SIZE];
    
    std::shared_ptr<UDPSocket> socket = std::shared_ptr<UDPSocket>(new UDPSocket(server->serverPort));
    while(!thread->isInterrupted())
    {
        memset(packet, 0, MAX_PACKET_SIZE);
        int recvSize = socket->recvFrom(&peer, packet, MAX_PACKET_SIZE);
        if(recvSize < 0) {
            ijn_print(DP_DEBUG, "timeout");
            continue;
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
                
                std::string key = peer.getIP() + ":" + std::to_string(peer.getPort());
                if(server->relayServerMap.count(key) == 0) {
                    server->relayServerMap[key] = std::shared_ptr<ijoon::Peer>(new ijoon::Peer(peer.getIP(), std::to_string(peer.getPort())));
                }
                
                ijoon::send(socket, peer, 0, REGISTRATION_RELAY_SERVER_SUCCESS);
                break;
            }
            case RELAY_SESSION_READY: // from RelS
            {
                ijn_print(DP_DEBUG, "received RELAY_SESSION_READY");
                
                if(server->rendezvousPeerInfoMap.count(messageHeader.connectionID) == 0) {
                    ijn_print(DP_ERROR, "[RELAY_SESSION_READY] invalid request from relay server");
                    break;
                }
                
                auto rendezvousPeerInfo = server->rendezvousPeerInfoMap[messageHeader.connectionID];
                
                std::string data = peer.getIP() + seperator + std::to_string(peer.getPort());
                ijoon::send(socket, rendezvousPeerInfo->sp, messageHeader.connectionID, RELAY_SERVER_INFORMATION, (char *)data.c_str(), data.length());
                ijoon::send(socket, rendezvousPeerInfo->tp, messageHeader.connectionID, RELAY_SERVER_INFORMATION, (char *)data.c_str(), data.length());
                
                break;
            }
            case RELAY_SESSION_CREATED: // from RelS
            {
                ijn_print(DP_DEBUG, "received RELAY_SESSION_CREATED");
                
                // 릴레이 서버에 SP/TP가 등록을 마쳤다는 뜻
                // SP에 CONNECTION_REQUEST에 대한 응답을 해줌

                if(server->rendezvousPeerInfoMap.count(messageHeader.connectionID) == 0) {
                    ijn_print(DP_ERROR, "[RELAY_SESSION_CREATED] relay connection info not exist");
                    break;
                }
                auto rendezvousPeerInfo = server->rendezvousPeerInfoMap[messageHeader.connectionID];
                
                // send connected packet
                std::string data = peer.getIP() + seperator + std::to_string(peer.getPort());
                ijoon::send(socket, rendezvousPeerInfo->sp, messageHeader.connectionID, CONNECTION_RELAY_SERVICE_SUCCESS, (char *)data.c_str(), data.length());
                ijoon::send(socket, rendezvousPeerInfo->tp, messageHeader.connectionID, CONNECTION_RELAY_SERVICE_SUCCESS, (char *)data.c_str(), data.length());
                
                // SP와 TP의 환경 체크
                std::string spKey = rendezvousPeerInfo->sp.getIP() + ":" + std::to_string(rendezvousPeerInfo->sp.getPort());
                std::string tpKey = rendezvousPeerInfo->tp.getIP() + ":" + std::to_string(rendezvousPeerInfo->tp.getPort());
                auto spRendezvousPeer = server->registeredRendezvousPeer[spKey];
                auto tpRendezvousPeer = server->registeredRendezvousPeer[tpKey];
                
                if(spRendezvousPeer == nullptr || tpRendezvousPeer == nullptr) {
                    ijn_print(DP_ERROR, "[RELAY_SESSION_CREATED] No registered rendezvous peer detected");
                    break;
                }
                
                bool isSPPublic = spRendezvousPeer->isPublic();
                bool isTPPublic = tpRendezvousPeer->isPublic();
                
                if(isTPPublic) { // pub/pub, pri/pub
                    std::string data = tpRendezvousPeer->publicPeer.getIP() + seperator + std::to_string(tpRendezvousPeer->publicPeer.getPort()); // tp public address
                    ijoon::send(socket, spRendezvousPeer->publicPeer, messageHeader.connectionID, DIRECT_CONNECTION_AVAILABLE, (char *)data.c_str(), data.length());
                }
                else if(isSPPublic && !isTPPublic) { // pub/pri
                    std::string data = spRendezvousPeer->publicPeer.getIP() + seperator + std::to_string(spRendezvousPeer->publicPeer.getPort()); // sp public address
                    ijoon::send(socket, tpRendezvousPeer->publicPeer, messageHeader.connectionID, REVERSE_CONNECTION, (char *)data.c_str(), data.length());
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
                    ijoon::send(socket, spRendezvousPeer->publicPeer, messageHeader.connectionID, UDP_HOLE_PUNCHING_AVAILABLE, (char *)data.c_str(), data.length());
                    
                    data = spRendezvousPeer->publicPeer.getIP(); // sp public address
                    data += seperator;
                    data += std::to_string(spRendezvousPeer->publicPeer.getPort());
                    data += seperator;
                    data += spRendezvousPeer->privatePeer.getIP();
                    data += seperator;
                    data += std::to_string(spRendezvousPeer->privatePeer.getPort());
                    ijoon::send(socket, tpRendezvousPeer->publicPeer, messageHeader.connectionID, UDP_HOLE_PUNCHING_AVAILABLE, (char *)data.c_str(), data.length());
                }
                
                break;
            }
            case RELAY_SESSION_CREATING_FAILED: // from RelS
            {
                ijn_print(DP_DEBUG, "received RELAY_SESSION_CREATING_FAILED");
                
                // 릴레이 서버에 SP/TP가 등록을 하지 못했다는 뜻
                // SP에 CONNECTION_REQUEST에 대한 응답을 해줌
                // 단, 일반적인 상황은 아니며, 준비된 릴레이서버가 없거나 모두 죽었을 경우 발생할 수 있는 메시지임
                // 이 경우 피어들간 연결이 안되는 증상이 발생할 수도 있음.
                
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
                
                std::string key = peer.getIP() + ":" + std::to_string(peer.getPort());
                if(server->registeredRendezvousPeer.count(key) == 0) {
                    server->registeredRendezvousPeer[key] = std::shared_ptr<RendezvousPeer>(new RendezvousPeer(socket, peer));
                    ijn_print(DP_INFO, "[REGISTRATION_RENDEZVOUS_CLIENT_REQUEST] new rendezvous peer registered");
                }
                
                auto rendezvousPeer = server->registeredRendezvousPeer[key];
                
                rendezvousPeer->setPrivatePeer(vec[0], vec[1]);
                
                ijn_print(DP_INFO, "Register local=%s:%d, public=%s:%d", rendezvousPeer->privatePeer.getIP().c_str(), rendezvousPeer->privatePeer.getPort(), rendezvousPeer->publicPeer.getIP().c_str(), rendezvousPeer->publicPeer.getPort());
                
                std::string data;
                data = peer.getIP();
                data += seperator;
                data += std::to_string(peer.getPort());
                ijoon::send(socket, rendezvousPeer->publicPeer, 0, REGISTRATION_RENDEZVOUS_CLIENT_SUCCESS, (char *)data.c_str(), data.length());
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
                
                std::string key = peer.getIP() + ":" + std::to_string(peer.getPort());
                auto rendezvousPeer = server->registeredRendezvousPeer[key];
                if(rendezvousPeer == nullptr) {
                    ijoon::send(socket, peer, 0, CONNECTION_FAILED);
                    break;
                }
                
                // connectionID 생성
                int connectionID = server->rendezvousPeerInfoMap.size();
                while(server->rendezvousPeerInfoMap.count(connectionID) != 0) {
                    connectionID++;
                }
                std::shared_ptr<ijoon::RendezvousPeerInfo> rendezvousPeerInfo = std::shared_ptr<ijoon::RendezvousPeerInfo>(new ijoon::RendezvousPeerInfo());
                rendezvousPeerInfo->connectionID = connectionID;
                rendezvousPeerInfo->sp = peer;
                rendezvousPeerInfo->tp.setIP(vec[0]);
                rendezvousPeerInfo->tp.setPort(vec[1]);
                server->rendezvousPeerInfoMap[connectionID] = rendezvousPeerInfo;

                std::string data;
                data += rendezvousPeer->publicPeer.getIP();
                data += seperator;
                data += vec[0];
                
                if(server->relayServerMap.size() == 0) {
                    ijn_print(DP_INFO, "[CONNECTION_REQUEST] no relay server");
                    ijoon::send(socket, peer, 0, CONNECTION_RELAY_SERVICE_FAILED);
                    break;
                }
                
                // note: implement this (relay server selection algorithm)
                std::shared_ptr<Peer> relayPeer;
                std::map<std::string, std::shared_ptr<Peer>>::iterator iter;
                for(iter = server->relayServerMap.begin(); iter != server->relayServerMap.end() ; ++iter ) {
                    relayPeer = iter->second;
                    break;
                }
                
                ijoon::send(socket, relayPeer, connectionID, RELAY_SERVICE_REQUEST, (char *)data.c_str(), data.length());
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
