#include "registry.h"
#include <google/protobuf/message.h>

Registry<int, google::protobuf::Message* >* BaseMessageRegistry;
void ijoon::initGlobalVariables() {
    BaseMessageRegistry = new Registry<int, google::protobuf::Message* >();
}
