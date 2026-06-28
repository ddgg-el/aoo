#pragma once
#include "aoo.h"
#include "aoo_types.h"
#include <napi.h>


namespace aoo_node_util {

	void Register(Napi::Env env, Napi::Object exports);
	Napi::Object endpointToObject(Napi::Env env, const AooEndpoint& ep);
	bool toSocketAddr(const std::string& ip, AooUInt16 port, AooSockAddrStorage& storage, AooAddrSize& len);
	bool toEndpoint(Napi::Object obj, AooSockAddrStorage& storage, AooEndpoint& ep);
}