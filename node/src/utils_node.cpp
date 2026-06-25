#include "utils_node.hpp"
#include "aoo.h"

namespace {

	static Napi::String Version(const Napi::CallbackInfo& info) {
		return Napi::String::New(info.Env(), aoo_getVersionString());
	}
	
}

namespace aoo_node_util {

	void Register(Napi::Env env, Napi::Object exports) {
		exports.Set("version", Napi::Function::New(env, Version));
	}

}