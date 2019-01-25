#include "randezvous_client.h"
#include "randezvous_message.h"
#include <sys/stat.h>
#include "registry.h"
#include <map>
#include <string>
#include <cstring>

using namespace std;

void onConnected(ijoon::RandezvousSession *session, void *buffer, unsigned int length);

int main(int argc, char** argv) {
    if(argc != 3) {
        printf("Usage : %s <randezvous_server_ip> <randezvous_server_port>\n", argv[0]);
        exit(-1);
    }
    
    ijoon::initGlobalVariables();
    SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::CONNECTION_RELAY_SERVICE_SUCCESS, onConnected);
    
    ijoon::RandezvousClient client(argv[1], argv[2]);
    client.start();
    
    while(true) {
        char sendBuf[255] = {0,};
        printf("Put full command (type HELP to help message): ");
        std::cin.getline(sendBuf, sizeof(sendBuf));
        const int sendBytes = strlen(sendBuf);
        if(sendBytes == 0) continue;
        
        char seperator = ' ';
        std::vector<std::string> vec;
        char *token = std::strtok((char *)&sendBuf, &seperator);
        while (token != NULL) {
            vec.push_back(token);
            token = std::strtok(NULL, &seperator);
        }
        if(vec[0].compare("CONN") == 0) {
            if(vec.size() != 3) {
                ijn_print(DP_ERROR, "invalid parameter: MESSAGE");
                continue;
            }
            
            std::string targetAddress;
            targetAddress = vec[1];
            targetAddress += seperator;
            targetAddress += vec[2];
            ijn_print(DP_INFO, "CONNECTION_REQUEST: %s", sendBuf);
            ijoon::send(client.socket, client.randezvousServerPeer, 0, ijoon::CONNECTION_REQUEST, (char *)targetAddress.c_str(), targetAddress.length());
        }
        else if(vec[0].compare("RELAY") == 0) {
            ijn_print(DP_INFO, "SEND RELAY PACKET: %s", sendBuf);
            if(vec.size() != 3) {
                ijn_print(DP_ERROR, "invalid parameter: RELAY_SERVER_IP, RELAY_SERVER_PORT, CONNECTION_ID, YOUR_MESSAGE");
                continue;
            }
            uint connectionID = atoi(vec[1].c_str());

            std::string message = vec[2];
            
            auto randezvousSession = client.randezvousSessionMap[connectionID];
            if(randezvousSession == nullptr) {
                ijn_print(DP_ERROR, "invalid connectionID");
                continue;
            }
            randezvousSession->send(123, (char *)message.c_str(), message.length());
        }
        else if(vec[0].compare("HELP") == 0) {
            printf("command type 1: CONN (send CONNECTION_REQUEST)\n");
            printf("CONN [TARGET_PEER_IP] [TARGET_PEER_PORT]\n");
            printf("example) CONN 127.0.0.1 11111\n");
            
            printf("command type 2: RELAY (Packet relay using uniqueID)\n");
            printf("RELAY [CONNECTION_ID] [YOUR_MESSAGE]\n");
            printf("example) RELAY 1 helloworld\n");
            
        }
        else {
            ijn_print(DP_ERROR, "invalid command: \"CONN\" or \"RELAY\"");
        }

        
    }
    
    
    getchar();
    return 0;
}

void onConnected(ijoon::RandezvousSession *session, void *buffer, unsigned int length)
{
    printf("onConnectionResponseSuccess called, connectionID=%u\n", session->getConnectionID());
}
