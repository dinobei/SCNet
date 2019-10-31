#pragma once
// google protobuf runtime library
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/message.h>

#include <cstring>
#include <ijoon/coreutils.h>

namespace ijoon {
    #define MAGIC_PACKET_LENGTH             2
    const char MAGIC_PACKET[2] = {'I', 'J'};
    
    #define HEADER_ELEMENTS                 5
    #define MAX_PACKET_HEADER_SIZE          (7 * HEADER_ELEMENTS)
    
    #define MAX_PACKET_SIZE 655350
    #define MAX_WAIT_SEND 1000
    
    struct MessageHeader {
        google::protobuf::uint32 dataSize;
        google::protobuf::uint32 packetType;
        google::protobuf::uint32 messageType;
        google::protobuf::uint32 cryptType;
        google::protobuf::uint32 connectionID;
    };
    
    enum MESSAGE_TYPE {
        PROTOBUF = 0,
        RAWBYTE,
    };
    
    std::shared_ptr<std::vector<std::string>> paramParser(char *param, int paramSize);
    
    bool readHeader(char *packet, int length, ijoon::MessageHeader &messageHeader, int &cursor);
    void makeHeader(char *buf, ijoon::MessageHeader &messageHeader);
}
