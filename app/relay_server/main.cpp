#include "relay_server.h"
#include <sys/stat.h>
#include "registry.h"

int main(int argv, char** argc) {
    ijoon::initGlobalVariables();
    
    ijoon::RelayServer server(9191, "127.0.0.1", "9190");
    server.socket->option(ijoon::SOCK_REUSE, 1);
    server.start();
    getchar();
    
    return 0;
}

