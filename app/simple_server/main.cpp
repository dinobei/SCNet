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

std::shared_ptr<ijoon::Session> getClientSession(std::shared_ptr<ijoon::TCPSocket> socket);
void onClientConnected(std::shared_ptr<ijoon::Session> session);
void onClientTimeout(std::shared_ptr<ijoon::Session> session);
void onClientDisconnected(std::shared_ptr<ijoon::Session> session);

void onPacket1(std::shared_ptr<ijoon::Session> session, Packet1 *pkt1);
void onPacket2(std::shared_ptr<ijoon::Session> session, Packet2 *pkt2);
void onPacket3(std::shared_ptr<ijoon::Session> session, Packet3 *pkt3);
void onPacket4(std::shared_ptr<ijoon::Session> session, Packet4 *pkt4);
void onArrayMessage(std::shared_ptr<ijoon::Session> session, ArrayMessage *arrayMessage);
long GetFileSize(std::string filename);
void onImageRequest(std::shared_ptr<ijoon::Session> session, ImageRequest *imageRequest);

void onRawByteArray(std::shared_ptr<ijoon::Session> session, void *buffer, unsigned int length);
void onRawByteArray2(std::shared_ptr<ijoon::Session> session, void *buffer, unsigned int length);

int main(int argv, char** argc) {
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(packetType1, Packet1, onPacket1);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(packetType2, Packet2, onPacket2);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(packetType3, Packet3, onPacket3);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(packetType4, Packet4, onPacket4);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(arrayMessageType, ArrayMessage, onArrayMessage);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(imageRequest, ImageRequest, onImageRequest);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(imageResponse, ImageResponse, nullptr);
    SCNET_RAW_MESSAGE_REGISTRATION(0, onRawByteArray);
    SCNET_RAW_MESSAGE_REGISTRATION(1, onRawByteArray2);

    ijoon::ClientManager clientManager(9190, 30 * 1000, false);
    clientManager.onServerStarted = onServerStarted;
    clientManager.onServerStopped = onServerStopped;
    clientManager.getClientSession = getClientSession;
    clientManager.onClientConnected = onClientConnected;
    clientManager.onClientTimeout = onClientTimeout;
    clientManager.onClientDisconnected = onClientDisconnected;
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

std::shared_ptr<ijoon::Session> getClientSession(std::shared_ptr<ijoon::TCPSocket> socket) {
    socket->option(ijoon::SOCK_RCVTIMEO_MS, 500);
    socket->option(ijoon::SOCK_SNDTIMEO_MS, 500);
    return std::make_shared<ijoon::Session>(socket);
}

void onClientConnected(std::shared_ptr<ijoon::Session> session) {
    ijn_print(DP_INFO, "[onClientConnected(std::shared_ptr<ijoon::Session>)] %d", session->getClientSocket()->getSocketIdentifier());
}

void onClientTimeout(std::shared_ptr<ijoon::Session> session) {
    if(session == nullptr) {
        ijn_print(DP_INFO, "[onClientTimeout(std::shared_ptr<ijoon::Session>)]");
    }
    else {
        ijn_print(DP_INFO, "[onClientTimeout(std::shared_ptr<ijoon::Session>)] %d", session->getClientSocket()->getSocketIdentifier());
    }
}

void onClientDisconnected(std::shared_ptr<ijoon::Session> session) {
    ijn_print(DP_INFO, "[onClientDisconnected(std::shared_ptr<ijoon::Session>)] %d", session->getClientSocket()->getSocketIdentifier());
}

void onPacket1(std::shared_ptr<ijoon::Session> session, Packet1 *pkt1) {
    ijn_print(DP_DEBUG, "[onPacket1()] number=%d", pkt1->number());
    session->send(pkt1);
}

void onPacket2(std::shared_ptr<ijoon::Session> session, Packet2 *pkt2) {
    ijn_print(DP_DEBUG, "[onPacket2()] str=%s", pkt2->str().c_str());
    session->send(pkt2);
}

void onPacket3(std::shared_ptr<ijoon::Session> session, Packet3 *pkt3) {
    ijn_print(DP_DEBUG, "[onPacket3()] boolvalue=%s", pkt3->boolvalue()?"true":"false");
    session->send(pkt3);
}

void onPacket4(std::shared_ptr<ijoon::Session> session, Packet4 *pkt4) {
    ijn_print(DP_DEBUG, "[onPacket4()] floatvalue=%f, doublevalue=%lf", pkt4->floatvalue(), pkt4->doublevalue());
    session->send(pkt4);
}

void onArrayMessage(std::shared_ptr<ijoon::Session> session, ArrayMessage *arrayMessage) {
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

void onImageRequest(std::shared_ptr<ijoon::Session> session, ImageRequest *imageRequest) {
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

void onRawByteArray(std::shared_ptr<ijoon::Session> session, void *buffer, unsigned int length)
{
    char *message = nullptr;
    if(buffer != nullptr) {
        message = static_cast<char *>(buffer);
    }
    
    message[length] = '\0';
    ijn_print(DP_DEBUG, "onRawByteArray, length: %u %s", length, message);
    session->send(0, (char *)buffer, length);
}

void onRawByteArray2(std::shared_ptr<ijoon::Session> session, void *buffer, unsigned int length)
{
    char *message = nullptr;
    if(buffer != nullptr) {
        message = static_cast<char *>(buffer);
    }
    
    message[length] = '\0';
    ijn_print(DP_DEBUG, "onRawByteArray2, length: %u %s", length, message);
    session->send(1, (char *)buffer, length);
}
