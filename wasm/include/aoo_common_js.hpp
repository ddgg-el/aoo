#pragma once

#include "aoo.h"
#include "aoo_types.h"
#include <cstdint>
#include <emscripten/val.h>

static constexpr uint32_t kMsgRingSize = 64;
static constexpr int kMsgMaxBytes = 512;

static constexpr AooSocketFlags kAooSocketAnyFamily = (AooSocketFlags)(kAooSocketIPv4 | kAooSocketIPv6);

struct MsgSlot {
	int32_t sampleOffset;
	int32_t channel;
	int32_t type;
	int32_t size;
	uint8_t data[kMsgMaxBytes];
	int64_t dueSample;
	AooSockAddrStorage addr;
	AooAddrSize addrlen;
	AooId id;
};

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

inline bool ipToSockAddr(const std::string& ip, int port, AooSockAddrStorage& addr, AooAddrSize& len) {
	len = sizeof(addr);
	return aoo_ipEndpointToSockAddr(ip.c_str(), (AooUInt16)port, kAooSocketAnyFamily, &addr, &len) == kAooOk;
}