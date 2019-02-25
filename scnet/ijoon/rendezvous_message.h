#pragma once
#include "rendezvous_server.h"


namespace ijoon {
    enum RENDEZVOUS_MSG {
        REGISTRATION_RENDEZVOUS_CLIENT_REQUEST = 10000, // (Serial), SP private ip, SP private port
        REGISTRATION_RENDEZVOUS_CLIENT_SUCCESS, // (Serial), SP public ip, SP public port
        REGISTRATION_RELAY_SERVER_REQUEST, // nullptr
        REGISTRATION_RELAY_SERVER_SUCCESS, // nullptr
        PING_RELAY_PEER, // nullptr
        PING_CONNECTED_PEER, // nullptr
        PING_CONNECTED_PEER_RESPONSE, // nullptr
        
        CONNECTION_REQUEST, // TP public ip, TP public port
        
        CONNECTION_FAILED, // nullptr
        
        // relay
        RELAY_SERVICE_REQUEST, // SP pubilc ip, TP pubilc ip
        RELAY_SESSION_READY, // nullptr
        RELAY_SERVER_INFORMATION, // RelS-ip, RelS-port, 1(SP) or 0(TP)
        REGISTRATION_RELAY_PEER_REQUEST, // 1(SP) or 0(TP) or nullptr for ping
        REGISTRATION_RELAY_PEER_SUCCESS, // nullptr
        REGISTRATION_RELAY_PEER_FAILED, // nullptr
        RELAY_SESSION_CREATED, // nullptr
        RELAY_SESSION_CREATING_FAILED, // nullptr
        
        CONNECTION_RELAY_SERVICE_SUCCESS, // RelS-ip, RelS-port
        CONNECTION_RELAY_SERVICE_FAILED, // nullptr
        
        // pub/pub or pri/pub
        DIRECT_CONNECTION_AVAILABLE, // TP public ip, TP public port
        DIRECT_CONNECTION_REQUEST, // nullptr
        DIRECT_CONNECTION_RESPONSE, // nullptr
        
        // pub/pri
        REVERSE_CONNECTION_READY, // nullptr
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
