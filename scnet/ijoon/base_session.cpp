#include "base_session.h"
#include "registry.h"

void ijoon::BaseSession::startThread(FuncPointer func, std::string name, void *param) {
    this->thread = new ijoon::Thread(func, name);
    this->thread->start(param);
}

bool ijoon::BaseSession::send(google::protobuf::Message *message) {
    int size = message->ByteSize() + PACKET_HEADER_SIZE;
    char *buf = new char[size];
    google::protobuf::io::ArrayOutputStream aos(buf,size);
    google::protobuf::io::CodedOutputStream coded_output(&aos);
    coded_output.WriteVarint32(message->ByteSize());
    coded_output.WriteVarint32(BaseMessageRegistry->GetType(message->GetTypeName()));
    coded_output.WriteVarint32(0);
    
    message->SerializeToCodedStream(&coded_output);
    
    if(!this->cs->safeSend(buf, 0, size, 0)) {
        delete[] buf;
        return false;
    }
    
    delete []buf;
    return true;
}

google::protobuf::Message *ijoon::BaseSession::recv() {
    char headerBuffer[PACKET_HEADER_SIZE];
    if(!this->cs->safeRecv(headerBuffer, 0, PACKET_HEADER_SIZE, MSG_PEEK)) {
        return nullptr;
    }
    
    ijoon::MessageHeader header = makeHeader(headerBuffer);
    
    const int responseSize = header.dataSize + PACKET_HEADER_SIZE;
    char *responseBuffer = new char[responseSize]; // size of the payload and hdr
    
    // Read the entire buffer including the header
    if(!this->cs->safeRecv(responseBuffer, 0, responseSize, 0)) {
        delete []responseBuffer;
        return nullptr;
    }
    
    google::protobuf::Message *response = BaseMessageRegistry->Create(header.packetType);
    if(response == nullptr) {
        ijn_print(DP_DEBUG, "Unknown packet type(=%d)", header.packetType);
        return nullptr;
    }
    
    deSerializeMessage(response, responseBuffer, responseSize, header);
    
    delete []responseBuffer;
    return response;
}

ijoon::MessageHeader ijoon::BaseSession::makeHeader(char *buf) {
    MessageHeader header;
    google::protobuf::io::ArrayInputStream ais(buf, PACKET_HEADER_SIZE);
    google::protobuf::io::CodedInputStream coded_input(&ais);
    coded_input.ReadVarint32(&header.dataSize); // Decode the HDR and get the size
    coded_input.ReadVarint32(&header.packetType); // Decode the HDR and get the packet type
    coded_input.ReadVarint32(&header.cryptType); // Decode the Crypt
    return header;
}

void ijoon::BaseSession::deSerializeMessage(google::protobuf::Message *message, char *buffer, int bufferSize, ijoon::MessageHeader header) {
    // Assign ArrayInputStream with enough memory
    google::protobuf::io::ArrayInputStream ais(buffer, bufferSize);
    google::protobuf::io::CodedInputStream coded_input(&ais);
    
    // Read an unsigned integer with Varint encoding, truncating to 32 bits.
    coded_input.ReadVarint32(&header.dataSize);
    coded_input.ReadVarint32(&header.packetType);
    coded_input.ReadVarint32(&header.cryptType);
    
    // After the message's length is read, PushLimit() is used to prevent the CodedInputStream
    // from reading beyond that length.Limits are used when parsing length-delimited
    // embedded messages
    google::protobuf::io::CodedInputStream::Limit msgLimit = coded_input.PushLimit(header.dataSize);
    
    // De-Serialize
    message->ParseFromCodedStream(&coded_input);
    
    // Once the embedded message has been parsed, PopLimit() is called to undo the limit
    coded_input.PopLimit(msgLimit);
}

