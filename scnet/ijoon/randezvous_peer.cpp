#include "randezvous_peer.h"

void ijoon::RandezvousPeer::setPrivatePeer(std::string ip, std::string port) {
    this->privatePeer.setIP(ip);
    this->privatePeer.setPort(port);
}

void ijoon::RandezvousPeer::setRelayPeer(std::string ip, std::string port, int uniqueID) {
    this->relayTargetUniqueID = uniqueID;
    this->relayPeer.setIP(ip);
    this->relayPeer.setPort(port);
}
