#include <google/protobuf/message.h>

#include "registry.h"

Registry<google::protobuf::Message *> *GetRegistry() {
	return Registry<google::protobuf::Message *>().Get();
}
