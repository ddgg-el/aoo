#pragma once
#include "aoo_controls.h"
#include "aoo_types.h"
#include <cstddef>
#include <cstdint>
#include <napi.h>
#include <vector>
#include "aoo.h"


template <class Derived, class AooT>
class StreamEndpointWrap: public Napi::ObjectWrap<Derived> {
public:
	StreamEndpointWrap(const Napi::CallbackInfo& info)
	:Napi::ObjectWrap<Derived>(info){}

	Napi::Value Setup(const Napi::CallbackInfo& info)
	{
		channels_ = info[0].As<Napi::Number>().Int32Value();
		sampleRate_ = info[1].As<Napi::Number>().DoubleValue();
		blockSize_ = info[2].As<Napi::Number>().Int32Value();

		planar_.resize(channels_);
		aoo()->setup(channels_, sampleRate_, blockSize_, 0);
		return info.Env().Undefined();
	}

	Napi::Value Send(const Napi::CallbackInfo& info) 
	{
		Napi::Function cb = info[0].As<Napi::Function>();
		sendCb_ = &cb;
		aoo()->send(&StreamEndpointWrap::SendTrampoline, this);
		sendCb_ = nullptr;
		return info.Env().Undefined();
	}

	Napi::Value HandleMessage(const Napi::CallbackInfo& info)
	{
		auto bytes = info[0].As<Napi::Buffer<uint8_t>>();
		std::string ip = info[1].As<Napi::String>().Utf8Value();
		AooUInt16 port = (AooUInt16) info[2].As<Napi::Number>().Uint32Value();

		AooSockAddrStorage addr;
		AooAddrSize addrlen = sizeof(addr);
		if (aoo_ipEndpointToSockAddr(ip.c_str(), port, kAooSocketDualStack, &addr, &addrlen) != kAooOk) {
			Napi::Error::New(info.Env(), "handleMessage: bad address").ThrowAsJavaScriptException();
			return info.Env().Undefined();
		}
		aoo()->handleMessage((const AooByte*)bytes.Data(), (AooInt32)bytes.Length(), &addr, addrlen);
		return info.Env().Undefined();
	}

	Napi::Value PollEvents(const Napi::CallbackInfo& info) 
	{
		Napi::Env env = info.Env();
		Napi::Array arr = Napi::Array::New(env);
		PollCtx ctx { env, arr, 0 };
		pollCtx_ = &ctx;
		aoo()->pollEvents();
		pollCtx_ = nullptr;
		return arr;
	}

	Napi::Value EventsAvailable(const Napi::CallbackInfo& info) 
	{
		return Napi::Boolean::New(info.Env(), aoo()->eventsAvailable() == kAooTrue);
	}

	Napi::Value Reset (const Napi::CallbackInfo& info) 
	{
		aoo()->reset();
		return info.Env().Undefined();
	}

	Napi::Value SetId(const Napi::CallbackInfo& info) 
	{
		aoo()->setId(info[0].As<Napi::Number>().Int32Value());
		return info.Env().Undefined();
	}

	Napi::Value SetBufferSize(const Napi::CallbackInfo& info) 
	{
		aoo()->setBufferSize(info[0].As<Napi::Number>().DoubleValue());
		return info.Env().Undefined();
	}

	Napi::Value SetPacketSize(const Napi::CallbackInfo& info) 
	{
		aoo()->setPacketSize(info[0].As<Napi::Number>().Int32Value());
		return info.Env().Undefined();
	}

	Napi::Value SetPingInterval(const Napi::CallbackInfo& info) 
	{
		aoo()->setPingInterval(info[0].As<Napi::Number>().DoubleValue());
		return info.Env().Undefined();
	}

	Napi::Value SetResampleMethod(const Napi::CallbackInfo& info) 
	{
		aoo()->setResampleMethod(info[0].As<Napi::Number>().Int32Value());
		return info.Env().Undefined();
	}

	Napi::Value SetDynamicResampling(const Napi::CallbackInfo& info) 
	{
		aoo()->setDynamicResampling(info[0].As<Napi::Boolean>().Value() ? kAooTrue : kAooFalse);
		return info.Env().Undefined();
	}

	Napi::Value SetBinaryFormat(const Napi::CallbackInfo& info) 
	{
		aoo()->setBinaryFormat(info[0].As<Napi::Boolean>().Value() ? kAooTrue : kAooFalse);
		return info.Env().Undefined();
	}

	Napi::Value SetDllBandwidth(const Napi::CallbackInfo& info) 
	{
		float q = (float) info[0].As<Napi::Number>().DoubleValue();
		aoo()->control(kAooCtlSetDllBandwidth, 0, &q, sizeof(q));
		return info.Env().Undefined();
	}

	Napi::Value GetRealSampleRate(const Napi::CallbackInfo& info) 
	{
		AooSampleRate samplerate = 0;
		aoo()->getRealSampleRate(samplerate);
		return Napi::Number::New(info.Env(), samplerate);
	}


protected:
	AooT* aoo() { return static_cast<Derived*>(this)->native(); }
	int channels_ = 1;
	double sampleRate_ = 48000.0;
	int blockSize_ = 256;
	std::vector<AooSample> deint_;
	std::vector<AooSample*> planar_;

	Napi::Function* sendCb_ = nullptr;

	struct PollCtx {
		Napi::Env env;
		Napi::Array arr;
		uint32_t n;
	};
	PollCtx* pollCtx_ = nullptr;
	// TODO: ask why AOO_CALL
	static AooInt32 AOO_CALL SendTrampoline(void* user, const AooByte* data, AooInt32 size, const void* address, AooAddrSize addrlen, AooFlag flags)
	{
		auto* self = static_cast<StreamEndpointWrap*>(user);
		if (!self->sendCb_) return 0;
		Napi::Env env = self->sendCb_->Env();
		char ip[64]; 
		AooSize ipsize = sizeof(ip); 
		AooUInt16 port = 0;
		aoo_sockAddrToIpEndpoint(address, addrlen, ip, &ipsize, &port, nullptr);
		self->sendCb_->Call({
			Napi::Buffer<uint8_t>::Copy(env, data, size),
			Napi::String::New(env, std::string(ip, ipsize)),
			Napi::Number::New(env, port)
		});
		return size;
	}
};