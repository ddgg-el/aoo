#pragma once

#include "aoo_endpoint_wrap.hpp"
#include "aoo_sink.hpp"
#include "aoo_types.h"
#include <cstdint>
#include <vector>

class AooSinkWrap : public StreamEndpointWrap<AooSinkWrap, AooSink> {
public:
	static void Register(Napi::Env env, Napi::Object exports);
	AooSinkWrap(const Napi::CallbackInfo& info);
	AooSink* native() const { return sink_.get(); };

private:
	Napi::Value SetLatency(const Napi::CallbackInfo& info);
	Napi::Value Process(const Napi::CallbackInfo& info);
	Napi::Value InviteSource(const Napi::CallbackInfo& info);

	static void HandleEvent(void* user, const AooEvent* e, AooThreadLevel);

	Napi::Value SetResendData(const Napi::CallbackInfo& info);
	Napi::Value SetResendInterval(const Napi::CallbackInfo& info);
	Napi::Value SetResendLimit(const Napi::CallbackInfo& info);
	Napi::Value UninviteSource(const Napi::CallbackInfo& info);
	Napi::Value UninviteAll(const Napi::CallbackInfo& info);
	Napi::Value ResetSource(const Napi::CallbackInfo& info);
	Napi::Value GetBufferFillRatio(const Napi::CallbackInfo& info);

	Napi::Value PollStreamMessages(const Napi::CallbackInfo& info);
	Napi::Value Delete(const Napi::CallbackInfo& info);

	static void AOO_CALL StreamMsgTrampoline(void* user, const AooStreamMessage* m, const AooEndpoint* src);

	struct StreamMsg {
		int sampleOffset;
		int channel;
		int type;
		std::vector<AooByte> data;
		std::string ip;
		uint16_t port;
		AooId id;
	};
	std::vector<StreamMsg> streamMsgs_;

	AooSink::Ptr sink_;
};