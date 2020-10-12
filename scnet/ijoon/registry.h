#ifndef __REGISTRY_H__
#define __REGISTRY_H__
#include "session.h"
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

class AbstractCallbackWrapper {
public:
    virtual void callback(std::shared_ptr<ijoon::BaseSession> session, scnet::Header *header, google::protobuf::Message *message) {}
    virtual void callback(std::shared_ptr<ijoon::BaseSession> session, char *message, unsigned int length) {}
};

template <class S, class T>
class CallbackWrapper : public AbstractCallbackWrapper{
public:
    CallbackWrapper(std::function<void(std::shared_ptr<S>, scnet::Header *, T *)> _callbackFunc) {
        callbackFunc = _callbackFunc;
    }
    ~CallbackWrapper() {}
    
    void callback(std::shared_ptr<ijoon::BaseSession> session, scnet::Header *header, google::protobuf::Message *message) override {
        if(callbackFunc == nullptr) return;
        if(message == nullptr) {
            callbackFunc(std::static_pointer_cast<S>(session), header, nullptr);
            return;
        }
        
        callbackFunc(std::static_pointer_cast<S>(session), header, static_cast<T *>((void *)message));
    }
    
public:
    std::function<void(std::shared_ptr<S>, scnet::Header *, T *)> callbackFunc;
};

template <class SrcType, class ObjectPtrType, class... Args>
class Registry
{
public:
    typedef std::function<ObjectPtrType(Args...)> Creator;
    
    static Registry<SrcType, ObjectPtrType> *Get() {
        static Registry<SrcType, ObjectPtrType> sharedRegistry = Registry<SrcType, ObjectPtrType>();
        return &sharedRegistry;
    }

    Registry() : registry_creater(), registry_getter() {}

    void Register(const SrcType& key, Creator creator)
    {
        if (HasCreator(key)) {
            printf("Key already registered.\n");
            std::exit(1);
        }
        registry_creater[key] = creator;
    }

    void Register(const std::string key, google::protobuf::uint32 type)
    {
        if (HasGetter(key)) {
            printf("Key already registered.\n");
            std::exit(1);
        }
        registry_getter[key] = type;
    }
    
    void Register(const SrcType& key, AbstractCallbackWrapper *callbackWrapper)
    {
        if (HasCallbackWrapper(key)) {
            printf("key already registered.\n");
            std::exit(1);
        }
        registry_callback_wrapper[key] = callbackWrapper;
    }
    
    unsigned int Register(AbstractCallbackWrapper *callbackWrapper)
    {
        if(callbackWrapper != nullptr) {
            registry_callback_wrapper[packet_index] = callbackWrapper;
            return packet_index++;
        }
        
        return 0;
    }
    
    inline bool HasCreator(const SrcType& key) { return (registry_creater.count(key) != 0); }
    inline bool HasGetter(const std::string key) { return (registry_getter.count(key) != 0); }
    inline bool HasCallbackWrapper(const SrcType& key) { return (registry_callback_wrapper.count(key) != 0); }

    ObjectPtrType Create(const SrcType& key, Args... args)
    {
        if (!HasCreator(key))
        {
            // Returns nullptr if the key is not registered.
            return nullptr;
        }
        return registry_creater[key](args...);
    }
    
    int GetType(const std::string key)
    {
        if(!HasGetter(key))
        {
            return -1;
        }

        return registry_getter[key];
    }
    
    AbstractCallbackWrapper *GetCallbackWrapper(const SrcType& packetTypeInt, Args... args)
    {
        if (!HasCallbackWrapper(packetTypeInt))
        {
            // Return nullptr if the key is not registered.
            return nullptr;
        }
        return registry_callback_wrapper[packetTypeInt];
    }
    
private:
    std::map<SrcType, Creator> registry_creater;
    std::map<std::string, google::protobuf::uint32> registry_getter;
    std::map<SrcType, AbstractCallbackWrapper *> registry_callback_wrapper;
    unsigned int packet_index = 1000; // TODO: settting outside
};

template <class SrcType, class ObjectPtrType, class... Args>
class Registerer {
public:
    Registerer( // packetType => pb instance
               Registry<SrcType, ObjectPtrType, Args...>* registry,
               const SrcType key,
               typename Registry<SrcType, ObjectPtrType, Args...>::Creator creator
               ) {
        registry->Register(key, creator);
    }

    Registerer( // className => packetType
               Registry<SrcType, ObjectPtrType, Args...>* registry,
               const std::string key,
               google::protobuf::uint32 type
               ) {
        registry->Register(key, type);
    }
    
    Registerer( // packetType => callbackWrapper
               Registry<SrcType, ObjectPtrType, Args...>* registry,
               const SrcType packetTypeInt,
               AbstractCallbackWrapper *callbackWrapper
               ) {
        const SrcType key = packetTypeInt;
        registry->Register(key, callbackWrapper);
    }
    
    template <class DerivedType>
    static ObjectPtrType DefaultCreator(Args... args) {
        return ObjectPtrType(new DerivedType(args...));
    }
};

Registry<int, google::protobuf::Message *> *GetRegistry();

// Reference: https://stackoverflow.com/a/17624752
// This is some crazy magic that helps produce __BASE__247
// Vanilla interpolation of __BASE__##__LINE__ would produce __BASE____LINE__
// I still can't figure out why it works, but it has to do with macro resolution ordering
#define PP_CAT(a, b) PP_CAT_I(a, b)
#define PP_CAT_I(a, b) PP_CAT_II(~, a ## b)
#define PP_CAT_II(p, res) res
#define UNIQUE_NAME(base) PP_CAT(base, __LINE__)

#define SCNET_PROTOBUF_MESSAGE_REGISTRATION(packetTypeInt, messageClassName, callbackFunc) \
static Registerer<int, google::protobuf::Message* > UNIQUE_NAME(a)( \
                    GetRegistry(), \
                    packetTypeInt, \
                    Registerer<int, google::protobuf::Message* >::DefaultCreator<messageClassName>); \
static Registerer<int, google::protobuf::Message* > UNIQUE_NAME(b)( \
                    GetRegistry(), \
                    messageClassName().GetTypeName(), \
                    packetTypeInt); \
static Registerer<int, google::protobuf::Message* > UNIQUE_NAME(c)( \
                    GetRegistry(), \
                    packetTypeInt, \
                    new CallbackWrapper<ijoon::Session, messageClassName>(callbackFunc))

#define SCNET_PROTOBUF_UDP_MESSAGE_REGISTRATION(packetTypeInt, messageClassName, callbackFunc) \
static Registerer<int, google::protobuf::Message* > UNIQUE_NAME(a)( \
                    GetRegistry(), \
                    packetTypeInt, \
                    Registerer<int, google::protobuf::Message* >::DefaultCreator<messageClassName>); \
static Registerer<int, google::protobuf::Message* > UNIQUE_NAME(b)( \
                    GetRegistry(), \
                    messageClassName().GetTypeName(), \
                    packetTypeInt); \
static Registerer<int, google::protobuf::Message* > UNIQUE_NAME(c)( \
                    GetRegistry(), \
                    packetTypeInt, \
                    new CallbackWrapper<ijoon::RendezvousSession, messageClassName>(callbackFunc))

#endif
