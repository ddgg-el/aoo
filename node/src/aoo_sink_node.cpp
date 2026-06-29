#include "aoo_sink_node.hpp"
#include "aoo.h"
#include "aoo_client_node.hpp"
#include "aoo_defines.h"
#include "aoo_endpoint_wrap.hpp"
#include "aoo_types.h"
#include "utils_node.hpp"
#include <cstdint>
#include <string>
#include <vector>

void AooSinkWrap::Register(Napi::Env env, Napi::Object exports)
{
 Napi::Function func = DefineClass(env, "AooSink", {
		InstanceMethod("setup", &AooSinkWrap::Setup),
		InstanceMethod("send", &AooSinkWrap::Send),
		InstanceMethod("handleMessage", &AooSinkWrap::HandleMessage),
		InstanceMethod("pollEvents", &AooSinkWrap::PollEvents),
		InstanceMethod("eventsAvailable", &AooSinkWrap::EventsAvailable),
		InstanceMethod("reset",  &AooSinkWrap::Reset),
		InstanceMethod("setId", &AooSinkWrap::SetId),
		InstanceMethod("setBufferSize", &AooSinkWrap::SetBufferSize),
		InstanceMethod("setPacketSize", &AooSinkWrap::SetPacketSize),
		InstanceMethod("setPingInterval", &AooSinkWrap::SetPingInterval),
		InstanceMethod("setResampleMethod", &AooSinkWrap::SetResampleMethod),
		InstanceMethod("setDynamicResampling", &AooSinkWrap::SetDynamicResampling),
		InstanceMethod("setBinaryFormat", &AooSinkWrap::SetBinaryFormat),
		InstanceMethod("setDllBandwidth", &AooSinkWrap::SetDllBandwidth),
		InstanceMethod("getRealSampleRate", &AooSinkWrap::GetRealSampleRate),
		
		InstanceMethod("setLatency", &AooSinkWrap::SetLatency),
		InstanceMethod("process", &AooSinkWrap::Process),
		InstanceMethod("inviteSource", &AooSinkWrap::InviteSource),

		InstanceMethod("setResendData",      &AooSinkWrap::SetResendData),
		InstanceMethod("setResendInterval",  &AooSinkWrap::SetResendInterval),
		InstanceMethod("setResendLimit",     &AooSinkWrap::SetResendLimit),
		InstanceMethod("uninviteSource",     &AooSinkWrap::UninviteSource),
		InstanceMethod("uninviteAll",        &AooSinkWrap::UninviteAll),
		InstanceMethod("resetSource",        &AooSinkWrap::ResetSource),
		InstanceMethod("getBufferFillRatio", &AooSinkWrap::GetBufferFillRatio),
		InstanceMethod("pollStreamMessages", &AooSinkWrap::PollStreamMessages),
		InstanceMethod("delete", &AooSinkWrap::Delete)
	});
	exports.Set("AooSink", func);
}

AooSinkWrap::AooSinkWrap(const Napi::CallbackInfo& info)
: StreamEndpointWrap(info)
{
	AooId id = info[0].As<Napi::Number>().Int32Value();
	sink_ = AooSink::create(id);
	sink_->setEventHandler(&AooSinkWrap::HandleEvent, this, kAooEventModePoll);
}

Napi::Value AooSinkWrap::SetLatency(const Napi::CallbackInfo& info)
{
	sink_->setLatency(info[0].As<Napi::Number>().DoubleValue());
	return info.Env().Undefined();
}

Napi::Value AooSinkWrap::Process(const Napi::CallbackInfo& info)
{
	Napi::Env env = info.Env();
	int n = blockSize_;
	deint_.assign((size_t)channels_ * n, 0.0f);
	for (int ch = 0; ch < channels_; ++ch) {
		planar_[ch] = deint_.data() + (size_t)ch * n;
	}

	sink_->process(planar_.data(), n, aoo_getCurrentNtpTime(), &AooSinkWrap::StreamMsgTrampoline, this);

	auto result = Napi::Float32Array::New(env, (size_t)channels_ * n);
	for (int ch = 0; ch < channels_; ++ch) {
		for (int i = 0; i < n; ++i) {
			result[i * channels_ + ch] = planar_[ch][i];   // planar -> interleaved
		}
	}
	return result;
}

Napi::Value AooSinkWrap::InviteSource(const Napi::CallbackInfo& info)
{
	Napi::Object ep = info[0].As<Napi::Object>();
	std::string ip = ep.Get("ip").As<Napi::String>().Utf8Value();
	AooUInt16 port = (AooUInt16)ep.Get("port").As<Napi::Number>().Uint32Value();
	AooId id = ep.Get("id").As<Napi::Number>().Uint32Value();
	AooSockAddrStorage addr;
	AooAddrSize addrlen = sizeof(addr);
	if(aoo_ipEndpointToSockAddr(ip.c_str(), port, kAooSocketDualStack, &addr, &addrlen) != kAooOk) {
		Napi::Error::New(info.Env(), "inviteSouce: bad address").ThrowAsJavaScriptException();
		return info.Env().Undefined();
	}
	AooEndpoint endpoint { &addr, addrlen, id};
	sink_->inviteSource(endpoint, nullptr);
	return info.Env().Undefined();
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
		o.Set("type", Napi::String::New(env, aoo_node_util::eventTypeName(e->type)));
		break;
	}
	self->pollCtx_->arr.Set(self->pollCtx_->n++, o);
}

