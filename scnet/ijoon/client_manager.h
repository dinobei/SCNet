#pragma once
#include "message_header.h"

namespace ijoon
{
    #define MAX_CONNECTION               64

    typedef void (*onAttachingPtr)(int serverIndex);
    typedef void (*onAttachFailedPtr)(int serverIndex);
    typedef void (*onAttachedPtr)(int serverIndex);
    typedef void (*onDetachedPtr)(int serverIndex);
    typedef void (*onDetachPtr)(int serverIndex);
    typedef void (*onCallbackPtr)(int serverIndex, google::protobuf::Message *response);
    
    // Thread
    void *clientThread(void *arg);
    void *sendRequestThread(void *arg);
    void *recvResponseThread(void *arg);
    
    class ClientManager;
    
    class ClientSession {
    public:
        ClientSession(int servIndex, std::string servIp, int servPort, ClientManager *manager) {
            this->servIndex = servIndex;
            this->servIp = servIp;
            this->servPort = servPort;
            this->manager = manager;
            
            this->eventQueue = new ijoon::BlockingQueue<google::protobuf::Message *>();
            
            this->mainThread = new ijoon::Thread(clientThread, "mainThread");
            this->mainThread->start(this);
        }
        
        ~ClientSession() {
            if(this->recvThread != nullptr) {
                this->recvThread->interrupt();
            }
            
            if(this->sendThread != nullptr) {
                this->sendThread->interrupt();
            }
            
            if(this->mainThread != nullptr) {
                this->mainThread->interrupt();
                this->mainThread->join();
            }
            
            delete this->eventQueue;
            this->eventQueue = NULL;
        }
    public:
        ijoon::JClientSocket *clntSock;

        ijoon::BlockingQueue<google::protobuf::Message *> *eventQueue;

        ijoon::Thread *mainThread;
        ijoon::Thread *sendThread;
        ijoon::Thread *recvThread;
        
    public: // Send request & recv response
        bool sendRequest(google::protobuf::Message *request);
        google::protobuf::Message *recvResponse();
        
    public:
        int getServerIndex() {return this->servIndex;}
        std::string getServerIp() {return this->servIp;}
        int getServerPort() {return this->servPort;}
        
    public:
        ClientManager *manager;
        
    private:
        int servIndex;
        std::string servIp;
        int servPort;
    };


    class ClientManager
    {
    public:
        ClientManager();
        ~ClientManager();

        int Attach(std::string ip, int port);
        bool Detach(int serverIndex);
        bool Control(int serverIndex, google::protobuf::Message *request);
        
        
    public: // Lifecycle callback
        onAttachingPtr onAttaching;
        onAttachFailedPtr onAttachFailed;
        onAttachedPtr onAttached;
        onDetachedPtr onDetached;
        onDetachPtr onDetach;
        onCallbackPtr onCallback;
    private:
        std::map<int, ClientSession *> clientMap;
    };
}
