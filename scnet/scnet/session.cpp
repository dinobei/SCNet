#include "session.h"
#include "registry.h"

bool scnet::Session::send(google::protobuf::Message *message, std::function<void(std::shared_ptr<Session>, scnet::Header *, google::protobuf::Message *)> cb) {
    auto registry = Registry<int, google::protobuf::Message *>().Get();
    int typeInt = registry->GetType(message->GetTypeName());
    if(typeInt < 0) {
        std::cout << "You must regist protobuf-message before send(), [" << message->GetTypeName() << "]" << std::endl;
        exit(-1);
    }
    scnet::Header header;
    header.set_packettype(typeInt);
    if(cb != nullptr) {
        header.set_req_cb(registry->Register(new CallbackWrapper<scnet::Session, google::protobuf::Message>(cb)));
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
    
    snd_mtx.lock();
    if(!this->cs->safe_send(ori_buf, 0, total_size , 0)) {
        snd_mtx.unlock();
        delete[] ori_buf;
        return false;
    }
    snd_mtx.unlock();
    
    delete []ori_buf;
    return true;
}

bool scnet::Session::send(scnet::Header *_header, google::protobuf::Message *message, std::function<void(std::shared_ptr<Session>, scnet::Header *, google::protobuf::Message *)> cb) {
    auto registry = Registry<int, google::protobuf::Message *>().Get();
    int typeInt = registry->GetType(message->GetTypeName());
    if(typeInt < 0) {
        std::cout << "You must regist protobuf-message before send(), [" << message->GetTypeName() << "]" << std::endl;
        exit(-1);
    }
    scnet::Header header;
    header.set_packettype(typeInt);
    if(cb != nullptr) {
        header.set_req_cb(registry->Register(new CallbackWrapper<scnet::Session, google::protobuf::Message>(cb)));
    }
    if(_header != nullptr && _header->req_cb() > 0) {
        header.set_res_cb(_header->req_cb());
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
    
    snd_mtx.lock();
    if(!this->cs->safe_send((char *)ori_buf, 0, total_size , 0)) {
        snd_mtx.unlock();
        delete[] ori_buf;
        return false;
    }
    snd_mtx.unlock();
    
    delete []ori_buf;
    return true;
}

bool scnet::Session::recv() {
    const int head_length = MAGIC_PACKET_LENGTH + 4 + 2;
    char head_pkt[head_length] = {0,};
    
    rcv_mtx.lock();
    if(!this->cs->safe_recv(head_pkt, 0, head_length, 0)) {
        rcv_mtx.unlock();
        return false;
    }
    
    if(head_pkt[0] != MAGIC_PACKET[0] || head_pkt[1] != MAGIC_PACKET[1]) {
        rcv_mtx.unlock();
        return false;
    }
    
    const int pkt_size = (unsigned char)head_pkt[2] << 24 | (unsigned char)head_pkt[3] << 16 | (unsigned char)head_pkt[4] << 8 | (unsigned char)head_pkt[5];
    const ushort header_size = head_pkt[6] << 8 | head_pkt[7];
    
    char *pkt = new char[pkt_size];
    if(pkt_size > 0 && !this->cs->safe_recv(pkt, pkt_size)) {
        rcv_mtx.unlock();
        return false;
    }
    rcv_mtx.unlock();
    
    scnet::Header header;
    header.ParseFromArray(pkt, header_size);
    using namespace google::protobuf;
    auto registry = Registry<int, google::protobuf::Message *>().Get();
    
    Message *message = registry->Create(header.packettype());
    if(pkt_size-header_size > 0) {
        message->ParseFromArray(pkt+header_size, pkt_size-header_size);
    }
    delete[] pkt;

    if(header.res_cb() > 0) {
        auto cb_wrapper = registry->GetCallbackWrapper(header.res_cb());
        
        auto sess = std::shared_ptr<Session>(this, [](Session *sess) {});
        cb_wrapper->callback(sess, &header, message);
        delete message;
        return true;
    }
    
    AbstractCallbackWrapper *callbackWrapper = registry->GetCallbackWrapper(header.packettype());
    if(callbackWrapper != nullptr) {
        auto sess = std::shared_ptr<Session>(this, [](Session *sess) {});
        callbackWrapper->callback(sess, &header, message);
    }

    delete message;
    return true;
}
