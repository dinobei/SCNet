#pragma once
/* std headers */
#include <iostream>
#include "base_session.h"

namespace ijoon {
    class EmbodiedSession : public BaseSession {
    public:
        EmbodiedSession(std::shared_ptr<ijoon::JClientSocket> cs, int identifier);
        ~EmbodiedSession();
        
        int getIdentifier() {return this->identifier;}
        
    private:
        int identifier;
    };
}


