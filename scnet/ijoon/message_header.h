#pragma once
// google protobuf runtime library
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/message.h>

#include <ijoon/coreutils.h>

namespace ijoon {
    #define MAGIC_PACKET_LENGTH             2
    const char MAGIC_PACKET[2] = {'I', 'J'};
    
    #define HEADER_ELEMENTS                 5
    #define MAX_PACKET_HEADER_SIZE          (7 * HEADER_ELEMENTS)
    
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
        RAWBYTE_RELAY,
        PROTOBUF_RELAY,
    };
    
    bool readHeader(char *packet, int length, ijoon::MessageHeader &messageHeader, int &cursor);
    void makeHeader(char *buf, ijoon::MessageHeader &messageHeader);
    
    bool send(std::shared_ptr<UDPSocket> socket, std::shared_ptr<Peer> peer, uint connectionID, int packetType);
    bool send(std::shared_ptr<UDPSocket> socket, Peer peer, uint connectionID, int packetType);
    bool send(std::shared_ptr<UDPSocket> socket, std::shared_ptr<Peer> peer, uint connectionID, int packetType, char *message, unsigned int length);
    bool send(std::shared_ptr<UDPSocket> socket, Peer peer, uint connectionID, int packetType, char *message, unsigned int length);
    bool send(std::shared_ptr<UDPSocket> socket, Peer peer, MessageHeader messageHeader, char *message);
    bool send(std::shared_ptr<UDPSocket> socket, Peer peer, uint connectionID, std::shared_ptr<google::protobuf::Message> message);
    bool send(std::shared_ptr<UDPSocket> socket, Peer peer, uint connectionID, google::protobuf::Message *message);
    
    bool sendRelay(std::shared_ptr<UDPSocket> socket, Peer peer, uint connectionID, int packetType, char *message, unsigned int length);
}
