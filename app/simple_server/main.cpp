#include "server.h"
#include <sys/stat.h>
#include "registry.h"

/* implemented proto messages */
#include "packet_type.pb.h"
#include "example.pb.h"
using namespace example;

void onDummyPacket1(std::shared_ptr<scnet::Session> session, scnet::Header *header, DummyPacket1 *pkt1);
void onDummyPacket2(std::shared_ptr<scnet::Session> session, scnet::Header *header, DummyPacket2 *pkt2);
void onPing(std::shared_ptr<scnet::Session> session, scnet::Header *header, Ping *ping);

void onServerStarted();
void onServerStopped();

std::shared_ptr<scnet::Session> getClientSession(std::shared_ptr<cppsocket::tcp_socket> socket);
void onClientConnected(std::shared_ptr<scnet::Session> session);
void onClientTimeout(std::shared_ptr<scnet::Session> session);
void onClientDisconnected(std::shared_ptr<scnet::Session> session);

int main(int argv, char** argc) {
    signal(SIGPIPE, SIG_IGN);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(dummyPacket1, DummyPacket1, onDummyPacket1);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(dummyPacket2, DummyPacket2, onDummyPacket2);
    SCNET_PROTOBUF_MESSAGE_REGISTRATION(ping, Ping, onPing);

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
        
        auto ping = Ping();
        session->send(&ping, nullptr);
    }
}

void onClientDisconnected(std::shared_ptr<scnet::Session> session) {
    std::cout << "ClientDisconnected, " << session->getClientSocket()->get_socket_identifier() << std::endl;
}

void onDummyPacket1(std::shared_ptr<scnet::Session> session, scnet::Header *header, DummyPacket1 *pkt1) {
    std::cout << "DummyPacket1 received, number=" << pkt1->number() << std::endl;
    session->send(header, pkt1);
}

void onDummyPacket2(std::shared_ptr<scnet::Session> session, scnet::Header *header, DummyPacket2 *pkt2) {
    std::cout << "DummyPacket2, array size: %d, message: " << pkt2->strarr_size() << std::endl;
    for(int i = 0 ; i < pkt2->strarr_size() ; i++) {
        std::cout << pkt2->strarr(i) << " ";
    }
    std::cout << std::endl;
    
    session->send(header, pkt2);
}

void onPing(std::shared_ptr<scnet::Session> session, scnet::Header *header, Ping *ping) {
    std::cout << "received ping from client" << std::endl;
}
