#include "rendezvous_client.h"
#include "rendezvous_message.h"
#include "message_header.h"
#include "session.h"
#include <ifaddrs.h>
#include <cstring>
#include "registry.h"

extern char seperator;

std::string getIPAddress()
{
    std::string ipAddress="0.0.0.0";
    struct ifaddrs *interfaces = NULL;
    struct ifaddrs *temp_addr = NULL;
    int success = 0;
    // retrieve the current interfaces - returns 0 on success
    success = getifaddrs(&interfaces);
    if (success == 0) {
        // Loop through linked list of interfaces
        temp_addr = interfaces;
        while(temp_addr != NULL) {
            if(temp_addr->ifa_addr->sa_family == AF_INET) {
                // Check if interface is en0 which is the wifi connection on the iPhone
                if(strcmp(temp_addr->ifa_name, "en0")==0){
                    ipAddress=inet_ntoa(((struct sockaddr_in*)temp_addr->ifa_addr)->sin_addr);
                }
            }
            temp_addr = temp_addr->ifa_next;
        }
    }
    // Free memory
    freeifaddrs(interfaces);
    return ipAddress;
}

void ijoon::RendezvousClient::start() {
    ijn_print(DP_INFO, "Rendezvous client start...");
    register_thread = new ijoon::Thread(registerThread, "register_thread");
    register_thread->start(this);
    recv_thread = new ijoon::Thread(recvThread, "recv_thread");
    recv_thread->start(this);
}

ijoon::THREAD_RET THREAD_API ijoon::registerThread(void *arg) {
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::RendezvousClient *client = (ijoon::RendezvousClient *)thread->getParam();

    std::string localIP = getIPAddress();
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
    
    std::string data = localIP + " " + std::to_string(localPort);
    
    while(!thread->isInterrupted()) {
        ijn_print(DP_INFO, "send REGISTRATION_RENDEZVOUS_CLIENT_REQUEST");
        ijoon::send(client->socket, client->rendezvousServerPeer, 0, REGISTRATION_RENDEZVOUS_CLIENT_REQUEST, (char *)data.c_str(), data.length());
        thread->sleep(30 * 1000);
    }
    
    return THREAD_EXIT;
}

