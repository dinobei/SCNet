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

bool ijoon::RendezvousSession::send(int packetType, char *message, unsigned int length) {
    if(this->privatePeer != nullptr) {
        printf("sent to private peer\n");
        return ijoon::send(socket, privatePeer, connectionID, packetType, message, length);
    }
    else if(this->publicPeer != nullptr) {
        printf("sent to public peer\n");
        return ijoon::send(socket, publicPeer, connectionID, packetType, message, length);
    }
    else if(this->relayPeer != nullptr) {
        printf("sent to relay peer\n");
        return ijoon::sendRelay(socket, relayPeer, connectionID, packetType, message, length);
    }
    
    printf("send failed\n");
    return false;
}

bool ijoon::RendezvousSession::send(std::shared_ptr<google::protobuf::Message> message) {
    if(this->privatePeer != nullptr) {
        printf("sent to private peer\n");
        return ijoon::send(socket, privatePeer, connectionID, message);
    }
    else if(this->publicPeer != nullptr) {
        printf("sent to public peer\n");
        return ijoon::send(socket, publicPeer, connectionID, message);
    }
    else if(this->relayPeer != nullptr) {
        printf("sent to relay peer\n");
        return ijoon::sendRelay(socket, relayPeer, connectionID, message);
    }
    
    printf("send failed\n");
    return false;
}

void ijoon::RendezvousSession::setPrivatePeer(std::string ip, std::string port) {
    this->privatePeer = std::shared_ptr<Peer>(new Peer(ip, port));
}

void ijoon::RendezvousSession::setPublicPeer(std::string ip, std::string port) {
    this->publicPeer = std::shared_ptr<Peer>(new Peer(ip, port));
}
void ijoon::RendezvousSession::setRelayPeer(std::string ip, std::string port) {
    this->relayPeer = std::shared_ptr<Peer>(new Peer(ip, port));
}

void ijoon::RendezvousSession::clearPrivatePeer() {
    this->privatePeer = nullptr;
}

void ijoon::RendezvousSession::clearPublicPeer() {
    this->publicPeer = nullptr;
}

void ijoon::RendezvousSession::clearRelayPeer() {
    this->relayPeer = nullptr;
}

bool ijoon::RendezvousSession::isConnected() {
    if(this->relayPeer == nullptr && this->publicPeer == nullptr && this->privatePeer == nullptr) {
        return false;
    }
    return true;
}
