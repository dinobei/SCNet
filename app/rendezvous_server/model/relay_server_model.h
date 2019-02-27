#pragma once
#include <string>

namespace ijoon {
    class RelayServerModel {
    public:
        std::string publicIP;
        std::string publicPort;
        size_t ping;
    };
}