ijoon::THREAD_RET THREAD_API ijoon::recvThread(void *arg) {
    ijoon::Thread *thread = (ijoon::Thread *)arg;
    ijoon::RendezvousClient *client = (ijoon::RendezvousClient *)thread->getParam();
    
    char *packet = new char[MAX_PACKET_SIZE];
    ijoon::Peer peer;
    
    while(!thread->isInterrupted()) {
        memset(packet, 0, MAX_PACKET_SIZE);
        ssize_t recvSize = client->socket->recvFrom(&peer, packet, MAX_PACKET_SIZE);
        if(recvSize < 0) {
            continue;
        }
        
        MessageHeader messageHeader;
        int cursor = 0;
        if(!ijoon::readHeader(packet, recvSize, messageHeader, cursor)) {
            continue;
        }
        
        if((recvSize-cursor) != messageHeader.dataSize) {
            continue;
        }
        
        char *body = &packet[cursor];
        
        switch (messageHeader.packetType) {
            case 123:
                ijn_print(DP_INFO, "packetType 123, body=%s", body);
                continue;
            case 1:
                printf("packetType 1 received\n");
                continue;
            default:
                break;
        }
        
        if(messageHeader.messageType != MESSAGE_TYPE::RAWBYTE) {
            continue;
        }
        
        switch (messageHeader.packetType) {
            case REGISTRATION_RENDEZVOUS_CLIENT_SUCCESS:
            {
                ijn_print(DP_DEBUG, "received REGISTRATION_RENDEZVOUS_CLIENT_SUCCESS");
                
                std::vector<std::string> vec;
                char *token = std::strtok((char *)body, &seperator);
                while (token != NULL) {
                    vec.push_back(token);
                    token = std::strtok(NULL, &seperator);
                }
                if(vec.size() != 2) {
                    ijn_print(DP_ERROR, "[REGISTRATION_RENDEZVOUS_CLIENT_SUCCESS] invalid parameters");
                    break;
                }
                
                ijn_print(DP_INFO, "[REGISTRATION_RENDEZVOUS_CLIENT_SUCCESS] MyPublicAddress=%s:%s", vec[0].c_str(), vec[1].c_str());
                
                continue;
            }
            case CONNECTION_FAILED:
            {
                ijn_print(DP_DEBUG, "received CONNECTION_FAILED");
                continue;
            }
            case DIRECT_CONNECTION_AVAILABLE: // SP only
            {
                ijn_print(DP_DEBUG, "received DIRECT_CONNECTION_AVAILABLE");
                
                std::vector<std::string> vec;
                char *token = std::strtok(body, &seperator);
                while (token != NULL) {
                    vec.push_back(token);
                    token = std::strtok(NULL, &seperator);
                }
                if(vec.size() != 2) {
                    ijn_print(DP_ERROR, "[DIRECT_CONNECTION_AVAILABLE] invalid parameters");
                    break;
                }
                
                ijoon::Peer targetPeer(vec[0], vec[1]);
                ijoon::send(client->socket, targetPeer, messageHeader.connectionID, DIRECT_CONNECTION_REQUEST);
                continue;
            }
            case DIRECT_CONNECTION_REQUEST: // TP only
            {
                ijn_print(DP_DEBUG, "received DIRECT_CONNECTION_REQUEST");
                
                ijoon::send(client->socket, peer, messageHeader.connectionID, DIRECT_CONNECTION_RESPONSE);
                continue;
            }
            case DIRECT_CONNECTION_RESPONSE: // SP only
            {
                ijn_print(DP_DEBUG, "received DIRECT_CONNECTION_RESPONSE");
                
                auto rendezvousSession = client->rendezvousSessionMap[messageHeader.connectionID];
                rendezvousSession->setPublicPeer(peer.getIP(), std::to_string(peer.getPort()));
                auto callbackWrapper = BaseMessageRegistry->GetCallbackWrapper(messageHeader.messageType, messageHeader.packetType);
                if(callbackWrapper != nullptr) {
                    callbackWrapper->callback(rendezvousSession.get(), nullptr, 0);
                }
                
                continue;
            }
            case REVERSE_CONNECTION: // TP only
            {
                ijn_print(DP_DEBUG, "received REVERSE_CONNECTION");
                
                std::vector<std::string> vec;
                char *token = std::strtok(body, &seperator);
                while (token != NULL) {
                    vec.push_back(token);
                    token = std::strtok(NULL, &seperator);
                }
                if(vec.size() != 2) {
                    ijn_print(DP_ERROR, "[REVERSE_CONNECTION] invalid parameters");
                    break;
                }
                
                ijoon::Peer spPeer(vec[0], vec[1]);
                ijoon::send(client->socket, spPeer, messageHeader.connectionID, REVERSE_CONNECTION_REQUEST);
                
                continue;
            }
            case REVERSE_CONNECTION_REQUEST: // SP only
            {
                ijn_print(DP_DEBUG, "received REVERSE_CONNECTION_REQUEST");
                
                ijoon::send(client->socket, peer, messageHeader.connectionID, REVERSE_CONNECTION_RESPONSE);
                
                auto rendezvousSession = client->rendezvousSessionMap[messageHeader.connectionID];
                rendezvousSession->setPublicPeer(peer.getIP(), std::to_string(peer.getPort()));
                
                continue;
            }
            case REVERSE_CONNECTION_RESPONSE: // TP only
            {
                ijn_print(DP_DEBUG, "received REVERSE_CONNECTION_RESPONSE");
                
                auto rendezvousSession = client->rendezvousSessionMap[messageHeader.connectionID];
                rendezvousSession->setPublicPeer(peer.getIP(), std::to_string(peer.getPort()));
                
                continue;
            }
            
            case UDP_HOLE_PUNCHING_AVAILABLE:
            {
                ijn_print(DP_DEBUG, "received UDP_HOLE_PUNCHING_AVAILABLE");

                std::vector<std::string> vec;
                char *token = std::strtok(body, &seperator);
                while (token != NULL) {
                    vec.push_back(token);
                    token = std::strtok(NULL, &seperator);
                }
                if(vec.size() != 4) {
                    ijn_print(DP_ERROR, "[UDP_HOLE_PUNCHING_AVAILABLE] invalid parameters");
                    break;
                }
                    
                // private, public 정보로 동시에 연결 요청 메시지를 보냄
                std::string data;
                
                ijoon::Peer publicPeer(vec[0], vec[1]);
                data = "1";
                ijoon::send(client->socket, publicPeer, messageHeader.connectionID, UDP_HOLE_PUNCHING_REQUEST, (char *)data.c_str(), data.length());
                
                ijoon::Peer privatePeer(vec[2], vec[3]);
                data = "0";
                ijoon::send(client->socket, privatePeer, messageHeader.connectionID, UDP_HOLE_PUNCHING_REQUEST, (char *)data.c_str(), data.length());
                
                continue;
            }
            case UDP_HOLE_PUNCHING_REQUEST:
            {
                ijn_print(DP_DEBUG, "received UDP_HOLE_PUNCHING_REQUEST");
                
                std::vector<std::string> vec;
                char *token = std::strtok(body, &seperator);
                while (token != NULL) {
                    vec.push_back(token);
                    token = std::strtok(NULL, &seperator);
                }
                if(vec.size() != 1) {
                    ijn_print(DP_ERROR, "[UDP_HOLE_PUNCHING_REQUEST] invalid parameters");
                    break;
                }
                
                std::string data = vec[0];
                ijoon::send(client->socket, peer, messageHeader.connectionID, UDP_HOLE_PUNCHING_RESPONSE, (char *)data.c_str(), data.length());
                continue;
            }
            case UDP_HOLE_PUNCHING_RESPONSE:
            {
                ijn_print(DP_DEBUG, "received UDP_HOLE_PUNCHING_RESPONSE");
                
                // Connected by hole punching or local

                std::vector<std::string> vec;
                char *token = std::strtok(body, &seperator);
                while (token != NULL) {
                    vec.push_back(token);
                    token = std::strtok(NULL, &seperator);
                }
                if(vec.size() != 1) {
                    ijn_print(DP_ERROR, "[UDP_HOLE_PUNCHING_RESPONSE] invalid parameters");
                    break;
                }
                
                const bool isPublic = atoi(vec[0].c_str()) ? true : false;
                
                ijn_print(DP_ERROR, "peer.getIP()=%s, peer.getPort()=%d", peer.getIP().c_str(), peer.getPort());
                auto rendezvousSession = client->rendezvousSessionMap[messageHeader.connectionID];
                if(isPublic) {
                    // public connection (common connection)
                    rendezvousSession->setPublicPeer(peer.getIP(), std::to_string(peer.getPort()));
                    
                    ijn_print(DP_DEBUG, "[UDP_HOLE_PUNCHING_RESPONSE] connected public, from %s:%d", peer.getIP().c_str(), peer.getPort());
                }
                else {
                    // private connection (equal net, or hole punching)
                    rendezvousSession->setPrivatePeer(peer.getIP(), std::to_string(peer.getPort()));
                    
                    ijn_print(DP_DEBUG, "[UDP_HOLE_PUNCHING_RESPONSE] connected private, from %s:%d", peer.getIP().c_str(), peer.getPort());
                }
                
                continue;
            }
            case RELAY_SERVER_INFORMATION:
            {
                ijn_print(DP_DEBUG, "received RELAY_SERVER_INFORMATION, connectionID=%d", messageHeader.connectionID);
                
                std::vector<std::string> vec;
                char *token = std::strtok(body, &seperator);
                while (token != NULL) {
                    vec.push_back(token);
                    token = std::strtok(NULL, &seperator);
                }
                if(vec.size() != 2) {
                    ijn_print(DP_ERROR, "[RELAY_SERVER_INFORMATION] invalid parameters");
                    break;
                }
                
                // regist connection ID
                std::shared_ptr<ijoon::RendezvousSession> rendezvousSession = std::shared_ptr<ijoon::RendezvousSession>(new ijoon::RendezvousSession(client->socket, messageHeader.connectionID));
                client->rendezvousSessionMap[messageHeader.connectionID] = rendezvousSession;
                
                ijn_print(DP_INFO, "[RELAY_SERVER_INFORMATION] RelayServerAddress=%s:%s", vec[0].c_str(), vec[1].c_str());
                
                ijoon::Peer relayPeer(vec[0], vec[1]);
                ijoon::send(client->socket, relayPeer, messageHeader.connectionID, REGISTRATION_RELAY_PEER_REQUEST);
                
                continue;
            }
            case CONNECTION_RELAY_SERVICE_SUCCESS:
            {
                ijn_print(DP_DEBUG, "received CONNECTION_RELAY_SERVICE_SUCCESS");
                
                std::vector<std::string> vec;
                char *token = std::strtok(body, &seperator);
                while (token != NULL) {
                    vec.push_back(token);
                    token = std::strtok(NULL, &seperator);
                }
                if(vec.size() != 2) {
                    ijn_print(DP_ERROR, "[CONNECTION_RELAY_SERVICE_SUCCESS] invalid parameters");
                    break;
                }
                
                auto rendezvousSession = client->rendezvousSessionMap[messageHeader.connectionID];
                rendezvousSession->setRelayPeer(vec[0], vec[1]);

                continue;
            }
            case REGISTRATION_RELAY_PEER_SUCCESS:
            {
                ijn_print(DP_DEBUG, "received REGISTRATION_RELAY_PEER_SUCCESS");
                
                continue;
            }
            case REGISTRATION_RELAY_PEER_FAILED:
            {
                ijn_print(DP_DEBUG, "received REGISTRATION_RELAY_PEER_FAILED");
                continue;
            }
            default:
            {
                ijn_print(DP_ERROR, "Undefined message received");
                break;
            }
        }
        
    }
    
    delete[] packet;
    
    return THREAD_EXIT;
}
