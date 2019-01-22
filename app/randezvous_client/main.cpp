#include "randezvous_client.h"
#include "randezvous_message.h"
#include <sys/stat.h>
#include "registry.h"
#include <map>
#include <string>
#include <cstring>

using namespace std;

void onConnected(ijoon::RandezvousSession *session, void *buffer, unsigned int length);

int main(int argv, char** argc) {
//    std::multimap<int, int> map;
//    map.insert(std::pair<int, int>(0, 1));
//    map.insert(std::pair<int, int>(0, 2));
//    int count = map.count(0);
//    printf("count : %d\n", count);

//    multimap<int, int> mm;
//
//    mm.insert(pair<int, int>(5, 100));
//    mm.insert(pair<int, int>(3, 100));
//    mm.insert(pair<int, int>(8, 30));
//    mm.insert(pair<int, int>(3, 40));
//    mm.insert(pair<int, int>(1, 70));
//    mm.insert(pair<int, int>(2, 2222));
//    mm.insert(pair<int, int>(2, 222));
//    mm.insert(pair<int, int>(2, 22222));
//    mm.insert(pair<int, int>(7, 100));
//    mm.insert(pair<int, int>(3, 333));
//    mm.insert(pair<int, int>(8, 50));
//
//    multimap<int, int>::iterator iter;
//    for (iter = mm.begin(); iter != mm.end(); ++iter)
//        cout << "(" << iter->first << "," << iter->second << ") ";
//    cout << endl;
//
//    // multimap 의 key 3 원소의 개수 찾기
//    cout << "key 3의 원소의 개수는 : " << mm.count(3) << endl;
//
//    // multimap의 key 3의 위치 찾기
//    iter = mm.find(3);
//    if (iter != mm.end())
//        cout << "첫 번째 key 3에 매핑된 value : " << iter->second << endl;
//
//    map<int, int>::iterator lower_iter;
//    map<int, int>::iterator upper_iter;
//    lower_iter = mm.lower_bound(1);
//    upper_iter = mm.upper_bound(1);
//
//    for( ; lower_iter != mm.end() ; lower_iter++) {
//        cout << "lower_iter : " << "(" << lower_iter->first << "," << lower_iter->second << ") " << endl;
//    }
//    for( ; upper_iter != mm.end() ; upper_iter++) {
//        cout << "upper_iter : " << "(" << upper_iter->first << "," << upper_iter->second << ") " << endl;
//    }
//
//
//    // key가 3인 요소의 범위  찾기
//    pair<map<int, int>::iterator, map<int, int>::iterator> iter_pair;
//    iter_pair = mm.equal_range(3);
//
//    for (iter = iter_pair.first; iter != iter_pair.second; ++iter)
//        cout << "(" << iter->first << ',' << iter->second << ") ";
//    cout << endl;
//    
//    return 0;
    
//    char seperator = ' ';
//    char buffer[255] = "this is test";//{0,};
//    std::vector<std::string> vec;
//    char *token = std::strtok(&buffer[2], &seperator);
//    while (token != NULL) {
//        vec.push_back(token);
//        token = std::strtok(NULL, &seperator);
//    }
//    for(int i = 0 ; i < vec.size() ; i++) {
//        printf("%s ", vec[i].c_str());
//    }
//    printf("\n");
//    return 0;
    
    ijoon::initGlobalVariables();
    SCNET_RAW_UDP_MESSAGE_REGISTRATION(ijoon::CONNECTED, onConnected);
    
    ijoon::RandezvousClient client("127.0.0.1", "9190");
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
            randezvousSession->send(123, (char *)message.c_str(), message.length());
            
//            int cnt = 10;
//            while(cnt++ < 200) {
//                char buf[255] = {0,};
//                sprintf(buf, "%d", cnt);
//                randezvousSession->send(123, buf, strlen(buf));
//            }
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
