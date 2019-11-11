#pragma once
#include <stdio.h>
#include "session.h"

namespace rels {
void onRelayServiceRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onRegistrationRelayServerResponse(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);

void onRegistrationRelayPeerRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onPingRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onPingResponse(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
}
