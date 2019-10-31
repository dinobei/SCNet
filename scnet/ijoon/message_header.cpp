#include "message_header.h"
#include "registry.h"
#include "rendezvous_message.h"

char seperator = ' ';

std::shared_ptr<std::vector<std::string>> ijoon::paramParser(char *param, int paramSize) {
    auto vec = std::shared_ptr<std::vector<std::string>>(new std::vector<std::string>());
    char *token = std::strtok(param, &seperator);
    while (token != NULL) {
        vec->push_back(token);
        token = std::strtok(NULL, &seperator);
    }
    if(vec->size() != paramSize) {
        ijn_print(DP_ERROR, "invalid parameters, %d != %d", vec->size(), paramSize);
        for(int i = 0 ; i < vec->size() ; i++) {
            ijn_print(DP_INFO, "%d) %s", i+1, vec->at(i).c_str());
        }
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
