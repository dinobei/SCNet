#include "embodied_server.h"

#include <sys/stat.h>

long GetFileSize(std::string filename)
{
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

void ijoon::EmbodiedServer::onClientServiceCallback(BaseSession *session, google::protobuf::Message *message) {
    google::protobuf::uint32 type = BaseMessageRegistry->GetType(message->GetTypeName());
    EmbodiedSession *eSess = static_cast<ijoon::EmbodiedSession*>(session);
    
    ijn_print(DP_INFO, "client's identifier is %d", eSess->getIdentifier());
    
    switch(type) {
        case example::PacketType::packetType1:
        {
            example::Packet1 *pkt1 = static_cast<example::Packet1*>(message);
            ijn_print(DP_DEBUG, "EmbodiedServer's callback() called (packetType1), number=%d", pkt1->number());
            eSess->send(pkt1);
        }
            break;
        case example::PacketType::packetType2:
        {
            example::Packet2 *pkt2 = static_cast<example::Packet2*>(message);
            ijn_print(DP_DEBUG, "EmbodiedServer's callback() called (packetType2), str=%s", pkt2->str().c_str());
            eSess->send(pkt2);
        }
            break;
        case example::PacketType::packetType3:
        {
            example::Packet3 *pkt3 = static_cast<example::Packet3*>(message);
            ijn_print(DP_DEBUG, "EmbodiedServer's callback() called (packetType3), boolValue=%s", pkt3->boolvalue() ? "true" : "false");
            eSess->send(pkt3);
        }
            break;
        case example::PacketType::packetType4:
        {
            example::Packet4 *pkt4 = static_cast<example::Packet4*>(message);
            ijn_print(DP_DEBUG, "EmbodiedServer's callback() called (packetType4), doubleValue=%lf, floatValue=%f", pkt4->doublevalue(), pkt4->floatvalue());
            eSess->send(pkt4);
        }
            break;
        case example::PacketType::imageRequest:
        {
            example::ImageRequest *request = static_cast<example::ImageRequest*>(message);
            
            

            int size = GetFileSize(request->name());
            ijn_print(DP_DEBUG, "requested image name: %s, size: %d", request->name().c_str(), size);
            
            if(size < 0) {
                break;
            }
            
            FILE *fp = fopen(request->name().c_str(), "rb");
            char *buf = new char[size];
            fread(buf, size, 1, fp);
            fclose(fp);
            
            example::ImageResponse *response = new example::ImageResponse();
            example::ImageHeader *imageHeader = response->mutable_header();
            imageHeader->set_width(1920);
            imageHeader->set_height(1080);
            imageHeader->set_name(request->name());
            imageHeader->set_size(size);
            
            response->set_imagebuffer(buf, size);
            
            bool ret = eSess->send(response);
            ijn_print(DP_DEBUG, "ret : %s", ret? "true" : "false");
            
            delete []buf;
        }
            break;
        case example::PacketType::arrayMessageType:
        {
            example::ArrayMessage *request = static_cast<example::ArrayMessage*>(message);
            eSess->send(request);
            
            ijn_print(DP_INFO, "received array size: %d, message: ", request->strarr_size());
            for(int i = 0 ; i < request->strarr_size() ; i++) {
                printf("%s ", request->strarr(i).c_str());
            }
            printf("\n");
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

