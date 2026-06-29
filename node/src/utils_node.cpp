#include "utils_node.hpp"
#include "aoo.h"
#include "aoo_client_node.hpp"
#include "aoo_endpoint_wrap.hpp"
#include "aoo_events.h"
#include "aoo_types.h"
#include <string>
#include <utility>
#include "codec/aoo_opus.h"

namespace {

	

	static const std::pair<const char*, int> kEventTypes[] = {
		{"error", kAooEventError},
		{"sinkPing", kAooEventSinkPing}, {"sourcePing", kAooEventSourcePing},
		{"invite", kAooEventInvite}, {"uninvite", kAooEventUninvite},
		{"sinkAdd", kAooEventSinkAdd}, {"sinkRemove", kAooEventSinkRemove},
		{"sourceAdd", kAooEventSourceAdd}, {"sourceRemove", kAooEventSourceRemove},
		{"streamStart", kAooEventStreamStart}, {"streamStop", kAooEventStreamStop},
		{"streamState", kAooEventStreamState}, {"streamTime", kAooEventStreamTime},
		{"streamLatency", kAooEventStreamLatency}, {"formatChange", kAooEventFormatChange},
		{"inviteDecline", kAooEventInviteDecline}, {"inviteTimeout", kAooEventInviteTimeout},
		{"uninviteTimeout", kAooEventUninviteTimeout},
		{"bufferOverrun", kAooEventBufferOverrun}, {"bufferUnderrun", kAooEventBufferUnderrun},
		{"blockDrop", kAooEventBlockDrop}, {"blockResend", kAooEventBlockResend},
		{"blockXRun", kAooEventBlockXRun}, {"frameResend", kAooEventFrameResend},
		{"disconnect", kAooEventDisconnect}, {"notification", kAooEventNotification},
		{"groupEject", kAooEventGroupEject},
		{"peerPing", kAooEventPeerPing}, {"peerState", kAooEventPeerState},
		{"peerHandshake", kAooEventPeerHandshake}, {"peerTimeout", kAooEventPeerTimeout},
		{"peerJoin", kAooEventPeerJoin}, {"peerLeave", kAooEventPeerLeave},
		{"peerMessage", kAooEventPeerMessage}, {"peerUpdate", kAooEventPeerUpdate},
		{"groupUpdate", kAooEventGroupUpdate}, {"userUpdate", kAooEventUserUpdate},
		{"clientLogin", kAooEventClientLogin}, {"clientLogout", kAooEventClientLogout},
		{"clientError", kAooEventClientError},
		{"groupAdd", kAooEventGroupAdd}, {"groupRemove", kAooEventGroupRemove},
		{"groupJoin", kAooEventGroupJoin}, {"groupLeave", kAooEventGroupLeave},
	};

	static Napi::String Version(const Napi::CallbackInfo& info) {
		return Napi::String::New(info.Env(), aoo_getVersionString());
	}

	static Napi::Value StrError(const Napi::CallbackInfo& info) {
		return Napi::String::New(info.Env(), aoo_strerror(info[0].As<Napi::Number>().Int32Value()));
	}

	static Napi::Value Terminate(const Napi::CallbackInfo& info) {
		aoo_terminate(); return info.Env().Undefined();
	}
	
	static Napi::Value MessageType(const Napi::CallbackInfo& info) {
		auto b = info[0].As<Napi::Buffer<uint8_t>>();
		AooMsgType type; AooId id; AooInt32 offset;
		if (aoo_parsePattern((const AooByte*)b.Data(), (AooInt32)b.Length(), &type, &id, &offset) != kAooOk)
			return Napi::Number::New(info.Env(), -1);
		return Napi::Number::New(info.Env(), (double)type);
	}
	
	static Napi::Object enumObj(Napi::Env env, std::initializer_list<std::pair<const char*, int>> kv) {
		auto o = Napi::Object::New(env);
		for (auto& [k, v] : kv) o.Set(k, Napi::Number::New(env, v));
		return o;
	}
	
}

namespace aoo_node_util {

	void Register(Napi::Env env, Napi::Object exports) {
		exports.Set("aoo_version",   Napi::Function::New(env, Version));
		exports.Set("aoo_strerror",  Napi::Function::New(env, StrError));
		exports.Set("aoo_terminate", Napi::Function::New(env, Terminate));
		exports.Set("messageType",   Napi::Function::New(env, MessageType));

		auto evt = Napi::Object::New(env);
		for (auto& [name, val] : kEventTypes) {
			evt.Set(name, Napi::Number::New(env, val));
		}
		exports.Set("AooEventType", evt);

		exports.Set("AooResampleMethod", enumObj(env, {
			{"hold", kAooResampleHold}, {"linear", kAooResampleLinear}, {"cubic", kAooResampleCubic} }));
		exports.Set("AooMsgType", enumObj(env, {
			{"source", kAooMsgTypeSource}, {"sink", kAooMsgTypeSink} }));
		exports.Set("AooDataType", enumObj(env, {
			{"raw", kAooDataRaw}, {"text", kAooDataText}, {"osc", kAooDataOSC},
			{"midi", kAooDataMIDI}, {"json", kAooDataJSON} }));
		exports.Set("AooOpusApplication", enumObj(env, {
			{"audio", OPUS_APPLICATION_AUDIO}, {"lowdelay", OPUS_APPLICATION_RESTRICTED_LOWDELAY},
			{"voip", OPUS_APPLICATION_VOIP} }));
		exports.Set("AooOpusSignalType", enumObj(env, {
			{"music", OPUS_SIGNAL_MUSIC}, {"voice", OPUS_SIGNAL_VOICE}, {"auto", OPUS_AUTO} }));
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

	const char* eventTypeName(AooEventType t) {
		for (auto& [name, val] : kEventTypes) {
			if(val == (int)t) return name;
		}
		return "unknown";
	}
}