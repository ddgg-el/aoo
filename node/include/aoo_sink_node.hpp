#pragma once

#include "aoo_sink.hpp"
#include "aoo_types.h"
#include <napi.h>
#include <vector>

class AooSinkWrap : public Napi::ObjectWrap<AooSinkWrap> {
public:
	static void Register(Napi::Env env, Napi::Object exports);
	AooSinkWrap(const Napi::CallbackInfo& info);
	AooSink* native() const { return sink_.get(); };

private:
	Napi::Value Setup(const Napi::CallbackInfo& info);	
	Napi::Value SetLatency(const Napi::CallbackInfo& info);
	Napi::Value HandleMessage(const Napi::CallbackInfo& info);
	Napi::Value Process(const Napi::CallbackInfo& info);
	Napi::Value Send(const Napi::CallbackInfo& info);
	Napi::Value PollEvents(const Napi::CallbackInfo& info);

	static AooInt32 AOO_CALL SendTrampoline(void* user, const AooByte* data, AooInt32 size, const void* address, AooAddrSize addrlen, AooFlag flags);
	static void HandleEvent(void* user, const AooEvent* e, AooThreadLevel);

	struct PollCtx { 
		Napi::Env env; 
		Napi::Array arr; 
		uint32_t n;
	};

	PollCtx* pollCtx_ = nullptr;
	AooSink::Ptr sink_;
	int channels_ = 1;
	double sampleRate_ = 48000.0;
	int blockSize_ = 256;
	std::vector<AooSample> out_;
	std::vector<AooSample*> planar_;
	Napi::Function* sendCb_ = nullptr;
};