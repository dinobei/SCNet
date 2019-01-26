#pragma once
#include "rendezvous_server.h"


namespace ijoon {
    enum RENDEZVOUS_MSG {
        REGISTRATION_RENDEZVOUS_CLIENT_REQUEST = 10000, // SP private ip, SP private port
        REGISTRATION_RENDEZVOUS_CLIENT_SUCCESS, // SP public ip, SP public port
        REGISTRATION_RELAY_SERVER_REQUEST, // nullptr
        REGISTRATION_RELAY_SERVER_SUCCESS, // nullptr
        
        CONNECTION_REQUEST, // TP public ip, TP public port
        
        CONNECTION_FAILED, // nullptr
        
        // relay
        RELAY_SERVICE_REQUEST, // SP pubilc ip, SP public port, TP public ip, TP public port
        RELAY_SESSION_READY, // SP pubilc ip, SP public port, TP public ip, TP public port
        RELAY_SERVER_INFORMATION, // RelS-ip, RelS-port, other-peer-IP, other-peer-PORT (to SP, TP)
        REGISTRATION_RELAY_PEER_REQUEST, // other peer ip, other peer port
        REGISTRATION_RELAY_PEER_SUCCESS, // other peer ip, other peer port
        REGISTRATION_RELAY_PEER_FAILED, // other peer ip, other peer port
        RELAY_SESSION_CREATED, // SP ip, SP port, TP ip, TP port
        RELAY_SESSION_CREATING_FAILED, // SP ip, SP port, TP ip, TP port     //note: timeout후에도 relay 클라이언트들이 나에게 regist를 하지 않으면 이 콜백을 날려줘야함
        
        CONNECTION_RELAY_SERVICE_SUCCESS, // RelS-ip, RelS-port, other-peer-IP, other-peer-PORT
        CONNECTION_RELAY_SERVICE_FAILED, // nullptr
        
        // pub/pub or pri/pub
        DIRECT_CONNECTION_AVAILABLE, // TP public ip, TP public port
        DIRECT_CONNECTION_REQUEST, // nullptr
        DIRECT_CONNECTION_RESPONSE, // nullptr
        
        // pub/pri
        REVERSE_CONNECTION, // SP public ip, SP public port
        REVERSE_CONNECTION_REQUEST, // nullptr
        REVERSE_CONNECTION_RESPONSE, // nullptr
        
        // pri/pri
        UDP_HOLE_PUNCHING_AVAILABLE, // SP's public ip, public port, private ip, private port to TP (The opposite is also the case.)
        UDP_HOLE_PUNCHING_REQUEST, // isPublic (1=true, 0=false)
        UDP_HOLE_PUNCHING_RESPONSE, // isPublic (1=true, 0=false)
        
        RENDEZVOUS_MSG_END,
    };
    
    #define MAX_PACKET_SIZE 65535
}
