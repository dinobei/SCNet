#include "rendezvous_client.h"
#include "rendezvous_message.h"
#include <sys/stat.h>
#include "registry.h"
#include <map>
#include <string>
#include <cstring>

#include "packet.pb.h"
#include "packet_type.pb.h"

using namespace std;
using namespace example;

void onServerConnecting();
void onServerConnectFailed();
void onServerConnected(std::string extIP, std::string extPort);
void onServerDisconnected();

void onConnecting(int connectionID);
void onConnected(std::shared_ptr<ijoon::KcpPeer> kcpPeer);
void onConnectFailed(std::string ip, std::string port);
void onDisconnected(std::shared_ptr<ijoon::KcpPeer> kcpPeer);

void onReceivedPacket0(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onReceivedPacket1(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onPacket1(std::shared_ptr<ijoon::KcpPeer> kcpPeer, Packet1 *pkt1);
void onPacket2(std::shared_ptr<ijoon::KcpPeer> kcpPeer, Packet2 *pkt2);

void onCameraListResponse(std::shared_ptr<ijoon::KcpPeer> kcpPeer, CameraListResponse *response);

int main(int argc, char** argv) {
    if(argc != 3) {
        printf("Usage : %s <rendezvous_server_ip> <rendezvous_server_port>\n", argv[0]);
        exit(-1);
    }
    
    SCNET_RAW_UDP_MESSAGE_REGISTRATION(0, onReceivedPacket0);
    SCNET_RAW_UDP_MESSAGE_REGISTRATION(1, onReceivedPacket1);
    SCNET_PROTOBUF_UDP_MESSAGE_REGISTRATION(packetType1, Packet1, onPacket1);
    SCNET_PROTOBUF_UDP_MESSAGE_REGISTRATION(packetType2, Packet2, onPacket2);
    SCNET_PROTOBUF_UDP_MESSAGE_REGISTRATION(cameraListRequest, CameraListRequest, nullptr);
    SCNET_PROTOBUF_UDP_MESSAGE_REGISTRATION(cameraListResponse, CameraListResponse, onCameraListResponse);
    
    ijoon::RendezvousClient::init(argv[1], argv[2], "SeRiAl", "12:34:56:78:9a:bc", "1.0.0");
    ijoon::RendezvousClient& client = ijoon::RendezvousClient::getInstance();
    client.callback.onServerConnecting = onServerConnecting;
    client.callback.onServerConnected = onServerConnected;
    client.callback.onServerDisconnected = onServerDisconnected;
    
    client.callback.onConnecting = onConnecting;
    client.callback.onConnected = onConnected;
    client.callback.onConnectFailed = onConnectFailed;
    client.callback.onDisconnected = onDisconnected;
    client.start();
    
    while(true) {
        char sendBuf[2000] = {0,};
        printf("Put full command (type HELP to help message): ");
        std::cin.getline(sendBuf, sizeof(sendBuf));
        const int sendBytes = strlen(sendBuf);
        if(sendBytes == 0) continue;
        
        char seperator = ' ';
        std::vector<std::string> vec;
        char *token = std::strtok((char *)&sendBuf, &seperator);
        while (token != NULL) {
            vec.emplace_back(token);
            token = std::strtok(nullptr, &seperator);
        }
        
        std::transform(vec[0].begin(), vec[0].end(), vec[0].begin(),
                       [](unsigned char c){ return std::toupper(c); });
        
        if(vec[0].compare("CONN") == 0) {
            if(vec.size() != 3) {
                ijn_print(DP_ERROR, "invalid parameter: CONN [TARGET_IP] [TARGET_PORT]");
                continue;
            }

            try {
                client.connect(vec[1], vec[2]);
            }
            catch(std::string msg) {
                ijn_print(DP_INFO, "%s", msg.c_str());
            }
        }
        else if(vec[0] == "SEND") {
            ijn_print(DP_INFO, "SEND PACKET: %s", sendBuf);
            if(vec.size() != 4) {
                ijn_print(DP_ERROR, "invalid parameter: CONNECTION_ID, PACKET_TYPE, YOUR_MESSAGE");
                continue;
            }
            uint connectionID = atoi(vec[1].c_str());
            int packetType = atoi(vec[2].c_str());
            std::string message = vec[3];
            
            auto kcpPeer = client.getKcpPeer(connectionID);
            if(kcpPeer == nullptr) {
                ijn_print(DP_ERROR, "invalid connectionID");
                continue;
            }
            
            kcpPeer->send(packetType, (char *)message.c_str(), message.length());
        }
        else if(vec[0].compare("SENDPB1") == 0) {
            ijn_print(DP_INFO, "SEND PROTOBUF PACKET: %s", sendBuf);
            if(vec.size() != 3) {
                ijn_print(DP_ERROR, "invalid parameter: CONNECTION_ID, INTEGER_VALUE");
                continue;
            }
            uint connectionID = atoi(vec[1].c_str());
            int value = atoi(vec[2].c_str());
            
            auto kcpPeer = client.getKcpPeer(connectionID);
            if(kcpPeer == nullptr) {
                ijn_print(DP_ERROR, "invalid connectionID");
                continue;
            }
            
            auto pkt1 = std::shared_ptr<Packet1>(new Packet1());
            pkt1->set_number(value);
            kcpPeer->send(pkt1);
        }
        else if(vec[0].compare("SENDPB2") == 0) {
            ijn_print(DP_INFO, "SEND PROTOBUF PACKET: %s", sendBuf);
            if(vec.size() != 3) {
                ijn_print(DP_ERROR, "invalid parameter: CONNECTION_ID, STRING_VALUE");
                continue;
            }
            uint connectionID = atoi(vec[1].c_str());
            std::string value = vec[2];
            
            auto kcpPeer = client.getKcpPeer(connectionID);
            if(kcpPeer == nullptr) {
                ijn_print(DP_ERROR, "invalid connectionID");
                continue;
            }

            auto pkt2 = std::shared_ptr<Packet2>(new Packet2());
            pkt2->set_str(value);
            kcpPeer->send(pkt2);
        }
        else if(vec[0].compare("LIST") == 0) {
            auto serverKcpPeer = client.getKcpPeer(ijoon::Peer(client.serverIP, client.serverPort));
            serverKcpPeer->send(std::shared_ptr<CameraListRequest>(new CameraListRequest()));
        }
        else if(vec[0].compare("HELP") == 0) {
            printf("command type 1: CONN (send CONNECTION_REQUEST)\n");
            printf("CONN [TARGET_PEER_IP] [TARGET_PEER_PORT]\n");
            printf("example) CONN 127.0.0.1 11111\n");
            
            printf("command type 2: SEND (Packet send using uniqueID)\n");
            printf("SEND [CONNECTION_ID] [PACKET_TYPE] [YOUR_MESSAGE]\n");
            printf("example) SEND 1 0 helloworld\n");
            
            printf("command type 3: SENDPB1 (Protobuf Packet1 send using uniqueID)\n");
            printf("SENDPB1 [CONNECTION_ID] [INTEGER_VALUE]\n");
            printf("example) SENDPB1 1 123123\n");
            
            printf("command type 4: SENDPB2 (Protobuf Packet2 send using uniqueID)\n");
            printf("SENDPB2 [CONNECTION_ID] [STRING_VALUE]\n");
            printf("example) SENDPB2 1 helloworld\n");
            
            printf("command type 5: get_camera_list (Protobuf CameraListRequest send to RendezvousServer)\n");
            printf("get_camera_list\n");
            printf("example) get_camera_list\n");
        }
        else if(vec[0].compare("STATUS") == 0) {
            printf("\n");
            for(auto iter = client.kcpPeerMap.begin() ; iter != client.kcpPeerMap.end() ; ++iter) {
                ijn_print(DP_ERROR, "%s", iter->first.c_str());
            }
            ijn_print(DP_ERROR, "------------");
            for(auto iter = client.connFilterMap.begin() ; iter != client.connFilterMap.end() ; ++iter) {
                ijn_print(DP_ERROR, "%d, %s", iter->first, iter->second.getKey().c_str());
            }
            printf("\n");
        }
        else if(vec[0].compare("q") == 0 || vec[0].compare("Q") == 0) {
            break;
        }
        else {
            ijn_print(DP_ERROR, "invalid command: \"CONN\" or \"SEND\" or \"SENDPB1\" or \"SENDPB2\" or \"LIST\"");
        }

        
    }
    
    
    getchar();
    return 0;
}

void onServerConnecting() {
    ijn_print(DP_DEBUG, "onServerConnecting...");
}

void onServerConnectFailed() {
    ijn_print(DP_DEBUG, "onServerConnectFailed...");
}

void onServerConnected(std::string extIP, std::string extPort) {
    ijn_print(DP_DEBUG, "onServerConnected... %s:%s", extIP.c_str(), extPort.c_str());
}

void onServerDisconnected() {
    ijn_print(DP_DEBUG, "onServerDisconnected...");
}

void onConnecting(int connectionID) {
    printf("onConnecting called, connectionID=%u\n", connectionID);
}
void onConnected(std::shared_ptr<ijoon::KcpPeer> kcpPeer) {
    printf("onConnected called, connectionID=%u\n", kcpPeer->connectionID);
}
void onConnectFailed(std::string ip, std::string port) {
    printf("onConnectFailed called, %s:%s\n", ip.c_str(), port.c_str());
}
void onDisconnected(std::shared_ptr<ijoon::KcpPeer> kcpPeer) {
    printf("onDisconnected called, connectionID=%u, address=%s\n", kcpPeer->connectionID, kcpPeer->getPeer().getKey().c_str());
}

void onReceivedPacket0(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
    if(length > 100) {
        printf("onReceivedPacket0 called, connectionID=%u, length=%d, buffer=%c%c%c...\n", kcpPeer->connectionID, length, ((char *)buffer)[0], ((char *)buffer)[1], ((char *)buffer)[2]);
    }
    else {
        printf("onReceivedPacket0 called, connectionID=%u, length=%d, buffer=%s\n", kcpPeer->connectionID, length, buffer);
    }
}
void onReceivedPacket1(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length) {
    if(length > 100) {
        printf("onReceivedPacket1 called, connectionID=%u, length=%d, buffer=%c%c%c...\n", kcpPeer->connectionID, length, ((char *)buffer)[0], ((char *)buffer)[1], ((char *)buffer)[2]);
    }
    else {
        printf("onReceivedPacket1 called, connectionID=%u, length=%d, buffer=%s\n", kcpPeer->connectionID, length, buffer);
    }
}
void onPacket1(std::shared_ptr<ijoon::KcpPeer> kcpPeer, Packet1 *pkt1) {
    printf("onPacket1 called, connectionID=%u, number=%d\n", kcpPeer->connectionID, pkt1->number());
}
void onPacket2(std::shared_ptr<ijoon::KcpPeer> kcpPeer, Packet2 *pkt2) {
    printf("onPacket2 called, connectionID=%u, str=%s\n", kcpPeer->connectionID, pkt2->str().c_str());
}

void onCameraListResponse(std::shared_ptr<ijoon::KcpPeer> kcpPeer, CameraListResponse *response) {
    for(int i = 0 ; i < response->cameralist_size() ; i++) {
        ijn_print(DP_INFO, "%d) [name=%s, serial=%s] %s:%s %s:%s %u",
                  i+1,
                  response->cameralist(i).name().c_str(),
                  response->cameralist(i).serial().c_str(),
                  response->cameralist(i).publicip().c_str(),
                  response->cameralist(i).publicport().c_str(),
                  response->cameralist(i).privateip().c_str(),
                  response->cameralist(i).privateport().c_str(),
                  response->cameralist(i).ping()
                  );
    }
}
