#pragma once
#include <iostream>
#include <ijoon/coreutils.h>

namespace ijoon {
    // peer와 소켓을 갖고 메시지를 전송하는 기능 (수신기능 없음)
    // 랑데뷰 서버 기능 역할을 위해 필요한 클라이언트 정보들을 저장하고 제공한다.
    class RendezvousPeer {
    public:
        RendezvousPeer(std::shared_ptr<UDPSocket> socket, ijoon::Peer peer): publicPeer(peer), socket(socket) {
            publicPeer = peer;
        }
        ~RendezvousPeer() {}
        
    public:
        bool isPublic();
        
        void setPrivatePeer(std::string ip, std::string port);
        void setRelayPeer(std::string ip, std::string port, int connectionID);
        
    public:
        ijoon::Peer privatePeer;
        
        ijoon::Peer publicPeer;
        
        int connectionID;
        ijoon::Peer relayPeer;
        
    public:
        time_t LastReportedTime; // 일정시간 이상 메시지가 없을 때 맵에서 제거하기 위함
        
    public:
        std::shared_ptr<UDPSocket> socket;
    };
}
