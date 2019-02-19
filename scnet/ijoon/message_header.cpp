#include "message_header.h"
#include "registry.h"

char seperator = ' ';

std::shared_ptr<std::vector<std::string>> ijoon::paramParser(char *param, int paramSize) {
    auto vec = std::shared_ptr<std::vector<std::string>>(new std::vector<std::string>());
    char *token = std::strtok(param, &seperator);
    while (token != NULL) {
        vec->push_back(token);
        token = std::strtok(NULL, &seperator);
    }
    if(vec->size() != paramSize) {
        ijn_print(DP_ERROR, "invalid parameters");
        return nullptr;
    }
    return vec;
}


void ijoon::makeHeader(char *buf, ijoon::MessageHeader &messageHeader) {
    google::protobuf::io::ArrayInputStream ais(buf, MAX_PACKET_HEADER_SIZE);
    google::protobuf::io::CodedInputStream coded_input(&ais);
    coded_input.ReadVarint32(&messageHeader.dataSize); // Decode the HDR and get the size
    coded_input.ReadVarint32(&messageHeader.packetType); // Decode the HDR and get the packet type
    coded_input.ReadVarint32(&messageHeader.messageType); // Decode the message type
    coded_input.ReadVarint32(&messageHeader.cryptType); // Decode the crypt
    coded_input.ReadVarint32(&messageHeader.connectionID); // Decode connection id
}

bool ijoon::readHeader(char *packet, int length, ijoon::MessageHeader &messageHeader, int &cursor) {
    if(length < MAGIC_PACKET_LENGTH + HEADER_ELEMENTS) {
        return false;
    }
    
    // read magic packet
    for(int i = 0 ; i < MAGIC_PACKET_LENGTH ; i++) {
        if(packet[i] != ijoon::MAGIC_PACKET[i]) {
            return false;
        }
    }
    
    cursor = MAGIC_PACKET_LENGTH;
    
    // read header
    int receivedHeaderComponent = 0;
    
    while(true) {
        if(cursor >= length) {
            return false;
        }
        
        if((packet[cursor++]&0xFF) > 127) {
            continue;
        }
        
        if(++receivedHeaderComponent == HEADER_ELEMENTS) {
            break;
        }
    }
    
    makeHeader(&packet[MAGIC_PACKET_LENGTH], messageHeader);
    return true;
}

bool ijoon::send(ikcpcb *kcp, uint connectionID, int packetType, char *message, unsigned int length) {
    int size = MAGIC_PACKET_LENGTH + MAX_PACKET_HEADER_SIZE + length;
    char *buf = new char[size];
    google::protobuf::io::ArrayOutputStream aos(buf,size);
    google::protobuf::io::CodedOutputStream coded_output(&aos);
    coded_output.WriteRaw(MAGIC_PACKET, MAGIC_PACKET_LENGTH);
    coded_output.WriteVarint32(length); // data size
    coded_output.WriteVarint32(packetType); // packet type
    coded_output.WriteVarint32(ijoon::MESSAGE_TYPE::RAWBYTE); // message type
    coded_output.WriteVarint32(0); // crypt type
    coded_output.WriteVarint32(connectionID); // connection id
    
    if(length != 0)
        coded_output.WriteRaw(message, length);
    
    ikcp_send(kcp, buf, coded_output.ByteCount());
    
    delete []buf;
    return true;
}

bool ijoon::send(ikcpcb *kcp, uint connectionID, std::shared_ptr<google::protobuf::Message> message) {
    int typeInt = BaseMessageRegistry->GetType(message->GetTypeName());
    if(typeInt < 0) {
        ijn_print(DP_ERROR, "You must regist protobuf-message before send(), [%s]", message->GetTypeName().c_str());
        exit(-1);
    }
    
    int size = MAGIC_PACKET_LENGTH + MAX_PACKET_HEADER_SIZE + message->ByteSize();
    char *buf = new char[size];
    google::protobuf::io::ArrayOutputStream aos(buf,size);
    google::protobuf::io::CodedOutputStream coded_output(&aos);
    coded_output.WriteRaw(ijoon::MAGIC_PACKET, MAGIC_PACKET_LENGTH);
    coded_output.WriteVarint32(message->ByteSize()); // data size
    coded_output.WriteVarint32(typeInt); // packet type
    coded_output.WriteVarint32(ijoon::MESSAGE_TYPE::PROTOBUF); // message type
    coded_output.WriteVarint32(0); // crypt type
    coded_output.WriteVarint32(connectionID); // connection id
    
    message->SerializeToCodedStream(&coded_output);
    
    ikcp_send(kcp, buf, coded_output.ByteCount());
    
    delete []buf;
    return true;
}

bool ijoon::sendRelay(ikcpcb *kcp, uint connectionID, int packetType, char *message, unsigned int length) {
    int size = MAGIC_PACKET_LENGTH + MAX_PACKET_HEADER_SIZE + length;
    char *buf = new char[size];
    google::protobuf::io::ArrayOutputStream aos(buf,size);
    google::protobuf::io::CodedOutputStream coded_output(&aos);
    coded_output.WriteRaw(MAGIC_PACKET, MAGIC_PACKET_LENGTH);
    coded_output.WriteVarint32(length); // data size
    coded_output.WriteVarint32(packetType); // packet type
    coded_output.WriteVarint32(ijoon::MESSAGE_TYPE::RAWBYTE_RELAY); // message type
    coded_output.WriteVarint32(0); // crypt type
    coded_output.WriteVarint32(connectionID); // connection id
    
    if(length != 0)
        coded_output.WriteRaw(message, length);
    
    ikcp_send(kcp, buf, coded_output.ByteCount());
    
    delete []buf;
    return true;
}

bool ijoon::sendRelay(ikcpcb *kcp, uint connectionID, std::shared_ptr<google::protobuf::Message> message) {
    int typeInt = BaseMessageRegistry->GetType(message->GetTypeName());
    if(typeInt < 0) {
        ijn_print(DP_ERROR, "You must regist protobuf-message before send(), [%s]", message->GetTypeName().c_str());
        exit(-1);
    }
    
    int size = MAGIC_PACKET_LENGTH + MAX_PACKET_HEADER_SIZE + message->ByteSize();
    char *buf = new char[size];
    google::protobuf::io::ArrayOutputStream aos(buf,size);
    google::protobuf::io::CodedOutputStream coded_output(&aos);
    coded_output.WriteRaw(ijoon::MAGIC_PACKET, MAGIC_PACKET_LENGTH);
    coded_output.WriteVarint32(message->ByteSize()); // data size
    coded_output.WriteVarint32(typeInt); // packet type
    coded_output.WriteVarint32(ijoon::MESSAGE_TYPE::PROTOBUF_RELAY); // message type
    coded_output.WriteVarint32(0); // crypt type
    coded_output.WriteVarint32(connectionID); // connection id
    
    message->SerializeToCodedStream(&coded_output);
    
    ikcp_send(kcp, buf, coded_output.ByteCount());
    
    delete []buf;
    return true;
}
