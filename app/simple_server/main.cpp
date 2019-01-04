#include "client_manager.h"
#include <sys/stat.h>
#include "registry.h"

/* implemented proto messages */
#include "packet.pb.h"
#include "packet_type.pb.h"
#include "get_image.pb.h"
using namespace example;


void onServerStarted();
void onServerStopped();

void onClientConnected(ijoon::Session *session);
void onClientServiceStarted(ijoon::Session *session);
void onClientServiceStopped(ijoon::Session *session);
void onClientServiceTimeout(ijoon::Session *session);
void onClientServiceDisconnected(ijoon::Session *session);

void onPacket1(ijoon::Session *session, Packet1 *pkt1);
void onPacket2(ijoon::Session *session, Packet2 *pkt2);
void onPacket3(ijoon::Session *session, Packet3 *pkt3);
void onPacket4(ijoon::Session *session, Packet4 *pkt4);
void onArrayMessage(ijoon::Session *session, ArrayMessage *arrayMessage);
long GetFileSize(std::string filename);
void onImageRequest(ijoon::Session *session, ImageRequest *imageRequest);

int main(int argv, char** argc) {
    ijoon::initGlobalVariables();
    SCNET_MESSAGE_REGISTRATION_WITH_RECV_CALLBACK(example, example::PacketType::packetType1, Packet1, ijoon::Session, onPacket1);
    SCNET_MESSAGE_REGISTRATION_WITH_RECV_CALLBACK(example, example::PacketType::packetType2, Packet2, ijoon::Session, onPacket2);
    SCNET_MESSAGE_REGISTRATION_WITH_RECV_CALLBACK(example, example::PacketType::packetType3, Packet3, ijoon::Session, onPacket3);
    SCNET_MESSAGE_REGISTRATION_WITH_RECV_CALLBACK(example, example::PacketType::packetType4, Packet4, ijoon::Session, onPacket4);
    SCNET_MESSAGE_REGISTRATION_WITH_RECV_CALLBACK(example, example::PacketType::arrayMessageType, ArrayMessage, ijoon::Session, onArrayMessage);
    SCNET_MESSAGE_REGISTRATION_WITH_RECV_CALLBACK(example, example::PacketType::imageRequest, ImageRequest, ijoon::Session, onImageRequest);
    SCNET_MESSAGE_REGISTRATION(example, example::PacketType::imageResponse, ImageResponse);

    ijoon::ClientManager clientManager(9190, 30 * 1000, false);
    clientManager.onServerStarted = onServerStarted;
    clientManager.onServerStopped = onServerStopped;
    clientManager.onClientConnected = onClientConnected;
    clientManager.onClientServiceStarted = onClientServiceStarted;
    clientManager.onClientServiceStopped = onClientServiceStopped;
    clientManager.onClientServiceTimeout = onClientServiceTimeout;
    clientManager.onClientServiceDisconnected = onClientServiceDisconnected;
    clientManager.start();
    getchar();
    
    clientManager.stop();
    getchar();
    
    return 0;
}

void onServerStarted() {
    ijn_print(DP_INFO, "[onServerStarted()]");
}

void onServerStopped() {
    ijn_print(DP_INFO, "[onServerStopped()]");
}

void onClientConnected(ijoon::Session *session) {
    session->getClientSocket()->option(ijoon::SOCK_RCVTIMEO_MS, 500);
    session->getClientSocket()->option(ijoon::SOCK_SNDTIMEO_MS, 500);
    ijn_print(DP_INFO, "[onClientConnected(ijoon::BaseSession *)] %d", session->getClientSocket()->getSocketIdentifier());
}

void onClientServiceStarted(ijoon::Session *session) {
    ijn_print(DP_INFO, "[onClientServiceStarted(ijoon::BaseSession *)] %d", session->getClientSocket()->getSocketIdentifier());
}

void onClientServiceStopped(ijoon::Session *session) {
    ijn_print(DP_INFO, "[onClientServiceStopped(ijoon::BaseSession *)] %d", session->getClientSocket()->getSocketIdentifier());
}

void onClientServiceTimeout(ijoon::Session *session) {
    if(session == nullptr) {
        ijn_print(DP_INFO, "[onClientServiceTimeout(ijoon::BaseSession *)]");
    }
    else {
        ijn_print(DP_INFO, "[onClientServiceTimeout(ijoon::BaseSession *)] %d", session->getClientSocket()->getSocketIdentifier());
    }
    
}

void onClientServiceDisconnected(ijoon::Session *session) {
    ijn_print(DP_INFO, "[onClientServiceDisconnected(ijoon::BaseSession *)] %d", session->getClientSocket()->getSocketIdentifier());
}

void onPacket1(ijoon::Session *session, Packet1 *pkt1) {
    ijn_print(DP_DEBUG, "[onPacket1()] number=%d", pkt1->number());
    session->send(pkt1);
}

void onPacket2(ijoon::Session *session, Packet2 *pkt2) {
    ijn_print(DP_DEBUG, "[onPacket2()] str=%s", pkt2->str().c_str());
    session->send(pkt2);
}

void onPacket3(ijoon::Session *session, Packet3 *pkt3) {
    ijn_print(DP_DEBUG, "[onPacket3()] boolvalue=%s", pkt3->boolvalue()?"true":"false");
    session->send(pkt3);
}

void onPacket4(ijoon::Session *session, Packet4 *pkt4) {
    ijn_print(DP_DEBUG, "[onPacket4()] floatvalue=%f, doublevalue=%lf", pkt4->floatvalue(), pkt4->doublevalue());
    session->send(pkt4);
}

void onArrayMessage(ijoon::Session *session, ArrayMessage *arrayMessage) {
    ijn_print(DP_INFO, "received array size: %d, message: ", arrayMessage->strarr_size());
    for(int i = 0 ; i < arrayMessage->strarr_size() ; i++) {
        printf("%s ", arrayMessage->strarr(i).c_str());
    }
    printf("\n");
    
    session->send(arrayMessage);
}

long GetFileSize(std::string filename)
{
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

void onImageRequest(ijoon::Session *session, ImageRequest *imageRequest) {
    int size = GetFileSize(imageRequest->name());
    ijn_print(DP_DEBUG, "requested image name: %s, size: %d", imageRequest->name().c_str(), size);
    
    if(size < 0) {
        return;
    }
    
    FILE *fp = fopen(imageRequest->name().c_str(), "rb");
    char *buf = new char[size];
    fread(buf, size, 1, fp);
    fclose(fp);
    
    example::ImageResponse *response = new example::ImageResponse();
    example::ImageHeader *imageHeader = response->mutable_header();
    imageHeader->set_width(1920);
    imageHeader->set_height(1080);
    imageHeader->set_name(imageRequest->name());
    imageHeader->set_size(size);
    
    response->set_imagebuffer(buf, size);
    
    bool ret = session->send(response);
    ijn_print(DP_DEBUG, "ret : %s", ret? "true" : "false");
    
    delete []buf;
    
}
