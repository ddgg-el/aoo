#include <napi.h>
#include "aoo_client_node.hpp"
#include "aoo_source_node.hpp"
#include "aoo_sink_node.hpp"
#include "aoo_server_node.hpp"
#include "aoo_utils_node.hpp"

static Napi::Object Init(Napi::Env env, Napi::Object exports) {
	aoo_initialize(nullptr);
	AooNodeUtils::Register(env,exports);
	AooClientWrap::Register(env, exports);
	AooSourceWrap::Register(env, exports);
	AooSinkWrap::Register(env, exports);
	AooServerWrap::Register(env, exports);
	
	return exports;
}

NODE_API_MODULE(aoo_native, Init)