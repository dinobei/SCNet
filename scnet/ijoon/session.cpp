#include "session.h"
#include "registry.h"

bool ijoon::Session::send(int packetType, char *message, unsigned int length) {
    int size = MAGIC_PACKET_LENGTH + MAX_PACKET_HEADER_SIZE + length;
    char *buf = new char[size];
    google::protobuf::io::ArrayOutputStream aos(buf,size);
    google::protobuf::io::CodedOutputStream coded_output(&aos);
    coded_output.WriteRaw(MAGIC_PACKET, MAGIC_PACKET_LENGTH);
    coded_output.WriteVarint32(length); // data size
    coded_output.WriteVarint32(packetType); // packet type
    coded_output.WriteVarint32(ijoon::MESSAGE_TYPE::RAWBYTE); // message type
    coded_output.WriteVarint32(0); // crypt type
    
    coded_output.WriteRaw(message, length);
    
    if(!this->cs->safeSend(buf, 0, coded_output.ByteCount() , 0)) {
        delete[] buf;
        return false;
    }
    
    delete []buf;
    return true;
}

bool ijoon::Session::send(google::protobuf::Message *message) {
    int typeInt = BaseMessageRegistry->GetType(message->GetTypeName());
    if(typeInt < 0) {
        ijn_print(DP_ERROR, "You must regist protobuf-message before send(), [%s]", message->GetTypeName().c_str());
        exit(-1);
    }
    
    int size = MAGIC_PACKET_LENGTH + MAX_PACKET_HEADER_SIZE + message->ByteSize();
    char *buf = new char[size];
    google::protobuf::io::ArrayOutputStream aos(buf,size);
    google::protobuf::io::CodedOutputStream coded_output(&aos);
    coded_output.WriteRaw(MAGIC_PACKET, MAGIC_PACKET_LENGTH);
    coded_output.WriteVarint32(message->ByteSize()); // data size
    coded_output.WriteVarint32(typeInt); // packet type
    coded_output.WriteVarint32(0); // message type
    coded_output.WriteVarint32(0); // crypt type
    
    message->SerializeToCodedStream(&coded_output);
    
    if(!this->cs->safeSend(buf, 0, coded_output.ByteCount() , 0)) {
        delete[] buf;
        return false;
    }
    
    delete []buf;
    return true;
}

bool ijoon::Session::send(std::shared_ptr<google::protobuf::Message> message) {
    int typeInt = BaseMessageRegistry->GetType(message->GetTypeName());
    if(typeInt < 0) {
        ijn_print(DP_ERROR, "You must regist protobuf-message before send(), [%s]", message->GetTypeName().c_str());
        exit(-1);
    }
    
    int size = MAGIC_PACKET_LENGTH + MAX_PACKET_HEADER_SIZE + message->ByteSize();
    char *buf = new char[size];
    google::protobuf::io::ArrayOutputStream aos(buf,size);
    google::protobuf::io::CodedOutputStream coded_output(&aos);
    coded_output.WriteRaw(MAGIC_PACKET, MAGIC_PACKET_LENGTH);
    coded_output.WriteVarint32(message->ByteSize()); // data size
    coded_output.WriteVarint32(typeInt); // packet type
    coded_output.WriteVarint32(0); // message type
    coded_output.WriteVarint32(0); // crypt type
    
    message->SerializeToCodedStream(&coded_output);
    
    if(!this->cs->safeSend(buf, 0, coded_output.ByteCount() , 0)) {
        delete[] buf;
        return false;
    }
    
    delete []buf;
    return true;
}

bool ijoon::Session::recvHeader(ijoon::MessageHeader &messageHeader) {
    char magicPacket[2] = {0,};
    // read magic packet
    if(!this->cs->safeRecv(magicPacket, 0, MAGIC_PACKET_LENGTH, 0)) {
        return false;
    }

    if(magicPacket[0] != MAGIC_PACKET[0] || magicPacket[1] != MAGIC_PACKET[1]) {
        return false;
    }

    char headerBuffer[MAX_PACKET_HEADER_SIZE] = {0,};
    
    // read header
    int readingHeaderSize = 0;
    int receivedHeaderComponent = 0;
    
    while(true) {
        if(!this->cs->safeRecv(headerBuffer, readingHeaderSize++, 1, 0)) {
            return false;
        }
        
        if((headerBuffer[readingHeaderSize-1]&0xFF) > 127) {
            continue;
        }
        
        if(++receivedHeaderComponent == HEADER_ELEMENTS) {
            break;
        }
    }
    
    ijoon::makeHeader(headerBuffer, messageHeader);
    return true;
}

google::protobuf::Message *ijoon::Session::recvProtobufBody(MessageHeader &messageHeader) {
    google::protobuf::Message *response = BaseMessageRegistry->Create(messageHeader.packetType);
    if(response == nullptr) {
        ijn_print(DP_INFO, "Unknown packet type(=%d)", messageHeader.packetType);
        return nullptr;
    }
    
    // read contents
    const int responseSize = messageHeader.dataSize;
    if(responseSize > 0) {
        char *responseBuffer = new char[responseSize];
        
        // Read the entire buffer including the header
        if(!this->cs->safeRecv(responseBuffer, 0, responseSize, 0)) {
            delete []responseBuffer;
            return nullptr;
        }
        
        response->ParseFromArray(responseBuffer, messageHeader.dataSize);
        delete []responseBuffer;
    }
    
    return response;
}

