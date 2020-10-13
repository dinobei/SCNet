#include <google/protobuf/message.h>

#include "registry.h"

Registry<int, google::protobuf::Message *> *GetRegistry() {
	return Registry<int, google::protobuf::Message *>().Get();
}
