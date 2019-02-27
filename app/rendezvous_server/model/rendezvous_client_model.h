#pragma once
#include <string>

namespace ijoon {
    class RendezvousClientModel {
    public:
        std::string serial;
        std::string publicIP;
        std::string publicPort;
        std::string privateIP;
        std::string privatePort;
        size_t ping;
        
    }
}
