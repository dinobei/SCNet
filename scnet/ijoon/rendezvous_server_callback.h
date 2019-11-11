#pragma once
#include <stdio.h>
#include "session.h"

namespace rens {
void onRegistrationRendezvousClientRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onRegistrationRelayServerRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onRelaySessionReady(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onRelaySessionCreated(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onConnectionRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onConnectionIdReceived(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onPingRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onUnregistrationRendezvousClientRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onUnregistrationRelayServerRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
};

