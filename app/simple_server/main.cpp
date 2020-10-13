#include "server.h"
#include <sys/stat.h>
#include "registry.h"

/* implemented proto messages */
#include "packet.pb.h"
#include "packet_type.pb.h"
#include "get_image.pb.h"
using namespace example;


void onServerStarted();
void onServerStopped();

std::shared_ptr<scnet::Session> getClientSession(std::shared_ptr<cppsocket::tcp_socket> socket);
void onClientConnected(std::shared_ptr<scnet::Session> session);
void onClientTimeout(std::shared_ptr<scnet::Session> session);
void onClientDisconnected(std::shared_ptr<scnet::Session> session);

void onPacket1(std::shared_ptr<scnet::Session> session, scnet::Header *header, Packet1 *pkt1);
void onPacket2(std::shared_ptr<scnet::Session> session, scnet::Header *header, Packet2 *pkt2);
void onPacket3(std::shared_ptr<scnet::Session> session, scnet::Header *header, Packet3 *pkt3);
void onPacket4(std::shared_ptr<scnet::Session> session, scnet::Header *header, Packet4 *pkt4);
void onArrayMessage(std::shared_ptr<scnet::Session> session, scnet::Header *header, ArrayMessage *arrayMessage);
long GetFileSize(std::string filename);
void onImageRequest(std::shared_ptr<scnet::Session> session, scnet::Header *header, ImageRequest *imageRequest);

int main(int argv, char** argc) {
    signal(SIGPIPE, SIG_IGN);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(packetType1, Packet1, onPacket1);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(packetType2, Packet2, onPacket2);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(packetType3, Packet3, onPacket3);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(packetType4, Packet4, onPacket4);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(arrayMessageType, ArrayMessage, onArrayMessage);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(imageRequest, ImageRequest, onImageRequest);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(imageResponse, ImageResponse, nullptr);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(9, scnet::Ping, nullptr);

    scnet::Server server(9190, 30 * 1000, true);
    server.onServerStarted = onServerStarted;
    server.onServerStopped = onServerStopped;
    server.getClientSession = getClientSession;
    server.onClientConnected = onClientConnected;
    server.onClientTimeout = onClientTimeout;
    server.onClientDisconnected = onClientDisconnected;
    server.start();
    getchar();
    
    server.stop();
    getchar();
    
    return 0;
}

void onServerStarted() {
    std::cout << "ServerStarted" << std::endl;
}

void onServerStopped() {
    std::cout << "ServerStopped" << std::endl;
}

std::shared_ptr<scnet::Session> getClientSession(std::shared_ptr<cppsocket::tcp_socket> socket) {
    int ms = 500;
    socket->option(cppsocket::SOCK_RCVTIMEO_MS, (void *)&ms);
    socket->option(cppsocket::SOCK_SNDTIMEO_MS, (void *)&ms);
    return std::make_shared<scnet::Session>(socket);
}

void onClientConnected(std::shared_ptr<scnet::Session> session) {
    std::cout << "ClientConnected, " << session->getClientSocket()->get_socket_identifier() << std::endl;
}

void onClientTimeout(std::shared_ptr<scnet::Session> session) {
    if(session == nullptr) {
        std::cout << "onClientTimeout" << std::endl;
    }
    else {
        std::cout << "ClientTimeout, " << session->getClientSocket()->get_socket_identifier() << std::endl;
    }
}

void onClientDisconnected(std::shared_ptr<scnet::Session> session) {
    std::cout << "ClientDisconnected, " << session->getClientSocket()->get_socket_identifier() << std::endl;
}

void onPacket1(std::shared_ptr<scnet::Session> session, scnet::Header *header, Packet1 *pkt1) {
    std::cout << "Packet1 received, number=" << pkt1->number() << std::endl;
    session->send(header, pkt1);
}

void onPacket2(std::shared_ptr<scnet::Session> session, scnet::Header *header, Packet2 *pkt2) {
    std::cout << "Packet2, str=" << pkt2->str() << std::endl;
    session->send(header, pkt2);
}

void onPacket3(std::shared_ptr<scnet::Session> session, scnet::Header *header, Packet3 *pkt3) {
    std::cout << "Packet3 received, boolvalue=" << (pkt3->boolvalue()?"true":"false") << std::endl;
    session->send(header, pkt3);
}

void onPacket4(std::shared_ptr<scnet::Session> session, scnet::Header *header, Packet4 *pkt4) {
    std::cout << "Packet4 received, floatvalue=" << pkt4->floatvalue() << ", doublevalue=" << pkt4->doublevalue() << std::endl;
    session->send(header, pkt4);
}

void onArrayMessage(std::shared_ptr<scnet::Session> session, scnet::Header *header, ArrayMessage *arrayMessage) {
    std::cout << "received array size: %d, message: " << arrayMessage->strarr_size() << std::endl;
    for(int i = 0 ; i < arrayMessage->strarr_size() ; i++) {
        std::cout << arrayMessage->strarr(i) << " ";
    }
    std::cout << std::endl;
    
    session->send(header, arrayMessage);
}

long GetFileSize(std::string filename)
{
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

void onImageRequest(std::shared_ptr<scnet::Session> session, scnet::Header *header, ImageRequest *imageRequest) {
    int size = GetFileSize(imageRequest->name());
    std::cout << "requested image name=" << imageRequest->name() << ", size=" << size << std::endl;
    
    if(size < 0) {
        auto response = example::ImageResponse();
        session->send(header, &response);
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
    
    bool ret = session->send(header, response);
    std::cout << "ret : " << (ret? "true" : "false") << std::endl;
    
    delete []buf;
    
}
