#pragma once
#include "randezvous_server.h"


namespace ijoon {
    enum RANDEZVOUS_MSG {
        REGISTER_REQUEST = 10000, // SP private ip, SP private port
        REGISTER_RESPONSE_SUCCESS, // * SP public ip, SP public port
        REGISTER_RESPONSE_FAILED, // * nullptr
        NOT_REGISTERED, // * nullptr
        
        
        CONNECTION_REQUEST, // TP public ip, TP public port
        CONNECTED, // RelS-ip, RelS-port, other-peer-IP, other-peer-PORT
        
        // pub/pub or pri/pub
        COMMON_CONNECTION_READY, // * TP public ip, TP public port
        COMMON_CONNECTION_REQUEST, // * nullptr
        COMMON_CONNECTION_RESPONSE, // * nullptr
        
        // pub/pri
        REVERSE_CONNECTION_READY, // * TP public ip, TP public port
        REVERSE_CONNECTION, // * SP public ip, SP public port
        REVERSE_CONNECTION_REQUEST, // nullptr
        REVERSE_CONNECTION_RESPONSE, // nullptr
        
        // pri/pri
        UDP_HOLE_PUNCHING, // * SP's public ip, public port, private ip, private port to TP (The opposite is also the case.)
        UDP_HOLE_PUNCHING_REQUEST, // * requested-ip, requested-port
        UDP_HOLE_PUNCHING_RESPONSE, // * requested-ip, requested-port
        
        // relay
        RELAY_SERVICE_REQUEST, // * SP pubilc ip, SP public port, TP public ip, TP public port
        RELAY_SERVICE_READY, // * SP pubilc ip, SP public port, TP public ip, TP public port
        READY_TO_RELAY, // * RelS-ip, RelS-port, other-peer-IP, other-peer-PORT (to SP, TP)
        NO_RELAY_SERVER, // * nullptr
        REGISTER_RELAY_PEER_REQUEST, // * other peer ip, other peer port
        REGISTER_RELAY_PEER_RESPONSE_SUCCESS, // * other peer ip, other peer port
        REGISTER_RELAY_PEER_RESPONSE_FAILED, // * other peer ip, other peer port
        RELAY_SERVICE_RESPONSE_SUCCESS, // * SP ip, SP port, TP ip, TP port
        RELAY_SERVICE_RESPONSE_FAILED, // * SP ip, SP port, TP ip, TP port     //note: timeout후에도 relay 클라이언트들이 나에게 regist를 하지 않으면 이 콜백을 날려줘야함
        
        REGISTER_RELAY_REQUEST, // * nullptr
        REGISTER_RELAY_RESPONSE, // * nullptr
        
        RANDEZVOUS_MSG_END,
    };
    
    #define MAX_PACKET_SIZE 65535
}
