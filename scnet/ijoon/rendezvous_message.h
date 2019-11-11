#pragma once
#include "session.h"

namespace ijoon {
    enum RENDEZVOUS_MSG {
        REGISTRATION_RENDEZVOUS_CLIENT_REQUEST = 10000, // SP private ip, SP private port, Serial, MAC, Version
        REGISTRATION_RENDEZVOUS_CLIENT_RESPONSE, // isSuccess(success:1 / competible:0 / not_competible:-1 / failed:-2 / internalError:-3), SP public ip, SP public port
        REGISTRATION_RELAY_SERVER_REQUEST, // nullptr
        REGISTRATION_RELAY_SERVER_RESPONSE, // isSuccess(success:1 / competible:0 / not_competible:-1 / failed:-2 / internalError:-3)
        PING_REQUEST, // nullptr
        PING_RESPONSE, // nullptr
        
        CONNECTION_REQUEST, // TP public ip, TP public port
        
        CONNECTION_ID_CREATED, // ConnectionID, TP public ip, TP public port (RanS to SP)
        CONNECTION_ID_RECEIVED, // ConnectionID (SP to RanS)
        CONNECTION_TARGET_INVALID, // TP public ip, TP public port
        CONNECTION_FAILED, // nullptr
        
        // relay
        RELAY_SERVICE_REQUEST, // ConnectionID, SP pubilc ip, TP pubilc ip
        RELAY_SESSION_READY, // ConnectionID
        RELAY_SERVER_INFORMATION, // ConnectionID, RelS-ip, RelS-port, 1(SP) or 0(TP)
        REGISTRATION_RELAY_PEER_REQUEST, // ConnectionID, 1(SP) or 0(TP)
        RELAY_SESSION_CREATED, // ConnectionID
        RELAY_SESSION_INVALID, // nullptr
        RELAY_SERVER_DISCONNECTED, // nullptr
        
        CONNECTION_RELAY_SERVICE_RESULT, // isSuccess(success:1 / failed:0), ConnectionID, RelS-ip, RelS-port
        
        // pub/pub or pri/pub
        DIRECT_CONNECTION_AVAILABLE, // ConnectionID, TP public ip, TP public port
        DIRECT_CONNECTION_REQUEST, // ConnectionID
        DIRECT_CONNECTION_RESPONSE, // ConnectionID
        
        // pub/pri
        REVERSE_CONNECTION_READY, // ConnectionID
        REVERSE_CONNECTION, // ConnectionID, SP public ip, SP public port
        REVERSE_CONNECTION_REQUEST, // ConnectionID
        REVERSE_CONNECTION_RESPONSE, // ConnectionID
        
        // pri/pri
        UDP_HOLE_PUNCHING_AVAILABLE, // ConnectionID, SP's public ip, public port, private ip, private port to TP (The opposite is also the case.)
        UDP_HOLE_PUNCHING_REQUEST, // ConnectionID, isPublic (1=true, 0=false)
        UDP_HOLE_PUNCHING_RESPONSE, // ConnectionID, isPublic (1=true, 0=false)
        
        UNREGISTRATION_RENDEZVOUS_CLIENT_REQUEST,
        UNREGISTRATION_RENDEZVOUS_CLIENT_RESPONSE,
        UNREGISTRATION_RELAY_SERVER_REQUEST, // nullptr
        UNREGISTRATION_RELAY_SERVER_RESPONSE, // nullptr
        RENDEZVOUS_MSG_END,
    };
}
