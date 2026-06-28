#include "aoo_source_node.hpp"
#include "aoo.h"
#include "aoo_defines.h"
#include "codec/aoo_pcm.h"
#include <cstddef>

void AooSourceWrap::Register(Napi::Env env, Napi::Object exports)
{
	Napi::Function func = DefineClass(env, "AooSource", {
		InstanceMethod("setup", &AooSourceWrap::Setup),
		InstanceMethod("setFormat", &AooSourceWrap::SetFormat),
		InstanceMethod("addSink", &AooSourceWrap::AddSink),
		InstanceMethod("startStream", &AooSourceWrap::StartStream),
		InstanceMethod("stopStream", &AooSourceWrap::StopStream),
		InstanceMethod("process", &AooSourceWrap::Process),
		InstanceMethod("send", &AooSourceWrap::Send)
	});
	exports.Set("AooSource", func);
}

AooSourceWrap::AooSourceWrap(const Napi::CallbackInfo& info)
: Napi::ObjectWrap<AooSourceWrap>(info)
{
	AooId id = info[0].As<Napi::Number>().Int32Value();
	source_ = AooSource::create(id);
}

Napi::Value AooSourceWrap::Setup(const Napi::CallbackInfo& info)
{
	channels_ = info[0].As<Napi::Number>().Int32Value();
	sampleRate_ = info[1].As<Napi::Number>().DoubleValue();
	blockSize_ = info[2].As<Napi::Number>().Int32Value();

	planar_.resize(channels_);
	source_->setup(channels_, sampleRate_, blockSize_, 0);
	return info.Env().Undefined();
}

Napi::Value AooSourceWrap::SetFormat(const Napi::CallbackInfo& info)
{
	AooFormatPcm fmt;
	AooFormatPcm_init(&fmt, channels_, sampleRate_, blockSize_, kAooPcmFloat32);
	AooError err = source_->setFormat(fmt.header);
	if(err != kAooOk) {
		Napi::Error::New(info.Env(), std::string("setFormat: ") + aoo_strerror(err)).ThrowAsJavaScriptException();
	}
	return info.Env().Undefined();
}

Napi::Value AooSourceWrap::AddSink(const Napi::CallbackInfo& info)
{
	Napi::Object ep = info[0].As<Napi::Object>();
	std::string ip = ep.Get("ip").As<Napi::String>().Utf8Value();
	AooUInt16 port = (AooUInt16) ep.Get("port").As<Napi::Number>().Int32Value();
	AooId id = ep.Get("id").As<Napi::Number>().Int32Value();

	AooSockAddrStorage addr;
	AooAddrSize addrlen = sizeof(addr);
	if(aoo_ipEndpointToSockAddr(ip.c_str(), port, kAooSocketDualStack, &addr, &addrlen) != kAooOk) {
		Napi::Error::New(info.Env(), "addSink: bad address").ThrowAsJavaScriptException();
		return info.Env().Undefined();
	}
	AooEndpoint endpoint { &addr, addrlen, id };
	source_->addSink(endpoint, kAooTrue);
	return info.Env().Undefined();
}

Napi::Value AooSourceWrap::StartStream(const Napi::CallbackInfo& info)
{
	source_->startStream(0, nullptr);
	return info.Env().Undefined();
}

Napi::Value AooSourceWrap::StopStream(const Napi::CallbackInfo& info)
{
	source_->stopStream(0);
	return info.Env().Undefined();
}

Napi::Value AooSourceWrap::Process(const Napi::CallbackInfo& info)
{
	auto in = info[0].As<Napi::Float32Array>();
	int numSamples = (int)in.ElementLength() / channels_;
	// FIXME: blocking???
	deint_.resize((size_t)channels_ * numSamples);
	for (int ch = 0; ch < channels_; ++ch) {
		planar_[ch] = deint_.data() + (size_t) ch * numSamples;
		for (int i = 0; i < numSamples; ++i) {
			planar_[ch][i] = in[i * channels_ + ch];
		}
	}
	source_->process(planar_.data(), numSamples, aoo_getCurrentNtpTime());
	return info.Env().Undefined();
}

Napi::Value AooSourceWrap::Send(const Napi::CallbackInfo& info)
{
	Napi::Function cb = info[0].As<Napi::Function>();
	sendCb_ = &cb;
	source_->send(&AooSourceWrap::SendTrampoline, this);
	sendCb_ = nullptr;
	return info.Env().Undefined();
}
// TODO: ask why AOO_CALL
AooInt32 AOO_CALL AooSourceWrap::SendTrampoline(void* user, const AooByte* data, AooInt32 size, const void* address, AooAddrSize addrlen, AooFlag flags )
{
	auto* self = static_cast<AooSourceWrap*>(user);
	if(!self->sendCb_) return 0;
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