char *ijoon::Session::recvRawBody(MessageHeader &messageHeader) {
    const int responseSize = messageHeader.dataSize;
    char *responseBuffer = new char[responseSize];
    
    if(!this->cs->safeRecv(responseBuffer, 0, responseSize, 0)) {
        delete []responseBuffer;
        return nullptr;
    }
    
    return responseBuffer;
}

void ijoon::RendezvousSession::setPrivateKcpPeer(std::string ip, std::string port,
                                                 int (*output)(const char *buf, int len, ikcpcb *kcp, void *user)) {
    this->privateKcpPeer = std::shared_ptr<KcpPeer>(new KcpPeer(this->socket, ip, port, output));
    this->privateKcpPeer->setConnectionID(this->connectionID);
}

void ijoon::RendezvousSession::setPublicKcpPeer(std::string ip, std::string port,
                                                int (*output)(const char *buf, int len, ikcpcb *kcp, void *user)) {
    this->publicKcpPeer = std::shared_ptr<KcpPeer>(new KcpPeer(this->socket, ip, port, output));
    this->publicKcpPeer->setConnectionID(this->connectionID);
}

void ijoon::RendezvousSession::setRelayKcpPeer(std::string ip, std::string port,
                                               int (*output)(const char *buf, int len, ikcpcb *kcp, void *user)) {
    this->relayKcpPeer = std::shared_ptr<KcpPeer>(new KcpPeer(this->socket, ip, port, output));
    this->relayKcpPeer->setConnectionID(this->connectionID);
}

void ijoon::RendezvousSession::setPrivateKcpPeer(std::shared_ptr<ijoon::KcpPeer> kcpPeer) {
    this->privateKcpPeer = kcpPeer;
    this->privateKcpPeer->setConnectionID(this->connectionID);
}

void ijoon::RendezvousSession::setPublicKcpPeer(std::shared_ptr<ijoon::KcpPeer> kcpPeer) {
    this->publicKcpPeer = kcpPeer;
    this->publicKcpPeer->setConnectionID(this->connectionID);
}

void ijoon::RendezvousSession::setRelayKcpPeer(std::shared_ptr<ijoon::KcpPeer> kcpPeer) {
    this->relayKcpPeer = kcpPeer;
    this->relayKcpPeer->setConnectionID(this->connectionID);
}

std::shared_ptr<ijoon::KcpPeer> ijoon::RendezvousSession::getPrivateKcpPeer() {
    return this->privateKcpPeer;
}

std::shared_ptr<ijoon::KcpPeer> ijoon::RendezvousSession::getPublicKcpPeer() {
    return this->publicKcpPeer;
}

std::shared_ptr<ijoon::KcpPeer> ijoon::RendezvousSession::getRelayKcpPeer() {
    return this->relayKcpPeer;
}

void ijoon::RendezvousSession::clearPrivateKcpPeer() {
    this->privateKcpPeer = nullptr;
}

void ijoon::RendezvousSession::clearPublicKcpPeer() {
    this->publicKcpPeer = nullptr;
}

void ijoon::RendezvousSession::clearRelayKcpPeer() {
    this->relayKcpPeer = nullptr;
}

bool ijoon::RendezvousSession::isPublic() {
    if(this->publicKcpPeer == nullptr || this->privateKcpPeer == nullptr) {
        return false;
    }
    
    if( (this->publicKcpPeer->getPeer().getIP().compare(this->getPrivateKcpPeer()->getPeer().getIP()) == 0) &&
       (this->getPublicKcpPeer()->getPeer().getPort() == this->getPrivateKcpPeer()->getPeer().getPort()) ) {
        return true;
    }

    return false;
}

bool ijoon::RendezvousSession::isConnected() {
    if(this->relayKcpPeer != nullptr || this->publicKcpPeer != nullptr || this->privateKcpPeer != nullptr) {
        return true;
    }
    return false;
}

bool ijoon::RendezvousSession::send(int packetType, char *message, unsigned int length) {
    if(this->privateKcpPeer != nullptr) {
        ijoon::send(this->privateKcpPeer->getKcp(), this->connectionID, packetType, message, length);
        return true;
    }
    else if(this->publicKcpPeer != nullptr) {
        ijoon::send(this->publicKcpPeer->getKcp(), this->connectionID, packetType, message, length);
        return true;
    }
    else if(this->relayKcpPeer != nullptr) {
        ijoon::send(this->relayKcpPeer->getKcp(), this->connectionID, packetType, message, length);
        return true;
    }
    return false;
}

bool ijoon::RendezvousSession::send(std::shared_ptr<google::protobuf::Message> message) {
    if(this->privateKcpPeer != nullptr) {
        ijoon::send(this->privateKcpPeer->getKcp(), this->connectionID, message);
        return true;
    }
    else if(this->publicKcpPeer != nullptr) {
        ijoon::send(this->publicKcpPeer->getKcp(), this->connectionID, message);
        return true;
    }
    else if(this->relayKcpPeer != nullptr) {
        ijoon::send(this->relayKcpPeer->getKcp(), this->connectionID, message);
        return true;
    }
    return false;
}