Napi::Value AooSinkWrap::SetResendData(const Napi::CallbackInfo& info) {
	sink_->setResendData(info[0].As<Napi::Boolean>().Value() ? kAooTrue : kAooFalse);
	return info.Env().Undefined();
}
Napi::Value AooSinkWrap::SetResendInterval(const Napi::CallbackInfo& info) {
	sink_->setResendInterval(info[0].As<Napi::Number>().DoubleValue());
	return info.Env().Undefined();
}
Napi::Value AooSinkWrap::SetResendLimit(const Napi::CallbackInfo& info) {
	sink_->setResendLimit(info[0].As<Napi::Number>().Int32Value());
	return info.Env().Undefined();
}
Napi::Value AooSinkWrap::UninviteAll(const Napi::CallbackInfo& info) {
	sink_->uninviteAll();
	return info.Env().Undefined();
}
Napi::Value AooSinkWrap::UninviteSource(const Napi::CallbackInfo& info) {
	AooSockAddrStorage st; AooEndpoint ep;
	if (!aoo_node_util::toEndpoint(info[0].As<Napi::Object>(), st, ep)) {
		Napi::Error::New(info.Env(), "uninviteSource: bad address").ThrowAsJavaScriptException();
		return info.Env().Undefined();
	}
	sink_->uninviteSource(ep);
	return info.Env().Undefined();
}
Napi::Value AooSinkWrap::ResetSource(const Napi::CallbackInfo& info) {
	AooSockAddrStorage st; AooEndpoint ep;
	if (!aoo_node_util::toEndpoint(info[0].As<Napi::Object>(), st, ep)) {
		Napi::Error::New(info.Env(), "resetSource: bad address").ThrowAsJavaScriptException();
		return info.Env().Undefined();
	}
	sink_->resetSource(ep);
	return info.Env().Undefined();
}
Napi::Value AooSinkWrap::GetBufferFillRatio(const Napi::CallbackInfo& info) {
	AooSockAddrStorage st; AooEndpoint ep;
	if (!aoo_node_util::toEndpoint(info[0].As<Napi::Object>(), st, ep)) {
		Napi::Error::New(info.Env(), "getBufferFillRatio: bad address").ThrowAsJavaScriptException();
		return info.Env().Undefined();
	}
	double ratio = 0;
	sink_->getBufferFillRatio(ep, ratio);
	return Napi::Number::New(info.Env(), ratio);
}

void AOO_CALL AooSinkWrap::StreamMsgTrampoline(void* user, const AooStreamMessage* m, const AooEndpoint* src)
{
	auto* self = static_cast<AooSinkWrap*>(user);
	char ip[64];
	AooSize ipsize = sizeof(ip);
	AooUInt16 port = 0;
	aoo_sockAddrToIpEndpoint(src->address, src->addrlen, ip, &ipsize, &port, nullptr);
	self->streamMsgs_.push_back({
		m->sampleOffset, 
		m->channel, 
		(int)m->type, 
		std::vector<AooByte>(m->data, m->data + m->size),
		std::string(ip, ipsize), 
		port, 
		src->id
	});
}

Napi::Value AooSinkWrap::PollStreamMessages(const Napi::CallbackInfo& info)
{
	Napi::Env env = info.Env();
	Napi::Array arr = Napi::Array::New(env, streamMsgs_.size());
	for (uint32_t i = 0; i < streamMsgs_.size(); ++i) {
		auto& m = streamMsgs_[i];
		Napi::Object o = Napi::Object::New(env);
		o.Set("sampleOffset", Napi::Number::New(env, m.sampleOffset));
		o.Set("channel", Napi::Number::New(env, m.channel));
		o.Set("type",         Napi::Number::New(env, m.type));
		o.Set("data",         Napi::Buffer<uint8_t>::Copy(env, m.data.data(), m.data.size()));
		
		Napi::Object s = Napi::Object::New(env);
		
		s.Set("ip", Napi::String::New(env, m.ip));
		s.Set("port", Napi::Number::New(env, m.port));
		s.Set("id", Napi::Number::New(env, m.id));
		o.Set("source", s);
		arr.Set(i, o);
	}
	streamMsgs_.clear();
	return arr;
}

Napi::Value AooSinkWrap::Delete(const Napi::CallbackInfo& info)
{
	sink_.reset();
	return info.Env().Undefined();
}
