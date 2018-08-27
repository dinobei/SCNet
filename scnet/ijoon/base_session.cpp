#include "base_session.h"
#include "registry.h"

void ijoon::BaseSession::startThread(FuncPointer func, std::string name, void *param) {
    this->thread = new ijoon::Thread(func, name);
    this->thread->start(param);
}

bool ijoon::BaseSession::send(google::protobuf::Message *message) {
    int size = MAGIC_PACKET_LENGTH + MAX_PACKET_HEADER_SIZE + message->ByteSize();
    char *buf = new char[size];
    google::protobuf::io::ArrayOutputStream aos(buf,size);
    google::protobuf::io::CodedOutputStream coded_output(&aos);
    coded_output.WriteRaw(MAGIC_PACKET, MAGIC_PACKET_LENGTH);
    coded_output.WriteVarint32(message->ByteSize()); // data size
    coded_output.WriteVarint32(BaseMessageRegistry->GetType(message->GetTypeName())); // packet type
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

bool ijoon::BaseSession::send(std::shared_ptr<google::protobuf::Message> message) {
    int size = MAGIC_PACKET_LENGTH + MAX_PACKET_HEADER_SIZE + message->ByteSize();
    char *buf = new char[size];
    google::protobuf::io::ArrayOutputStream aos(buf,size);
    google::protobuf::io::CodedOutputStream coded_output(&aos);
    coded_output.WriteRaw(MAGIC_PACKET, MAGIC_PACKET_LENGTH);
    coded_output.WriteVarint32(message->ByteSize()); // data size
    coded_output.WriteVarint32(BaseMessageRegistry->GetType(message->GetTypeName())); // packet type
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

google::protobuf::Message *ijoon::BaseSession::recv() {
    char magicPacket[2] = {0,};
    // read magic packet
    if(!this->cs->safeRecv(magicPacket, 0, MAGIC_PACKET_LENGTH, 0)) {
        return nullptr;
    }

    if(magicPacket[0] != MAGIC_PACKET[0] || magicPacket[1] != MAGIC_PACKET[1]) {
        return nullptr;
    }

    char headerBuffer[MAX_PACKET_HEADER_SIZE] = {0,};
    
    // read header
    int readingHeaderSize = 0;
    int receivedHeaderComponent = 0;
    
    while(true) {
        if(!this->cs->safeRecv(headerBuffer, readingHeaderSize++, 1, 0)) {
            return nullptr;
        }
        
        if(headerBuffer[readingHeaderSize-1] > 127) {
            continue;
        }
        
        if(++receivedHeaderComponent == HEADER_ELEMENTS) {
            break;
        }
    }
    
    ijoon::MessageHeader header = makeHeader(headerBuffer);
    
    google::protobuf::Message *response = BaseMessageRegistry->Create(header.packetType);
    if(response == nullptr) {
        ijn_print(DP_INFO, "Unknown packet type(=%d)", header.packetType);
        return nullptr;
    }
    
    // read contents
    const int responseSize = header.dataSize;
    if(responseSize > 0) {
        char *responseBuffer = new char[responseSize];
        
        // Read the entire buffer including the header
        if(!this->cs->safeRecv(responseBuffer, 0, responseSize, 0)) {
            delete []responseBuffer;
            return nullptr;
        }
        
        response->ParseFromArray(responseBuffer, header.dataSize);
        delete []responseBuffer;
    }
    
    return response;
}

ijoon::MessageHeader ijoon::BaseSession::makeHeader(char *buf) {
    MessageHeader header;
    google::protobuf::io::ArrayInputStream ais(buf, MAX_PACKET_HEADER_SIZE);
    google::protobuf::io::CodedInputStream coded_input(&ais);
    coded_input.ReadVarint32(&header.dataSize); // Decode the HDR and get the size
    coded_input.ReadVarint32(&header.packetType); // Decode the HDR and get the packet type
    coded_input.ReadVarint32(&header.messageType); // Decode the message type
    coded_input.ReadVarint32(&header.cryptType); // Decode the crypt
    return header;
}
