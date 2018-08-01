#include "embodied_session.h"

ijoon::EmbodiedSession::EmbodiedSession(std::shared_ptr<ijoon::JClientSocket> cs, int identifier) : BaseSession(cs), identifier(identifier) {
}

ijoon::EmbodiedSession::~EmbodiedSession() {
    
}
