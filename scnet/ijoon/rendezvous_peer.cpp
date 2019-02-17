#include "rendezvous_peer.h"

bool ijoon::RendezvousPeer::isPublic() {
    if( (this->publicPeer.getIP().compare(this->privatePeer.getIP()) == 0) && (this->publicPeer.getPort() == this->privatePeer.getPort()) ) {
        return true;
    }
    
    return false;
}

void ijoon::RendezvousPeer::setPrivatePeer(std::string ip, std::string port) {
    this->privatePeer.setIP(ip);
    this->privatePeer.setPort(port);
}

void ijoon::RendezvousPeer::setRelayPeer(std::string ip, std::string port, int connectionID) {
    this->connectionID = connectionID;
    this->relayPeer.setIP(ip);
    this->relayPeer.setPort(port);
}
