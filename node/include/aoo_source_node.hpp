#pragma once

#include <napi.h>
#include "aoo_client_node.hpp"
#include "aoo_source.hpp"
#include "aoo_types.h"
#include <vector>

class AooSourceWrap : public Napi::ObjectWrap<AooSourceWrap> {
public:
	static void Register(Napi::Env env, Napi::Object exports);
	AooSourceWrap(const Napi::CallbackInfo& info);
	AooSource* native() const { return source_.get(); };

private:
	AooSource::Ptr source_;
	int channels_ = 1;
	double sampleRate_ = 48000;
	int blockSize_ = 256;
	std::vector<AooSample> deint_;
	std::vector<AooSample*> planar_;
	Napi::Function* sendCb_ = nullptr;

	Napi::Value Setup(const Napi::CallbackInfo& info);
	Napi::Value SetFormat(const Napi::CallbackInfo& info);
	Napi::Value AddSink(const Napi::CallbackInfo& info);
	Napi::Value StartStream(const Napi::CallbackInfo& info);
	Napi::Value StopStream(const Napi::CallbackInfo& info);
	Napi::Value Process(const Napi::CallbackInfo& info);
	Napi::Value Send(const Napi::CallbackInfo& info);

	static AooInt32 AOO_CALL SendTrampoline(void* user, const AooByte* data, AooInt32 size, const void* address, AooAddrSize addrlen, AooFlag flags );
};