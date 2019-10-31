#pragma once
#include <stdio.h>
#include "session.h"

void onRegistrationRendezvousClientResponse(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);

void onConnectionIdCreated(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);

void onRelayServerInformation(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onConnectionRelayServiceResult(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onRelaySessionInvalid(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onRelayServerDisconnected(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);

void onDirectConnectionAvailable(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onDirectConnectionRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onDirectConnectionResponse(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);

void onReverseConnectionReady(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onReverseConnection(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onReverseConnectionRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onReverseConnectionResponse(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);

void onUdpHolePunchingAvailable(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onUdpHolePunchingRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onUdpHolePunchingResponse(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);

void onConnectionTargetInvalid(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);

void onPingRequest(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);
void onPingResponse(std::shared_ptr<ijoon::KcpPeer> kcpPeer, void *buffer, unsigned int length);

