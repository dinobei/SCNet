#ifndef __REGISTRY_H__
#define __REGISTRY_H__
#include <functional>
#include <string>
#include <google/protobuf/message.h>

namespace ijoon {
    void initGlobalVariables();
}

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
    
    inline bool HasCreator(const SrcType& key) { return (registry_creater.count(key) != 0); }
    inline bool HasGetter(const std::string key) { return (registry_getter.count(key) != 0); }

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

private:
    std::map<SrcType, Creator> registry_creater;
    std::map<std::string, google::protobuf::uint32> registry_getter;
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
    
    template <class DerivedType>
    static ObjectPtrType DefaultCreator(Args... args) {
        return ObjectPtrType(new DerivedType(args...));
    }
};

extern Registry<int, google::protobuf::Message* >* BaseMessageRegistry;

#define ANONYMOUS_VARIABLE_NAME(message) message##__LINE__##1
#define ANONYMOUS_TYPE_NAME(message) message##__LINE__##2

#define IJN_REGISTER_MESSAGES(ns, typeInt, message) \
static Registerer<int, google::protobuf::Message* > ANONYMOUS_VARIABLE_NAME(message)( \
                     BaseMessageRegistry, \
                     typeInt, \
                     Registerer<int, google::protobuf::Message* >::DefaultCreator<message>); \
static Registerer<int, google::protobuf::Message* > ANONYMOUS_TYPE_NAME(message)( \
                     BaseMessageRegistry, \
                     #ns "." #message, \
                     typeInt)


#endif
