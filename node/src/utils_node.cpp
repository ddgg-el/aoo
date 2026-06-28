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

	bool toSockAddr(const std::string& ip, AooUInt16 port, AooSockAddrStorage& storage, AooAddrSize& len) {
		len = sizeof(AooSockAddrStorage);
		return aoo_ipEndpointToSockAddr(ip.c_str(), port, kAooSocketDualStack, &storage, &len) == kAooOk;
	}

	bool toEndpoint(Napi::Object obj, AooSockAddrStorage& storage, AooEndpoint& ep) {
		std::string ip = obj.Get("ip").As<Napi::String>().Utf8Value();
		AooUInt16 port = (AooUInt16) obj.Get("port").As<Napi::Number>().Uint32Value();
		AooId id       = obj.Get("id").As<Napi::Number>().Int32Value();
		AooAddrSize len = 0;
		if (!toSockAddr(ip, port, storage, len)) return false;
		ep = AooEndpoint{ &storage, len, id };
		return true;
	}

}