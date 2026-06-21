#pragma once

#include "aoo.h"
#include "aoo_types.h"
#include <emscripten/val.h>

inline emscripten::val endPointToVal(const AooEndpoint& ep) {
	char ipbuf[64];
	AooSize ipsize = sizeof(ipbuf);
	AooUInt16 port = 0;
	aoo_sockAddrToIpEndpoint(ep.address, ep.addrlen, ipbuf, &ipsize, &port, nullptr);
	auto o = emscripten::val::object();
	o.set("ip", std::string(ipbuf, ipsize));
	o.set("port", (int)port);
	o.set("id", ep.id);
	return o;
}

static constexpr AooSocketFlags kAooSocketAnyFamily = (AooSocketFlags)(kAooSocketIPv4 | kAooSocketIPv6);