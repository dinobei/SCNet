#include "embodied_server.h"

void ijoon::EmbodiedServer::onClientServiceCallback(BaseSession *session, google::protobuf::Message *message) {
    google::protobuf::uint32 type = BaseMessageRegistry->GetType(message->GetTypeName());
    EmbodiedSession *eSess = static_cast<ijoon::EmbodiedSession*>(session);
    
    ijn_print(DP_INFO, "client's identifier is %d", eSess->getIdentifier());
    
    switch(type) {
        case simple::PacketType::packetType1:
        {
            simple::packet_1 *pkt1 = static_cast<simple::packet_1*>(message);
            ijn_print(DP_DEBUG, "EmbodiedServer's callback() called (packetType1), number=%d", pkt1->number());
            eSess->send(pkt1);
        }
            break;
        case simple::PacketType::packetType2:
        {
            simple::packet_2 *pkt2 = static_cast<simple::packet_2*>(message);
            ijn_print(DP_DEBUG, "EmbodiedServer's callback() called (packetType2), number=%d", pkt2->number());
            eSess->send(pkt2);
        }
            break;
        case simple::PacketType::packetType3:
        {
            simple::packet_3 *pkt3 = static_cast<simple::packet_3*>(message);
            ijn_print(DP_DEBUG, "EmbodiedServer's callback() called (packetType3), number=%d", pkt3->number());
            eSess->send(pkt3);
        }
            break;
        case simple::PacketType::packetType4:
        {
            simple::packet_4 *pkt4 = static_cast<simple::packet_4*>(message);
            ijn_print(DP_DEBUG, "EmbodiedServer's callback() called (packetType4), number=%d", pkt4->number());
            eSess->send(pkt4);
        }
            break;
        default:
            ijn_print(DP_INFO, "Unprocessed type of message.");
            break;
    }
}

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
    
    return new ijoon::EmbodiedSession(clntSocket, clientId);
}

