#pragma once

#include "aoo_endpoint_wrap.hpp"
#include "aoo_source.hpp"

class AooSourceWrap : public StreamEndpointWrap<AooSourceWrap, AooSource> {
public:
	static void Register(Napi::Env env, Napi::Object exports);
	AooSourceWrap(const Napi::CallbackInfo& info);
	AooSource* native() const { return source_.get(); };

private:
	AooSource::Ptr source_;

	Napi::Value AddSink(const Napi::CallbackInfo& info);
	Napi::Value RemoveSink(const Napi::CallbackInfo& info);
	Napi::Value StartStream(const Napi::CallbackInfo& info);
	Napi::Value StopStream(const Napi::CallbackInfo& info);
	Napi::Value Process(const Napi::CallbackInfo& info);
	Napi::Value HandleInvite(const Napi::CallbackInfo& info);
	Napi::Value HandleUninvite(const Napi::CallbackInfo& info);
	
	static void HandleEvent(void* user, const AooEvent* e, AooThreadLevel);

	Napi::Value SetRedundancy(const Napi::CallbackInfo& info);
	Napi::Value SetResendBufferSize(const Napi::CallbackInfo& info);
	Napi::Value SetStreamTimeSendInterval(const Napi::CallbackInfo& info);
	Napi::Value RemoveAllSinks(const Napi::CallbackInfo& info);
	Napi::Value Activate(const Napi::CallbackInfo& info);
	Napi::Value SetSinkChannelOffset(const Napi::CallbackInfo& info);

	Napi::Value AddStreamMessage(const Napi::CallbackInfo& info);

	Napi::Value SetFormat(const Napi::CallbackInfo& info);
	Napi::Value SetOpusBitrate(const Napi::CallbackInfo& info);
	Napi::Value SetOpusComplexity(const Napi::CallbackInfo& info);
	Napi::Value SetOpusSignalType(const Napi::CallbackInfo& info);

	Napi::Value Delete(const Napi::CallbackInfo& info);
};