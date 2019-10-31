#include "session.h"
#include "registry.h"
#include "rendezvous_message.h"

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
    coded_output.WriteVarint32(0); // connectionID
    
    coded_output.WriteRaw(message, length);
    
    if(!this->cs->safeSend(buf, 0, coded_output.ByteCount() , 0)) {
        delete[] buf;
        return false;
    }
    
    delete []buf;
    return true;
}

bool ijoon::Session::send(google::protobuf::Message *message) {
    auto registry = Registry<int, google::protobuf::Message *>().Get();
    int typeInt = registry->GetType(message->GetTypeName());
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
    coded_output.WriteVarint32(ijoon::MESSAGE_TYPE::PROTOBUF); // message type
    coded_output.WriteVarint32(0); // crypt type
    coded_output.WriteVarint32(0); // connectionID
    
    message->SerializeToCodedStream(&coded_output);
    
    if(!this->cs->safeSend(buf, 0, coded_output.ByteCount() , 0)) {
        delete[] buf;
        return false;
    }
    
    delete []buf;
    return true;
}

bool ijoon::Session::send(std::shared_ptr<google::protobuf::Message> message) {
    auto registry = Registry<int, google::protobuf::Message *>().Get();
    int typeInt = registry->GetType(message->GetTypeName());
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
    coded_output.WriteVarint32(ijoon::MESSAGE_TYPE::PROTOBUF); // message type
    coded_output.WriteVarint32(0); // crypt type
    coded_output.WriteVarint32(0); // connectionID
    
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
    auto registry = Registry<int, google::protobuf::Message *>().Get();
    google::protobuf::Message *response = registry->Create(messageHeader.packetType);
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

bool ijoon::KcpPeer::send(ijoon::MessageHeader& messageHeader, char *message, unsigned int length) {
    int size = MAGIC_PACKET_LENGTH + MAX_PACKET_HEADER_SIZE + length;
    char *buf = new char[size];
    google::protobuf::io::ArrayOutputStream aos(buf,size);
    google::protobuf::io::CodedOutputStream coded_output(&aos);
    coded_output.WriteRaw(MAGIC_PACKET, MAGIC_PACKET_LENGTH);
    coded_output.WriteVarint32(length); // data size
    coded_output.WriteVarint32(messageHeader.packetType); // packet type
    coded_output.WriteVarint32(messageHeader.messageType); // message type
    coded_output.WriteVarint32(0); // crypt type
    coded_output.WriteVarint32(connectionID); // connectionID
    
    if(length != 0)
        coded_output.WriteRaw(message, length);
    
    mutex.lock();
    ikcp_send(kcp, buf, coded_output.ByteCount());
    mutex.unlock();
    
    delete []buf;
    return true;
}

bool ijoon::KcpPeer::send(int packetType, char *message, unsigned int length) {
    int size = MAGIC_PACKET_LENGTH + MAX_PACKET_HEADER_SIZE + length;
    char *buf = new char[size];
    google::protobuf::io::ArrayOutputStream aos(buf,size);
    google::protobuf::io::CodedOutputStream coded_output(&aos);
    coded_output.WriteRaw(MAGIC_PACKET, MAGIC_PACKET_LENGTH);
    coded_output.WriteVarint32(length); // data size
    coded_output.WriteVarint32(packetType); // packet type
    coded_output.WriteVarint32(ijoon::MESSAGE_TYPE::RAWBYTE); // message type
    coded_output.WriteVarint32(0); // crypt type
    coded_output.WriteVarint32(connectionID); // connectionID
    
    if(length != 0)
        coded_output.WriteRaw(message, length);
    
    mutex.lock();
    ikcp_send(kcp, buf, coded_output.ByteCount());
    mutex.unlock();
    
    delete []buf;
    return true;
}

bool ijoon::KcpPeer::send(google::protobuf::Message *message) {
    auto registry = Registry<int, google::protobuf::Message *>().Get();
    int typeInt = registry->GetType(message->GetTypeName());
    assert(typeInt>=0);

    int size = MAGIC_PACKET_LENGTH + MAX_PACKET_HEADER_SIZE + message->ByteSize();
    char *buf = new char[size];
    google::protobuf::io::ArrayOutputStream aos(buf,size);
    google::protobuf::io::CodedOutputStream coded_output(&aos);
    coded_output.WriteRaw(MAGIC_PACKET, MAGIC_PACKET_LENGTH);
    coded_output.WriteVarint32(message->ByteSize()); // data size
    coded_output.WriteVarint32(typeInt); // packet type
    coded_output.WriteVarint32(ijoon::MESSAGE_TYPE::PROTOBUF); // message type
    coded_output.WriteVarint32(0); // crypt type
    coded_output.WriteVarint32(connectionID); // connectionID

    message->SerializeToCodedStream(&coded_output);

    mutex.lock();
    ikcp_send(kcp, buf, coded_output.ByteCount());
    mutex.unlock();

    delete []buf;
    return true;
}

bool ijoon::KcpPeer::send(std::shared_ptr<google::protobuf::Message> message) {
    auto registry = Registry<int, google::protobuf::Message *>().Get();
    int typeInt = registry->GetType(message->GetTypeName());
    
    assert(typeInt>=0);
    
    int size = MAGIC_PACKET_LENGTH + MAX_PACKET_HEADER_SIZE + message->ByteSize();
    char *buf = new char[size];
    google::protobuf::io::ArrayOutputStream aos(buf,size);
    google::protobuf::io::CodedOutputStream coded_output(&aos);
    coded_output.WriteRaw(MAGIC_PACKET, MAGIC_PACKET_LENGTH);
    coded_output.WriteVarint32(message->ByteSize()); // data size
    coded_output.WriteVarint32(typeInt); // packet type
    coded_output.WriteVarint32(ijoon::MESSAGE_TYPE::PROTOBUF); // message type
    coded_output.WriteVarint32(0); // crypt type
    coded_output.WriteVarint32(connectionID); // connectionID
    
    message->SerializeToCodedStream(&coded_output);
    
    mutex.lock();
    ikcp_send(kcp, buf, coded_output.ByteCount());
    mutex.unlock();

    delete []buf;
    return true;
}

int ijoon::KcpPeer::getSendBufSize() {
    mutex.lock();
    int waitsnd = ikcp_waitsnd(kcp);
    mutex.unlock();
    return waitsnd;
}
