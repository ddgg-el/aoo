#include "aoo_source_node.hpp"
#include "aoo.h"
#include "aoo_client_node.hpp"
#include "aoo_defines.h"
#include "aoo_events.h"
#include "aoo_types.h"
#include "codec/aoo_pcm.h"
#include "utils_node.hpp"
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
		InstanceMethod("send", &AooSourceWrap::Send),
		InstanceMethod("pollEvents", &AooSourceWrap::PollEvents),
		InstanceMethod("removeSink", &AooSourceWrap::RemoveSink)

	});
	exports.Set("AooSource", func);
}

AooSourceWrap::AooSourceWrap(const Napi::CallbackInfo& info)
: Napi::ObjectWrap<AooSourceWrap>(info)
{
	AooId id = info[0].As<Napi::Number>().Int32Value();
	source_ = AooSource::create(id);
	source_->setEventHandler(&AooSourceWrap::HandleEvent, this, kAooEventModePoll);
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

Napi::Value AooSourceWrap::PollEvents(const Napi::CallbackInfo& info)
{
	Napi::Env env = info.Env();
	Napi::Array arr = Napi::Array::New(env);
	PollCtx ctx { env, arr, 0};
	pollCtx_ = &ctx;
	source_->pollEvents();
	pollCtx_ = nullptr;
	return arr;
}

void AooSourceWrap::HandleEvent(void* user, const AooEvent* e, AooThreadLevel)
{
	auto* self = static_cast<AooSourceWrap*>(user);
	if(!self->pollCtx_) return;
	Napi::Env env = self->pollCtx_->env;
	Napi::Object o = Napi::Object::New(env);

	switch (e->type) {
	case kAooEventSinkPing: {
		auto& p = e->sinkPing;
		double rtt = aoo_ntpTimeToSeconds((p.t4 - p.t1) - (p.t3 - p.t2));
		o.Set("type", "sinkPing");
		o.Set("endpoint", aoo_node_util::endpointToObject(env, p.endpoint));
		o.Set("rtt", Napi::Number::New(env, rtt));
		o.Set("packetLoss", Napi::Number::New(env, p.packetLoss));
		break;
	}
	case kAooEventSinkAdd:
	case kAooEventSourceAdd:
		o.Set("type", e->type == kAooEventSinkAdd ? "sinkAdd" : "sinkRemove");
		o.Set("endpoint", aoo_node_util::endpointToObject(env, e->endpoint.endpoint));
		break;
	case kAooEventInvite:
		o.Set("type", "invite");
		o.Set("endpoint", aoo_node_util::endpointToObject(env, e->invite.endpoint));
		o.Set("token", Napi::Number::New(env, e->invite.token));
		break;
	case kAooEventUninvite:
		o.Set("type", "uninvite");
		o.Set("endpoint", aoo_node_util::endpointToObject(env, e->uninvite.endpoint));
		o.Set("token", Napi::Number::New(env, e->uninvite.token));
		break;
	default:
		o.Set("type", Napi::Number::New(env, (double)e->type));
		break;
	}
	self->pollCtx_->arr.Set(self->pollCtx_->n++, o);
}

Napi::Value AooSourceWrap::RemoveSink(const Napi::CallbackInfo& info)
{
	Napi::Object ep = info[0].As<Napi::Object>();
	std::string ip = ep.Get("ip").As<Napi::String>().Utf8Value();
	AooUInt16 port = (AooUInt16) ep.Get("port").As<Napi::Number>().Uint32Value();
	AooId id = ep.Get("id").As<Napi::Number>().Int32Value();

	AooSockAddrStorage addr;
	AooAddrSize addrlen = sizeof(addr);
	if(aoo_ipEndpointToSockAddr(ip.c_str(), port, kAooSocketDualStack, &addr, &addrlen) != kAooOk) {
		Napi::Error::New(info.Env(), "removeSink: bad address").ThrowAsJavaScriptException();
		return info.Env().Undefined();
	}
	AooEndpoint endpoint { &addr, addrlen, id };
	source_->removeSink(endpoint);
	return info.Env().Undefined();
}