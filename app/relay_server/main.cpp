#include "relay_server.h"
#include <sys/stat.h>
#include "registry.h"

int main(int argc, char** argv) {
    if(argc != 4) {
        printf("Usage : %s <relay_server_port> <rendezvous_server_ip> <rendezvous_server_port>\n", argv[0]);
        exit(-1);
    }
    
    ijoon::initGlobalVariables();
    
    ijoon::RelayServer server(atoi(argv[1]), argv[2], argv[3]);
    server.socket->option(ijoon::SOCK_REUSE, 1);
    server.start();
    getchar();
    
    return 0;
}

