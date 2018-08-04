#pragma once
// google protobuf runtime library
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/message.h>

#include <ijoon/coreutils.h>

namespace ijoon {
    #define PACKET_DATA_LENGTH              4
    #define PACKET_CMD_TYPE                 4
    #define PACKET_CRYPT_TYPE               4
    
    #define HEADER_ELEMENTS                 3
    #define MAX_PACKET_HEADER_SIZE          (7 * HEADER_ELEMENTS)
    
    struct MessageHeader {
        google::protobuf::uint32 dataSize;
        google::protobuf::uint32 packetType;
        google::protobuf::uint32 cryptType;
    };
}
