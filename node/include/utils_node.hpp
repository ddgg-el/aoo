#pragma once
#include "aoo_types.h"
#include <napi.h>


namespace aoo_node_util {

	void Register(Napi::Env env, Napi::Object exports);
	Napi::Object endpointToObject(Napi::Env env, const AooEndpoint& ep);
	
}