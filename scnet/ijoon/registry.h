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
    virtual void callback(ijoon::Session *session, google::protobuf::Message *message) {}
};

template <class S, class T>
class CallbackWrapper : public AbstractCallbackWrapper{
public:
    CallbackWrapper(std::function<void(S *, T *)> _callbackFunc) {
        callbackFunc = _callbackFunc;
    }
    ~CallbackWrapper() {}
    
    void callback(ijoon::Session *session, google::protobuf::Message *message) override {
        if(message == nullptr) {
            callbackFunc(static_cast<S *>(session), nullptr);
            return;
        }
        
        callbackFunc(static_cast<S *>(session), static_cast<T *>(message));
    }
    
public:
    std::function<void(S *, T *)> callbackFunc;
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
    
    AbstractCallbackWrapper *GetCallbackWrapper(const SrcType& key, Args... args)
    {
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
               Registry<int, ObjectPtrType, Args...>* registry,
               const std::string key,
               google::protobuf::uint32 type
               ) {
        registry->Register(key, type);
    }
    
    Registerer(
               Registry<SrcType, ObjectPtrType, Args...>* registry,
               const SrcType key,
               AbstractCallbackWrapper *callbackWrapper
               ) {
        registry->Register(key, callbackWrapper);
    }
    
    template <class DerivedType>
    static ObjectPtrType DefaultCreator(Args... args) {
        return ObjectPtrType(new DerivedType(args...));
    }
};

extern Registry<int, google::protobuf::Message* >* BaseMessageRegistry;

#define ANONYMOUS_VARIABLE_NAME1(message) message##__LINE__##1
#define ANONYMOUS_VARIABLE_NAME2(message) message##__LINE__##2
#define ANONYMOUS_VARIABLE_NAME3(message) message##__LINE__##3

#define SCNET_MESSAGE_REGISTRATION_WITH_RECV_CALLBACK(ns, typeInt, messageClassName, sessionClassName, callbackFunc) \
static Registerer<int, google::protobuf::Message* > ANONYMOUS_VARIABLE_NAME1(messageClassName)( \
                     BaseMessageRegistry, \
                     typeInt, \
                     Registerer<int, google::protobuf::Message* >::DefaultCreator<messageClassName>); \
static Registerer<int, google::protobuf::Message* > ANONYMOUS_VARIABLE_NAME2(messageClassName)( \
                     BaseMessageRegistry, \
                     #ns "." #messageClassName, \
                     typeInt); \
static Registerer<int, google::protobuf::Message* > ANONYMOUS_VARIABLE_NAME3(messageClassName)( \
                    BaseMessageRegistry, \
                    typeInt, \
                    new CallbackWrapper<sessionClassName, messageClassName>(callbackFunc))

#define SCNET_MESSAGE_REGISTRATION(ns, typeInt, messageClassName) \
static Registerer<int, google::protobuf::Message* > ANONYMOUS_VARIABLE_NAME1(messageClassName)( \
                    BaseMessageRegistry, \
                    typeInt, \
                    Registerer<int, google::protobuf::Message* >::DefaultCreator<messageClassName>); \
static Registerer<int, google::protobuf::Message* > ANONYMOUS_VARIABLE_NAME2(messageClassName)( \
                    BaseMessageRegistry, \
                    #ns "." #messageClassName, \
                    typeInt)

#endif
