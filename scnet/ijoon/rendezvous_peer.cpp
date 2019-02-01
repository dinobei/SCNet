#include "rendezvous_peer.h"

void ijoon::RendezvousPeer::setPrivatePeer(std::string ip, std::string port) {
    this->privatePeer.setIP(ip);
    this->privatePeer.setPort(port);
}

void ijoon::RendezvousPeer::setRelayPeer(std::string ip, std::string port, int connectionID) {
    this->connectionID = connectionID;
    this->relayPeer.setIP(ip);
    this->relayPeer.setPort(port);
}
