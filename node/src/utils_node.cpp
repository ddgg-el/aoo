#include "utils_node.hpp"
#include "aoo.h"
#include "aoo_client_node.hpp"
#include "aoo_types.h"
#include <string>

namespace {

	static Napi::String Version(const Napi::CallbackInfo& info) {
		return Napi::String::New(info.Env(), aoo_getVersionString());
	}
	
}

namespace aoo_node_util {

	void Register(Napi::Env env, Napi::Object exports) {
		exports.Set("aoo_version", Napi::Function::New(env, Version));
	}

	Napi::Object endpointToObject(Napi::Env env, const AooEndpoint &ep) {
		char ip[64];
		AooSize ipsize = sizeof(ip);
		AooUInt16 port = 0;
		aoo_sockAddrToIpEndpoint((ep.address), ep.addrlen, ip, &ipsize, &port, nullptr);
		Napi::Object o = Napi::Object::New(env);
		o.Set("ip", Napi::String::New(env, std::string(ip, ipsize)));
		o.Set("port", Napi::Number::New(env, port));
		o.Set("id", Napi::Number::New(env, ep.id));
		return o;

	}

}