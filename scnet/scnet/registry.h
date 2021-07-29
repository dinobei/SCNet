#pragma once
#include <functional>
#include <string>
#include <google/protobuf/message.h>
#define NOMINMAX
#ifdef max
#undef  max
#endif
#ifdef min
#undef min
#endif

#include "session.h"

class AbstractCallbackWrapper {
public:
    virtual void callback(std::shared_ptr<scnet::BaseSession> session, scnet::Header *header, google::protobuf::Message *message) {}
    virtual void callback(std::shared_ptr<scnet::BaseSession> session, char *message, unsigned int length) {}
};

template <class S, class T>
class CallbackWrapper : public AbstractCallbackWrapper{
public:
    CallbackWrapper(std::function<void(std::shared_ptr<S>, scnet::Header *, T *)> _callbackFunc) {
        callbackFunc = _callbackFunc;
    }
    ~CallbackWrapper() {}
    
    void callback(std::shared_ptr<scnet::BaseSession> session, scnet::Header *header, google::protobuf::Message *message) override {
        if(callbackFunc == nullptr) {
            std::cout << "[warning] unprocessed message: " << header->packettype() << std::endl;
            return;
        }
        if(message == nullptr) {
            callbackFunc(std::static_pointer_cast<S>(session), header, nullptr);
            return;
        }
        
        callbackFunc(std::static_pointer_cast<S>(session), header, static_cast<T *>((void *)message));
    }
    
public:
    std::function<void(std::shared_ptr<S>, scnet::Header *, T *)> callbackFunc;
};

template <class ObjectPtrType, class... Args>
class Registry
{
public:
    typedef std::function<ObjectPtrType(Args...)> Creator;
    
    static Registry<ObjectPtrType> *Get() {
        static Registry<ObjectPtrType> sharedRegistry = Registry<ObjectPtrType>();
        return &sharedRegistry;
    }

    Registry() : registry_creater() {}

    void Register(const std::string& key, Creator creator)
    {
        if (HasCreator(key)) {
            printf("Key already registered.\n");
            std::exit(1);
        }
        registry_creater[key] = creator;
    }
    
    void Register(const std::string& key, AbstractCallbackWrapper *callbackWrapper)
    {
        if (HasCallbackWrapper(key)) {
            printf("key already registered.\n");
            std::exit(1);
        }
        registry_callback_wrapper[key] = callbackWrapper;
    }
    
    inline bool HasCreator(const std::string& key) { return (registry_creater.count(key) != 0); }
    inline bool HasCallbackWrapper(const std::string& key) { return (registry_callback_wrapper.count(key) != 0); }

    ObjectPtrType Create(const std::string& key, Args... args)
    {
        if (!HasCreator(key))
        {
            // Returns nullptr if the key is not registered.
            return nullptr;
        }
        return registry_creater[key](args...);
    }
        
    AbstractCallbackWrapper *GetCallbackWrapper(const std::string& packetType, Args... args)
    {
        if (!HasCallbackWrapper(packetType))
        {
            // Return nullptr if the key is not registered.
            return nullptr;
        }
        return registry_callback_wrapper[packetType];
    }
    
private:
    std::map<std::string, Creator> registry_creater;
    std::map<std::string, AbstractCallbackWrapper *> registry_callback_wrapper;
    unsigned int packet_index = 1000; // TODO: settting outside
};

template <class ObjectPtrType, class... Args>
class Registerer {
public:
    Registerer( // packetType => pb instance
               Registry<ObjectPtrType, Args...>* registry,
               const std::string key,
               typename Registry<ObjectPtrType, Args...>::Creator creator
               ) {
        registry->Register(key, creator);
    }
    
    Registerer( // packetType => callbackWrapper
               Registry<ObjectPtrType, Args...>* registry,
               const std::string key,
               AbstractCallbackWrapper *callbackWrapper
               ) {
        registry->Register(key, callbackWrapper);
    }
    
    template <class DerivedType>
    static ObjectPtrType DefaultCreator(Args... args) {
        return ObjectPtrType(new DerivedType(args...));
    }
};

Registry<google::protobuf::Message *> *GetRegistry();

// Reference: https://stackoverflow.com/a/17624752
// This is some crazy magic that helps produce __BASE__247
// Vanilla interpolation of __BASE__##__LINE__ would produce __BASE____LINE__
// I still can't figure out why it works, but it has to do with macro resolution ordering
#define PP_CAT(a, b) PP_CAT_I(a, b)
#define PP_CAT_I(a, b) PP_CAT_II(~, a ## b)
#define PP_CAT_II(p, res) res
#define UNIQUE_NAME(base) PP_CAT(base, __LINE__)

#define SCNET_PROTOBUF_MESSAGE_REGISTRATION(messageClassName, callbackFunc) \
static Registerer<google::protobuf::Message* > UNIQUE_NAME(a)( \
                    GetRegistry(), \
                    messageClassName().GetTypeName(), \
                    Registerer<google::protobuf::Message* >::DefaultCreator<messageClassName>); \
static Registerer<google::protobuf::Message* > UNIQUE_NAME(c)( \
                    GetRegistry(), \
                    messageClassName().GetTypeName(), \
                    new CallbackWrapper<scnet::Session, messageClassName>(callbackFunc))
