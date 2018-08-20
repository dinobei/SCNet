#pragma once
// google protobuf runtime library
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/message.h>

#include <ijoon/coreutils.h>

namespace ijoon {
    #define MAGIC_PACKET_LENGTH             2
    const char MAGIC_PACKET[2] = {'I', 'J'};
    
    #define HEADER_ELEMENTS                 4
    #define MAX_PACKET_HEADER_SIZE          (7 * HEADER_ELEMENTS)
    
    struct MessageHeader {
        google::protobuf::uint32 dataSize;
        google::protobuf::uint32 packetType;
        google::protobuf::uint32 messageType;
        google::protobuf::uint32 cryptType;
    };
}
