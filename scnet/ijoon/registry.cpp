#include "registry.h"
#include <google/protobuf/message.h>

Registry<int, google::protobuf::Message *> *GetRegistry() {
	return Registry<int, google::protobuf::Message *>().Get();
}