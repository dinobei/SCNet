#include "randezvous_server.h"
#include <sys/stat.h>
#include "registry.h"

int main(int argc, char** argv) {
    if(argc != 2) {
        printf("Usage : %s <randezvous_server_port>\n", argv[0]);
        exit(-1);
    }
    
    ijoon::initGlobalVariables();

    ijoon::RandezvousServer server(atoi(argv[1]));
    server.start();
    getchar();
    
    return 0;
}
