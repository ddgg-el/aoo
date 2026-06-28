#include "aoo_sink_node.hpp"
#include "aoo.h"
#include "aoo_types.h"
#include "utils_node.hpp"

void AooSinkWrap::Register(Napi::Env env, Napi::Object exports)
{
 Napi::Function func = DefineClass(env, "AooSink", {
		InstanceMethod("setup",         &AooSinkWrap::Setup),
		InstanceMethod("setLatency",    &AooSinkWrap::SetLatency),
		InstanceMethod("handleMessage", &AooSinkWrap::HandleMessage),
		InstanceMethod("process",       &AooSinkWrap::Process),
		InstanceMethod("send",          &AooSinkWrap::Send),
		InstanceMethod("pollEvents",    &AooSinkWrap::PollEvents)
	});
	exports.Set("AooSink", func);
}

AooSinkWrap::AooSinkWrap(const Napi::CallbackInfo& info)
: Napi::ObjectWrap<AooSinkWrap>(info)
{
	AooId id = info[0].As<Napi::Number>().Int32Value();
	sink_ = AooSink::create(id);
	sink_->setEventHandler(&AooSinkWrap::HandleEvent, this, kAooEventModePoll);
}

Napi::Value AooSinkWrap::Setup(const Napi::CallbackInfo& info)
{
	channels_   = info[0].As<Napi::Number>().Int32Value();
	sampleRate_ = info[1].As<Napi::Number>().DoubleValue();
	blockSize_  = info[2].As<Napi::Number>().Int32Value();
	planar_.resize(channels_);
	sink_->setup(channels_, sampleRate_, blockSize_, 0);
	return info.Env().Undefined();
}

Napi::Value AooSinkWrap::SetLatency(const Napi::CallbackInfo& info)
{
	sink_->setLatency(info[0].As<Napi::Number>().DoubleValue());
	return info.Env().Undefined();
}

Napi::Value AooSinkWrap::HandleMessage(const Napi::CallbackInfo& info)
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
	sink_->handleMessage((const AooByte*)bytes.Data(), (AooInt32)bytes.Length(), &addr, addrlen);
	return info.Env().Undefined();
}

Napi::Value AooSinkWrap::Process(const Napi::CallbackInfo& info)
{
	Napi::Env env = info.Env();
	int n = blockSize_;
	out_.assign((size_t)channels_ * n, 0.0f);
	for (int ch = 0; ch < channels_; ++ch) {
		planar_[ch] = out_.data() + (size_t)ch * n;
	}

	sink_->process(planar_.data(), n, aoo_getCurrentNtpTime(), nullptr, nullptr);

	auto result = Napi::Float32Array::New(env, (size_t)channels_ * n);
	for (int ch = 0; ch < channels_; ++ch) {
		for (int i = 0; i < n; ++i) {
			result[i * channels_ + ch] = planar_[ch][i];   // planar -> interleaved
		}
	}
	return result;
}

Napi::Value AooSinkWrap::Send(const Napi::CallbackInfo& info)
{
	Napi::Function cb = info[0].As<Napi::Function>();
	sendCb_ = &cb;
	sink_->send(&AooSinkWrap::SendTrampoline, this);
	sendCb_ = nullptr;
	return info.Env().Undefined();
}

AooInt32 AOO_CALL AooSinkWrap::SendTrampoline(void* user, const AooByte* data, AooInt32 size, const void* address, AooAddrSize addrlen, AooFlag flags)
{
	auto* self = static_cast<AooSinkWrap*>(user);
	if (!self->sendCb_) return 0;
	Napi::Env env = self->sendCb_->Env();
	char ip[64]; AooSize ipsize = sizeof(ip); AooUInt16 port = 0;
	aoo_sockAddrToIpEndpoint(address, addrlen, ip, &ipsize, &port, nullptr);
	self->sendCb_->Call({
		Napi::Buffer<uint8_t>::Copy(env, data, size),
		Napi::String::New(env, std::string(ip, ipsize)),
		Napi::Number::New(env, port)
	});
	return size;
}

Napi::Value AooSinkWrap::PollEvents(const Napi::CallbackInfo& info) {
	Napi::Env env = info.Env();
	Napi::Array arr = Napi::Array::New(env);
	PollCtx ctx { env, arr, 0 };
	pollCtx_ = &ctx;
	sink_->pollEvents();
	pollCtx_ = nullptr;
	return arr;
}

void AooSinkWrap::HandleEvent(void* user, const AooEvent* e, AooThreadLevel) {
	auto* self = static_cast<AooSinkWrap*>(user);
	if (!self->pollCtx_) return;
	Napi::Env env = self->pollCtx_->env;
	Napi::Object o = Napi::Object::New(env);

	switch (e->type) {
	case kAooEventSourcePing: {
		auto& p = e->sourcePing;
		double rtt = aoo_ntpTimeToSeconds((p.t4 - p.t1) - (p.t3 - p.t2));
		o.Set("type", "sourcePing");
		o.Set("endpoint", aoo_node_util::endpointToObject(env, p.endpoint));
		o.Set("rtt", Napi::Number::New(env, rtt));
		break;
	}
	case kAooEventSourceAdd:
	case kAooEventSourceRemove:
		o.Set("type", e->type == kAooEventSourceAdd ? "sourceAdd" : "sourceRemove");
		o.Set("endpoint", aoo_node_util::endpointToObject(env, e->endpoint.endpoint));
		break;
	case kAooEventStreamStart:
		o.Set("type", "streamStart");
		o.Set("endpoint", aoo_node_util::endpointToObject(env, e->streamStart.endpoint));
		break;
	case kAooEventStreamStop:
		o.Set("type", "streamStop");
		o.Set("endpoint", aoo_node_util::endpointToObject(env, e->endpoint.endpoint));
		break;
	case kAooEventStreamState: {
		auto& p = e->streamState;
		const char* st = p.state == kAooStreamStateActive ? "active"
		               : p.state == kAooStreamStateBuffering ? "buffering" : "inactive";
		o.Set("type", "streamState");
		o.Set("endpoint", aoo_node_util::endpointToObject(env, p.endpoint));
		o.Set("state", Napi::String::New(env, st));
		break;
	}
	case kAooEventFormatChange: {
		auto& p = e->formatChange;
		o.Set("type", "formatChange");
		o.Set("endpoint", aoo_node_util::endpointToObject(env, p.endpoint));
		if (p.format) {
			o.Set("codec", Napi::String::New(env, p.format->codecName));
			o.Set("channels", Napi::Number::New(env, p.format->numChannels));
			o.Set("sampleRate", Napi::Number::New(env, p.format->sampleRate));
			o.Set("blockSize", Napi::Number::New(env, p.format->blockSize));
		}
		break;
	}
	default:
		o.Set("type", Napi::Number::New(env, (double)e->type));
		break;
	}
	self->pollCtx_->arr.Set(self->pollCtx_->n++, o);
}
