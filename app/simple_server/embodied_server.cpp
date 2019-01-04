#include "embodied_server.h"

void ijoon::EmbodiedServer::onServerStarted() {
    ijn_print(DP_INFO, "[LifeCycle] onServerStarted");
}

void ijoon::EmbodiedServer::onServerStopped() {
    ijn_print(DP_INFO, "[LifeCycle] onServerStopped");
}

void ijoon::EmbodiedServer::onClientServiceStarted(BaseSession *session) {
    ijn_print(DP_INFO, "[LifeCycle] onClientServiceStarted");
}
void ijoon::EmbodiedServer::onClientServiceTimeout(BaseSession *session) {
    ijn_print(DP_INFO, "[LifeCycle] onClientServiceTimeout");
}

void ijoon::EmbodiedServer::onClientServiceDisconnected(BaseSession *session) {
    ijn_print(DP_INFO, "[LifeCycle] onClientServiceDisconnected");
}

void ijoon::EmbodiedServer::onClientServiceStopped(BaseSession *session) {
    EmbodiedSession *eSession = static_cast<EmbodiedSession *>(session);
    
    ijn_print(DP_INFO, "[LifeCycle] onClientServiceStopped, identifier: %d", eSession->getIdentifier());
}

ijoon::BaseSession *ijoon::EmbodiedServer::getSession(std::shared_ptr<ijoon::JClientSocket> clntSocket) {
    // Derived class of BaseSession is available what you want.
    // Get identifier from database for example.
    int clientId = 777;
    clntSocket->option(SOCK_RCVTIMEO_MS, 500);
    clntSocket->option(SOCK_SNDTIMEO_MS, 500);
    return new ijoon::EmbodiedSession(clntSocket, clientId);
}

