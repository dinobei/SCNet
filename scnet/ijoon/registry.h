#ifndef __REGISTRY_H__
#define __REGISTRY_H__
#include "session.h"
#include <functional>
#include <string>
#include <google/protobuf/message.h>

namespace ijoon {
    void initGlobalVariables();
}

class AbstractCallbackWrapper {
public:
    virtual void callback(ijoon::BaseSession *session, google::protobuf::Message *message) {}
    virtual void callback(ijoon::BaseSession *session, char *message, unsigned int length) {}
};

template <class S, class T>
class CallbackWrapper : public AbstractCallbackWrapper{
public:
    CallbackWrapper(std::function<void(S *, T *)> _callbackFunc) {
        callbackFunc = _callbackFunc;
    }
    ~CallbackWrapper() {}
    
    void callback(ijoon::BaseSession *session, google::protobuf::Message *message) override {
        if(callbackFunc == nullptr) return;
        if(message == nullptr) {
            callbackFunc(static_cast<S *>(session), nullptr);
            return;
        }
        
        callbackFunc(static_cast<S *>(session), static_cast<T *>((void *)message));
    }
    
public:
    std::function<void(S *, T *)> callbackFunc;
};

template <class S>
class RawCallbackWrapper : public AbstractCallbackWrapper{
public:
    RawCallbackWrapper(std::function<void(S *, void *, unsigned int)> _callbackFunc) {
        callbackFunc = _callbackFunc;
    }
    ~RawCallbackWrapper() {}
    
    void callback(ijoon::BaseSession *session, char *message, unsigned int length) override {
        if(message == nullptr) {
            callbackFunc(static_cast<S *>(session), nullptr, 0);
            return;
        }
        
        callbackFunc(static_cast<S *>(session), (void *)message, length);
    }
    
public:
    std::function<void(S *, void *, unsigned int)> callbackFunc;
};

template <class SrcType, class ObjectPtrType, class... Args>
class Registry
{
public:
    typedef std::function<ObjectPtrType(Args...)> Creator;

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
    
    AbstractCallbackWrapper *GetCallbackWrapper(const SrcType& messageTypeInt, const SrcType& packetTypeInt, Args... args)
    {
        const SrcType& key = messageTypeInt * 100000 + packetTypeInt;
        
        if (!HasCallbackWrapper(key))
        {
            // Returns nullptr if the key is not registered.
            return nullptr;
        }
        return registry_callback_wrapper[key];
    }
    
private:
    std::map<SrcType, Creator> registry_creater;
    std::map<std::string, google::protobuf::uint32> registry_getter;
    std::map<SrcType, AbstractCallbackWrapper *> registry_callback_wrapper;
};

template <class SrcType, class ObjectPtrType, class... Args>
class Registerer {
public:
    Registerer(
               Registry<SrcType, ObjectPtrType, Args...>* registry,
               const SrcType key,
               typename Registry<SrcType, ObjectPtrType, Args...>::Creator creator
               ) {
        registry->Register(key, creator);
    }

    Registerer(
               Registry<SrcType, ObjectPtrType, Args...>* registry,
               const std::string key,
               google::protobuf::uint32 type
               ) {
        registry->Register(key, type);
    }
    
    Registerer(
               Registry<SrcType, ObjectPtrType, Args...>* registry,
               const SrcType messageTypeInt,
               const SrcType packetTypeInt,
               AbstractCallbackWrapper *callbackWrapper
               ) {
        const SrcType key = messageTypeInt * 100000 + packetTypeInt;
        registry->Register(key, callbackWrapper);
    }
    
    template <class DerivedType>
    static ObjectPtrType DefaultCreator(Args... args) {
        return ObjectPtrType(new DerivedType(args...));
    }
};

extern Registry<int, google::protobuf::Message* >* BaseMessageRegistry;


// Reference: https://stackoverflow.com/a/17624752
// This is some crazy magic that helps produce __BASE__247
// Vanilla interpolation of __BASE__##__LINE__ would produce __BASE____LINE__
// I still can't figure out why it works, but it has to do with macro resolution ordering
#define PP_CAT(a, b) PP_CAT_I(a, b)
#define PP_CAT_I(a, b) PP_CAT_II(~, a ## b)
#define PP_CAT_II(p, res) res
#define UNIQUE_NAME(base) PP_CAT(base, __LINE__)

#define SCNET_PROTOBUF_MESSAGE_REGISTRATION(ns, packetTypeInt, messageClassName, callbackFunc) \
static Registerer<int, google::protobuf::Message* > UNIQUE_NAME(a)( \
                    BaseMessageRegistry, \
                    packetTypeInt, \
                    Registerer<int, google::protobuf::Message* >::DefaultCreator<messageClassName>); \
static Registerer<int, google::protobuf::Message* > UNIQUE_NAME(b)( \
                    BaseMessageRegistry, \
                    #ns "." #messageClassName, \
                    packetTypeInt); \
static Registerer<int, google::protobuf::Message* > UNIQUE_NAME(c)( \
                    BaseMessageRegistry, \
                    ijoon::MESSAGE_TYPE::PROTOBUF, \
                    packetTypeInt, \
                    new CallbackWrapper<ijoon::Session, messageClassName>(callbackFunc))

#define SCNET_RAW_MESSAGE_REGISTRATION(packetTypeInt, callbackFunc) \
static Registerer<int, google::protobuf::Message* > UNIQUE_NAME(a)( \
                    BaseMessageRegistry, \
                    ijoon::MESSAGE_TYPE::RAWBYTE, \
                    packetTypeInt, \
                    new RawCallbackWrapper<ijoon::Session>(callbackFunc))

#define SCNET_RAW_UDP_MESSAGE_REGISTRATION(packetTypeInt, callbackFunc) \
                    static Registerer<int, google::protobuf::Message* > UNIQUE_NAME(a)( \
                    BaseMessageRegistry, \
                    ijoon::MESSAGE_TYPE::RAWBYTE, \
                    packetTypeInt, \
                    new RawCallbackWrapper<ijoon::RendezvousSession>(callbackFunc))

#define SCNET_PROTOBUF_UDP_MESSAGE_REGISTRATION(ns, packetTypeInt, messageClassName, callbackFunc) \
static Registerer<int, google::protobuf::Message* > UNIQUE_NAME(a)( \
                    BaseMessageRegistry, \
                    packetTypeInt, \
                    Registerer<int, google::protobuf::Message* >::DefaultCreator<messageClassName>); \
static Registerer<int, google::protobuf::Message* > UNIQUE_NAME(b)( \
                    BaseMessageRegistry, \
                    #ns "." #messageClassName, \
                    packetTypeInt); \
static Registerer<int, google::protobuf::Message* > UNIQUE_NAME(c)( \
                    BaseMessageRegistry, \
                    ijoon::MESSAGE_TYPE::PROTOBUF, \
                    packetTypeInt, \
                    new CallbackWrapper<ijoon::RendezvousSession, messageClassName>(callbackFunc))

#endif
