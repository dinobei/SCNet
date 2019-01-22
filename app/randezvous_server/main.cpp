#include "randezvous_server.h"
#include <sys/stat.h>
#include "registry.h"

int main(int argv, char** argc) {
    ijoon::initGlobalVariables();

    ijoon::RandezvousServer server(9190);
    server.start();
    getchar();
    
    return 0;
}
