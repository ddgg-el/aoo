#pragma once
#include "aoo.h"
#include "aoo_events.h"
#include "aoo_types.h"
#include <napi.h>


namespace AooNodeUtils {

	void Register(Napi::Env env, Napi::Object exports);
	Napi::Object endpointToObject(Napi::Env env, const AooEndpoint& ep);
	bool toSocketAddr(const std::string& ip, AooUInt16 port, AooSockAddrStorage& storage, AooAddrSize& len);
	bool toEndpoint(Napi::Object obj, AooSockAddrStorage& storage, AooEndpoint& ep);
	const char* eventTypeName(AooEventType t);

	struct InPacket {
		std::vector<AooByte> data;
		std::string ip;
		uint16_t port;
	};

	struct PollCtx { 
		Napi::Env env; 
		Napi::Array arr;
		uint32_t n;
	};

	enum class AooClientRequestsType {
		Connect,
		JoinGroup,
		LeaveGroup,
		Disconnect, 
		JoinChain
	};

	struct CompletedRequest {
		AooId reqId;
		AooError error;
		AooId userId = kAooIdInvalid;
		AooId groupId = kAooIdInvalid;
	};
}