#include "randezvous_server.h"
#include "randezvous_message.h"
#include "message_header.h"
#include <cstring>

extern char seperator;

bool ijoon::RandezvousPeer::isPublic() {
    if( (this->publicPeer.getIP().compare(this->privatePeer.getIP()) == 0) && (this->publicPeer.getPort() == this->privatePeer.getPort()) ) {
        return true;
    }
    
    return false;
}

void ijoon::RandezvousServer::start() {
    ijn_print(DP_INFO, "Randezvous server start...");
    
    thread = new ijoon::Thread(randezvousThread, "randezvous thread");
    thread->start(this);
    // socket을 생성하고, port를 바인딩
    // 스레드를 실행시킴
    // 스레드에서는 명령 수신 대기를 함
}

ijoon::THREAD_RET THREAD_API ijoon::randezvousThread(void *arg)
{
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::RandezvousServer *server = (ijoon::RandezvousServer *)thread->getParam();
    
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
                ijn_print(DP_DEBUG, "received REGISTER_RELAY_REQUEST");
                
                std::string key = peer.getIP() + ":" + std::to_string(peer.getPort());
                if(server->relayServerMap.count(key) == 0) {
                    server->relayServerMap[key] = std::shared_ptr<ijoon::Peer>(new ijoon::Peer(peer.getIP(), std::to_string(peer.getPort())));
                }
                
                ijoon::send(socket, peer, 0, REGISTRATION_RELAY_SERVER_SUCCESS);
                break;
            }
            case RELAY_SESSION_READY: // from RelS
            {
                ijn_print(DP_DEBUG, "received RELAY_SERVICE_READY");
                
                std::vector<std::string> vec;
                char *token = std::strtok(body, &seperator);
                while (token != NULL) {
                    vec.push_back(token);
                    token = std::strtok(NULL, &seperator);
                }
                if(vec.size() != 4) {
                    ijn_print(DP_ERROR, "[RELAY_SERVICE_READY] invalid parameters");
                    break;
                }
                
                std::string data;
                
                data = peer.getIP();
                data += seperator;
                data += std::to_string(peer.getPort());
                data += seperator;
                data += vec[2]; // TP ip
                data += seperator;
                data += vec[3]; // TP port
                Peer sourcePeer(vec[0], vec[1]);
                ijoon::send(socket, sourcePeer, messageHeader.connectionID, RELAY_SERVER_INFORMATION, (char *)data.c_str(), data.length());

                data = peer.getIP();
                data += seperator;
                data += std::to_string(peer.getPort());
                data += seperator;
                data += vec[0]; // SP ip
                data += seperator;
                data += vec[1]; // SP port
                Peer targetPeer(vec[2], vec[3]);
                ijoon::send(socket, targetPeer, messageHeader.connectionID, RELAY_SERVER_INFORMATION, (char *)data.c_str(), data.length());
                
                break;
            }
            case RELAY_SESSION_CREATED: // from RelS
            {
                ijn_print(DP_DEBUG, "received RELAY_SERVICE_RESPONSE_SUCCESS");
                
                // 릴레이 서버에 SP/TP가 등록을 마쳤다는 뜻
                // SP에 CONNECTION_REQUEST에 대한 응답을 해줌
                
                std::vector<std::string> vec;
                char *token = std::strtok(body, &seperator);
                while (token != NULL) {
                    vec.push_back(token);
                    token = std::strtok(NULL, &seperator);
                }
                if(vec.size() != 4) {
                    ijn_print(DP_ERROR, "[RELAY_SERVICE_RESPONSE_SUCCESS] invalid parameters");
                    break;
                }

                // send connected packet
                std::string data;
                
                data = peer.getIP();
                data += seperator;
                data += std::to_string(peer.getPort());
                data += seperator;
                data += vec[2];
                data += seperator;
                data += vec[3];
                ijoon::Peer sourcePeer(vec[0], vec[1]);
                ijoon::send(socket, sourcePeer, messageHeader.connectionID, CONNECTION_RELAY_SERVICE_SUCCESS, (char *)data.c_str(), data.length());
                
                data = peer.getIP();
                data += seperator;
                data += std::to_string(peer.getPort());
                data += seperator;
                data += vec[0];
                data += seperator;
                data += vec[1];
                ijoon::Peer targetPeer(vec[2], vec[3]);
                ijoon::send(socket, targetPeer, messageHeader.connectionID, CONNECTION_RELAY_SERVICE_SUCCESS, (char *)data.c_str(), data.length());

                
                // SP와 TP의 환경 체크
                std::string spKey = vec[0] + ":" + vec[1];
                std::string tpKey = vec[2] + ":" + vec[3];
                auto spRandezvousPeer = server->registeredRandezvousPeer[spKey];
                auto tpRandezvousPeer = server->registeredRandezvousPeer[tpKey];
                
                if(spRandezvousPeer == nullptr || tpRandezvousPeer == nullptr) {
                    ijn_print(DP_ERROR, "[RELAY_SERVICE_RESPONSE_SUCCESS] No registered randezvous peer detected");
                    break;
                }
                
                bool isSPPublic = spRandezvousPeer->isPublic();
                bool isTPPublic = tpRandezvousPeer->isPublic();
                
                if(isTPPublic) { // pub/pub, pri/pub
                    std::string data = vec[2] + seperator + vec[3]; // tp public address
                    ijoon::send(socket, spRandezvousPeer->publicPeer, messageHeader.connectionID, DIRECT_CONNECTION_AVAILABLE, (char *)data.c_str(), data.length());
                }
                else if(isSPPublic && !isTPPublic) { // pub/pri
                    std::string data;
                    data = vec[0]; // sp public address
                    data += seperator;
                    data += vec[1];
                    ijoon::send(socket, tpRandezvousPeer->publicPeer, messageHeader.connectionID, REVERSE_CONNECTION, (char *)data.c_str(), data.length());
                }
                else { // pri/pri
                    std::string data;
                    
                    data = tpRandezvousPeer->publicPeer.getIP(); // tp public address
                    data += seperator;
                    data += std::to_string(tpRandezvousPeer->publicPeer.getPort());
                    data += seperator;
                    data += tpRandezvousPeer->privatePeer.getIP();
                    data += seperator;
                    data += std::to_string(tpRandezvousPeer->privatePeer.getPort());
                    ijoon::send(socket, spRandezvousPeer->publicPeer, messageHeader.connectionID, UDP_HOLE_PUNCHING_AVAILABLE, (char *)data.c_str(), data.length());
                    
                    data = spRandezvousPeer->publicPeer.getIP(); // sp public address
                    data += seperator;
                    data += std::to_string(spRandezvousPeer->publicPeer.getPort());
                    data += seperator;
                    data += spRandezvousPeer->privatePeer.getIP();
                    data += seperator;
                    data += std::to_string(spRandezvousPeer->privatePeer.getPort());
                    ijoon::send(socket, tpRandezvousPeer->publicPeer, messageHeader.connectionID, UDP_HOLE_PUNCHING_AVAILABLE, (char *)data.c_str(), data.length());
                }
                
                break;
            }
            case RELAY_SESSION_CREATING_FAILED: // from RelS
            {
                ijn_print(DP_DEBUG, "received RELAY_SERVICE_RESPONSE_FAILED");
                
                // 릴레이 서버에 SP/TP가 등록을 하지 못했다는 뜻
                // SP에 CONNECTION_REQUEST에 대한 응답을 해줌
                // 단, 일반적인 상황은 아니며, 준비된 릴레이서버가 없거나 모두 죽었을 경우 발생할 수 있는 메시지임
                // 이 경우 피어들간 연결이 안되는 증상이 발생할 수도 있음.
                
                break;
            }
            case REGISTRATION_RENDEZVOUS_CLIENT_REQUEST: // from SP, TP
            {
                ijn_print(DP_DEBUG, "received REGISTER_REQUEST");
                
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
                if(server->registeredRandezvousPeer.count(key) == 0) {
                    server->registeredRandezvousPeer[key] = std::shared_ptr<RandezvousPeer>(new RandezvousPeer(socket, peer));
                    ijn_print(DP_INFO, "[REGISTER_REQUEST] new randezvous peer registered");
                }
                
                auto randezvousPeer = server->registeredRandezvousPeer[key];
                
                randezvousPeer->setPrivatePeer(vec[0], vec[1]);
                
                ijn_print(DP_INFO, "Register local=%s:%d, public=%s:%d", randezvousPeer->privatePeer.getIP().c_str(), randezvousPeer->privatePeer.getPort(), randezvousPeer->publicPeer.getIP().c_str(), randezvousPeer->publicPeer.getPort());
                
                std::string data;
                data = peer.getIP();
                data += seperator;
                data += std::to_string(peer.getPort());
                ijoon::send(socket, randezvousPeer->publicPeer, 0, REGISTRATION_RENDEZVOUS_CLIENT_SUCCESS, (char *)data.c_str(), data.length());
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
                auto randezvousPeer = server->registeredRandezvousPeer[key];
                if(randezvousPeer == nullptr) {
                    ijoon::send(socket, peer, 0, CONNECTION_FAILED);
                    break;
                }
                
                // connectionID 생성
                int connectionID = server->randezvousPeerInfoMap.size();
                while(server->randezvousPeerInfoMap.count(connectionID) != 0) {
                    connectionID++;
                }
                std::shared_ptr<ijoon::RandezvousPeerInfo> randezvousPeerInfo = std::shared_ptr<ijoon::RandezvousPeerInfo>(new ijoon::RandezvousPeerInfo());
                randezvousPeerInfo->connectionID = connectionID;
                randezvousPeerInfo->spUniqueKey = peer.getIP() + seperator + std::to_string(peer.getPort());
                randezvousPeerInfo->tpUniqueKey = vec[0] + seperator + vec[1];
                server->randezvousPeerInfoMap[connectionID];

                std::string data;
                data += randezvousPeer->publicPeer.getIP();
                data += seperator;
                data += std::to_string(randezvousPeer->publicPeer.getPort());
                data += seperator;
                data += vec[0];
                data += seperator;
                data += vec[1];
                
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
    
    ijn_print(DP_INFO, "randezvousThread Finished.");
    delete[] packet;
    
    return THREAD_EXIT;
}
