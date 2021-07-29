#include "session.h"
#include "registry.h"

int message_id = 1;
bool scnet::Session::send(google::protobuf::Message *message, DedicatedCallback onReceived, std::function<void()> onTimeout, std::function<void()> onEnded) {
    auto registry = Registry<google::protobuf::Message *>().Get();

    scnet::Header header;
    header.set_packettype(message->GetTypeName());
    if(onReceived != nullptr) {
        auto currentTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        CallbackContext cbCtx{onReceived, onTimeout, onEnded, currentTime, 0, 3*1000, 3600*1000};
        
        cbCtxMap.insert(std::pair<int, CallbackContext>(message_id, cbCtx));
        header.set_id(message_id++);
    }
    
    const int packet_size = header.ByteSizeLong() + message->ByteSizeLong();
    const int total_size = MAGIC_PACKET_LENGTH + 4 + 2 + packet_size;
    char *buf = new char[total_size];
    char *ori_buf = buf;
    memcpy(buf, MAGIC_PACKET, 2);
    buf += 2;
    buf[0] = (packet_size >> 24) & 0xFF;
    buf[1] = (packet_size >> 16) & 0xFF;
    buf[2] = (packet_size >> 8) & 0xFF;
    buf[3] = packet_size & 0xFF;
    buf += 4;
    buf[0] = (header.ByteSizeLong() >> 8) & 0xFF;
    buf[1] = header.ByteSizeLong() & 0xFF;
    buf += 2;
    header.SerializeToArray(buf, header.ByteSizeLong());
    buf += header.ByteSizeLong();
    message->SerializeToArray(buf, message->ByteSizeLong());
    
    std::lock_guard<std::mutex> lg(snd_mtx);
    if(!this->cs->safe_send(ori_buf, 0, total_size , 0)) {
        delete[] ori_buf;
        return false;
    }
    
    delete []ori_buf;
    return true;
}

bool scnet::Session::send(scnet::Header *_header, google::protobuf::Message *message) {
    auto registry = Registry<google::protobuf::Message *>().Get();

    scnet::Header header;
    header.set_id(_header->id());
    header.set_packettype(message->GetTypeName());
    
    const int packet_size = header.ByteSizeLong() + message->ByteSizeLong();
    const int total_size = MAGIC_PACKET_LENGTH + 4 + 2 + packet_size;
    char *buf = new char[total_size];
    char *ori_buf = buf;
    memcpy(buf, MAGIC_PACKET, 2);
    buf += 2;
    buf[0] = (packet_size >> 24) & 0xFF;
    buf[1] = (packet_size >> 16) & 0xFF;
    buf[2] = (packet_size >> 8) & 0xFF;
    buf[3] = packet_size & 0xFF;
    buf += 4;
    buf[0] = (header.ByteSizeLong() >> 8) & 0xFF;
    buf[1] = header.ByteSizeLong() & 0xFF;
    buf += 2;
    header.SerializeToArray(buf, header.ByteSizeLong());
    buf += header.ByteSizeLong();
    message->SerializeToArray(buf, message->ByteSizeLong());
    
    std::lock_guard<std::mutex> lg(snd_mtx);
    if(!this->cs->safe_send((char *)ori_buf, 0, total_size , 0)) {
        delete[] ori_buf;
        return false;
    }
    
    delete []ori_buf;
    return true;
}

bool scnet::Session::recv() {
    const int head_length = MAGIC_PACKET_LENGTH + 4 + 2;
    char head_pkt[head_length] = {0,};
    
    {
        std::lock_guard<std::mutex> lg(rcv_mtx);
        if(!this->cs->safe_recv(head_pkt, 0, head_length, 0)) {
            return false;
        }
    }
    
    if(head_pkt[0] != MAGIC_PACKET[0] || head_pkt[1] != MAGIC_PACKET[1]) {
        return false;
    }
    
    const int pkt_size = (unsigned char)head_pkt[2] << 24 | (unsigned char)head_pkt[3] << 16 | (unsigned char)head_pkt[4] << 8 | (unsigned char)head_pkt[5];
    const ushort header_size = head_pkt[6] << 8 | head_pkt[7];
    
    char *pkt = new char[pkt_size];
    {
        std::lock_guard<std::mutex> lg(rcv_mtx);
        if(pkt_size > 0 && !this->cs->safe_recv(pkt, pkt_size)) {
            delete[] pkt;
            return false;
        }
    }
    
    scnet::Header header;
    header.ParseFromArray(pkt, header_size);
    using namespace google::protobuf;
    auto registry = Registry<google::protobuf::Message *>().Get();
    
    Message *message = registry->Create(header.packettype());
    if(pkt_size-header_size > 0) {
        message->ParseFromArray(pkt+header_size, pkt_size-header_size);
    }
    delete[] pkt;

    if(header.id() > 0) {
        auto iter = cbCtxMap.find(header.id());
        if(iter != cbCtxMap.end()) {
            auto cbCtx = iter->second;
            cbCtxMap[header.id()].resUts = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            auto sess = std::shared_ptr<Session>(this, [](Session *sess) {});
            cbCtx.onReceived(sess, &header, message);
            delete message;
            return true;
        }
    }
    
    AbstractCallbackWrapper *callbackWrapper = registry->GetCallbackWrapper(header.packettype());
    if(callbackWrapper != nullptr) {
        auto sess = std::shared_ptr<Session>(this, [](Session *sess) {});
        callbackWrapper->callback(sess, &header, message);
    }

    delete message;
    return true;
}

void scnet::Session::updateDedicatedCallbacks() {
    for(auto iter = this->cbCtxMap.begin() ; iter != this->cbCtxMap.end() ;) {
        auto callbackContext = iter->second;
        auto reqUts = std::chrono::system_clock::from_time_t(callbackContext.reqUts);
        auto resUts = std::chrono::system_clock::from_time_t(callbackContext.resUts);
        auto timeoutMs = std::chrono::milliseconds(callbackContext.timeout);
        auto ctxTimeoutMs = std::chrono::milliseconds(callbackContext.ctxTimeout);
        
        auto currentTime = std::chrono::system_clock::now();
        if(callbackContext.resUts == 0) {
            // check if timeout
            if(reqUts + timeoutMs < currentTime) {
                if(callbackContext.onTimeout != nullptr) callbackContext.onTimeout();
                iter = this->cbCtxMap.erase(iter);
            } else {
                iter++;
            }
        } else {
            // check if discarded session
            if(resUts + ctxTimeoutMs < currentTime) {
                if(callbackContext.onEnded != nullptr) callbackContext.onEnded();
                iter = this->cbCtxMap.erase(iter);
            } else {
                iter++;
            }
        }
    }
}
