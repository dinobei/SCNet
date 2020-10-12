#pragma once
// google protobuf runtime library
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/message.h>

#include <cstring>
#include <cppsocket/tcp_socket.h>

namespace ijoon
{
#define MAGIC_PACKET_LENGTH 2
    const char MAGIC_PACKET[2] = {'I', 'J'};
} // namespace ijoon
