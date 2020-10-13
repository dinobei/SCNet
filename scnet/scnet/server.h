#pragma once
#include <map>
#include <functional>
#include <assert.h>
#include <chrono>
#include <thread>
#include <atomic>

#include "session.h"

namespace scnet {
    class Server {
    public:
        Server(ushort port, int recvTimeoutMs, bool useMultiThread);
        ~Server();
        
        bool start();
        bool stop();
        
        int clientSize();
        std::shared_ptr<Session> session(int socketId);
        std::map<int, std::shared_ptr<Session>> &getClientMap();
        
        bool addClient(std::shared_ptr<cppsocket::tcp_socket> clientSocket, std::shared_ptr<Session> sess);
        bool removeClient(std::shared_ptr<scnet::Session> session);
        bool removeClient(std::shared_ptr<cppsocket::tcp_socket> clientSocket);
        bool removeClient(int socketId);
        
        ushort getServerPort();
        int getRecvTimeoutMs();
        
        bool isMultiThreadBased() { return this->useMultiThread; }
        
        // Server lifecycle
        std::function<void()> onServerStarted;
        std::function<void()> onServerStopped;
        
        // Client lifecycle
        std::function<std::shared_ptr<scnet::Session>(std::shared_ptr<cppsocket::tcp_socket>)> getClientSession;
        std::function<void(std::shared_ptr<Session>)> onClientConnected;
        std::function<void(std::shared_ptr<Session>)> onClientTimeout;
        std::function<void(std::shared_ptr<Session>)> onClientDisconnected;

        void ServerMainThread();
        void ServerServiceThread(int socketId);
    private:
        cppsocket::tcp_socket *servSocket;
        std::map<int, std::shared_ptr<Session>> clientMap;
        std::thread *thread;
        std::atomic_bool condition;
        ushort port;
        int recvTimeoutMs;
        bool useMultiThread;
    };
}
