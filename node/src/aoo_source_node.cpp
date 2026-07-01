#include "aoo_source_node.hpp"
#include "aoo.h"
#include "aoo_endpoint_wrap.hpp"
#include "aoo_types.h"
#include "aoo_utils_node.hpp"
#include "codec/aoo_opus.h"
#include "codec/aoo_pcm.h"
#include "opus_defines.h"
#include <cstdint>

void AooSourceWrap::Register(Napi::Env env, Napi::Object exports)
{
	Napi::Function func = DefineClass(env, "AooSource", {
		InstanceMethod("send", &AooSourceWrap::Send),
		InstanceMethod("setup", &AooSourceWrap::Setup),
		InstanceMethod("handleMessage", &AooSourceWrap::HandleMessage),
		InstanceMethod("pollEvents", &AooSourceWrap::PollEvents),
		InstanceMethod("eventsAvailable", &AooSourceWrap::EventsAvailable),
		InstanceMethod("reset", &AooSourceWrap::Reset),
		InstanceMethod("setId", &AooSourceWrap::SetId),
		InstanceMethod("setBufferSize", &AooSourceWrap::SetBufferSize),
		InstanceMethod("setPacketSize", &AooSourceWrap::SetPacketSize),
		InstanceMethod("setPingInterval", &AooSourceWrap::SetPingInterval),
		InstanceMethod("setResampleMethod",&AooSourceWrap::SetResampleMethod),
		InstanceMethod("setDynamicResampling", &AooSourceWrap::SetDynamicResampling),
		InstanceMethod("setBinaryFormat" ,&AooSourceWrap::SetBinaryFormat),
		InstanceMethod("setDllBandwidth", &AooSourceWrap::SetDllBandwidth),
		InstanceMethod("getRealSampleRate", &AooSourceWrap::GetRealSampleRate),
		InstanceMethod("setRedundancy", &AooSourceWrap::SetRedundancy),
		InstanceMethod("setResendBufferSize", &AooSourceWrap::SetResendBufferSize),
		InstanceMethod("setStreamTimeSendInterval", &AooSourceWrap::SetStreamTimeSendInterval),
		InstanceMethod("removeAllSinks", &AooSourceWrap::RemoveAllSinks),
		InstanceMethod("activate", &AooSourceWrap::Activate),
		InstanceMethod("setSinkChannelOffset", &AooSourceWrap::SetSinkChannelOffset),
		InstanceMethod("setFormat", &AooSourceWrap::SetFormat),
		InstanceMethod("setOpusBitrate", &AooSourceWrap::SetOpusBitrate),
		InstanceMethod("setOpusComplexity", &AooSourceWrap::SetOpusComplexity),
		InstanceMethod("setOpusSignalType", &AooSourceWrap::SetOpusSignalType),
		InstanceMethod("addSink", &AooSourceWrap::AddSink),
		InstanceMethod("removeSink", &AooSourceWrap::RemoveSink),
		InstanceMethod("startStream", &AooSourceWrap::StartStream),
		InstanceMethod("stopStream", &AooSourceWrap::StopStream),
		InstanceMethod("process", &AooSourceWrap::Process),
		InstanceMethod("handleInvite", &AooSourceWrap::HandleInvite),
		InstanceMethod("handleUninvite", &AooSourceWrap::HandleUninvite),
		InstanceMethod("addStreamMessage", &AooSourceWrap::AddStreamMessage),
		InstanceMethod("delete", &AooSourceWrap::Delete)
	});
	exports.Set("AooSource", func);
}

AooSourceWrap::AooSourceWrap(const Napi::CallbackInfo& info)
: StreamEndpointWrap(info)
{
	AooId id = info[0].As<Napi::Number>().Int32Value();
	source_ = AooSource::create(id);
	source_->setEventHandler(&AooSourceWrap::HandleEvent, this, kAooEventModePoll);
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

Napi::Value AooSourceWrap::HandleInvite(const Napi::CallbackInfo& info)
{
	Napi::Object ep = info[0].As<Napi::Object>();
	std::string ip = ep.Get("ip").As<Napi::String>().Utf8Value();
	AooUInt16 port = (AooUInt16) ep.Get("port").As<Napi::Number>().Uint32Value();
	AooId id       = ep.Get("id").As<Napi::Number>().Int32Value();
	AooId token    = info[1].As<Napi::Number>().Int32Value();
	bool  accept   = info[2].As<Napi::Boolean>().Value();
	AooSockAddrStorage addr; AooAddrSize addrlen = sizeof(addr);
	if (aoo_ipEndpointToSockAddr(ip.c_str(), port, kAooSocketDualStack, &addr, &addrlen) != kAooOk) {
		Napi::Error::New(info.Env(), "handleInvite: bad address").ThrowAsJavaScriptException();
		return info.Env().Undefined();
	}
	AooEndpoint endpoint { &addr, addrlen, id };
	source_->handleInvite(endpoint, token, accept ? kAooTrue : kAooFalse);
	return info.Env().Undefined();
}

Napi::Value AooSourceWrap::HandleUninvite(const Napi::CallbackInfo& info)
{
	AooSockAddrStorage addr;
	AooEndpoint ep;
	if(!AooNodeUtils::toEndpoint(info[0].As<Napi::Object>(), addr, ep)) {
		Napi::Error::New(info.Env(), "handleUninvite: bad address").ThrowAsJavaScriptException();
		return info.Env().Undefined();
	}
	AooId token = info[1].As<Napi::Number>().Int32Value();
	bool accept = info[2].As<Napi::Boolean>().Value();
	source_->handleUninvite(ep, token, accept ? kAooTrue : kAooFalse);
	return info.Env().Undefined();
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
		o.Set("endpoint", AooNodeUtils::endpointToObject(env, p.endpoint));
		o.Set("rtt", Napi::Number::New(env, rtt));
		o.Set("packetLoss", Napi::Number::New(env, p.packetLoss));
		break;
	}
	case kAooEventSinkAdd:
	case kAooEventSourceAdd:
		o.Set("type", e->type == kAooEventSinkAdd ? "sinkAdd" : "sinkRemove");
		o.Set("endpoint", AooNodeUtils::endpointToObject(env, e->endpoint.endpoint));
		break;
	case kAooEventInvite:
		o.Set("type", "invite");
		o.Set("endpoint", AooNodeUtils::endpointToObject(env, e->invite.endpoint));
		o.Set("token", Napi::Number::New(env, e->invite.token));
		break;
	case kAooEventUninvite:
		o.Set("type", "uninvite");
		o.Set("endpoint", AooNodeUtils::endpointToObject(env, e->uninvite.endpoint));
		o.Set("token", Napi::Number::New(env, e->uninvite.token));
		break;
	default:
		o.Set("type", Napi::String::New(env, AooNodeUtils::eventTypeName(e->type)));
		break;
	}
	self->pollCtx_->arr.Set(self->pollCtx_->n++, o);
}

Napi::Value AooSourceWrap::SetRedundancy(const Napi::CallbackInfo& info)
{
	source_->setRedundancy(info[0].As<Napi::Number>().Int32Value());
	return info.Env().Undefined();
}

Napi::Value AooSourceWrap::SetResendBufferSize(const Napi::CallbackInfo& info)
{
	source_->setResendBufferSize(info[0].As<Napi::Number>().DoubleValue());
	return info.Env().Undefined();
}

Napi::Value AooSourceWrap::SetStreamTimeSendInterval(const Napi::CallbackInfo& info)
{
	source_->setStreamTimeSendInterval(info[0].As<Napi::Number>().DoubleValue());
	return info.Env().Undefined();
}

Napi::Value AooSourceWrap::RemoveAllSinks(const Napi::CallbackInfo& info)
{
	source_->removeAllSinks();
	return info.Env().Undefined();
}

Napi::Value AooSourceWrap::Activate(const Napi::CallbackInfo& info)
{
	AooSockAddrStorage addr;
	AooEndpoint ep;
	if(!AooNodeUtils::toEndpoint(info[0].As<Napi::Object>(), addr, ep)) {
		Napi::Error::New(info.Env(), "activate: bad address").ThrowAsJavaScriptException();
		return info.Env().Undefined();
	}
	source_->activate(ep, info[1].As<Napi::Boolean>().Value() ? kAooTrue : kAooFalse);
	return info.Env().Undefined();
}

Napi::Value AooSourceWrap::SetSinkChannelOffset(const Napi::CallbackInfo& info)
{
	AooSockAddrStorage addr;
	AooEndpoint ep;
	if(!AooNodeUtils::toEndpoint(info[0].As<Napi::Object>(), addr, ep)) {
		Napi::Error::New(info.Env(), "setSinkChannelOffset: bad address").ThrowAsJavaScriptException();
		return info.Env().Undefined();
	}
	source_->setSinkChannelOffset(ep, info[1].As<Napi::Number>().Int32Value());
	return info.Env().Undefined();
}

Napi::Value AooSourceWrap::AddStreamMessage(const Napi::CallbackInfo& info)
{
	Napi::Object o = info[0].As<Napi::Object>();
	auto data = o.Get("data").As<Napi::Buffer<uint8_t>>();
	AooStreamMessage msg;
	msg.sampleOffset = o.Has("sampleOffset") ? o.Get("sampleOffset").As<Napi::Number>().Int32Value() : 0;
	msg.channel = o.Has("channel") ? o.Get("channel").As<Napi::Number>().Int32Value() : 0;
	msg.type = (AooDataType)o.Get("type").As<Napi::Number>().Int32Value();
	msg.size = (AooInt32) data.Length();
	msg.data = (const AooByte*) data.Data();

	source_->addStreamMessage(msg);
	return info.Env().Undefined();
}

Napi::Value AooSourceWrap::SetFormat(const Napi::CallbackInfo& info) 
{
	Napi::Env env = info.Env();
	std::string codec = "pcm";
	Napi::Object fmt;
	bool hasFmt = info.Length() > 0 && info[0].IsObject();
	if (hasFmt) {
		fmt = info[0].As<Napi::Object>();
		if (fmt.Has("codec")) codec = fmt.Get("codec").As<Napi::String>().Utf8Value();
	}

	AooError err;
	if (codec == "opus") {
		opus_int32 app = OPUS_APPLICATION_AUDIO;
		if (hasFmt && fmt.Has("application")) {
			std::string a = fmt.Get("application").As<Napi::String>().Utf8Value();
			if      (a == "lowdelay") app = OPUS_APPLICATION_RESTRICTED_LOWDELAY;
			else if (a == "voip")     app = OPUS_APPLICATION_VOIP;
		}
		int blockSize = (hasFmt && fmt.Has("blockSize"))
			? fmt.Get("blockSize").As<Napi::Number>().Int32Value() : 480;

		AooFormatOpus of;
		AooFormatOpus_init(&of, channels_, 48000, blockSize, app);  // Opus rate = 48 kHz
		err = source_->setFormat(of.header);

		if (err == kAooOk && hasFmt) {                              // optional live tweaks
			if (fmt.Has("bitrate")) {
				opus_int32 v = fmt.Get("bitrate").As<Napi::Number>().Int32Value();
				source_->codecControl(kAooCodecOpus, OPUS_SET_BITRATE_REQUEST, 0, &v, sizeof(v));
			}
			if (fmt.Has("complexity")) {
				opus_int32 v = fmt.Get("complexity").As<Napi::Number>().Int32Value();
				source_->codecControl(kAooCodecOpus, OPUS_SET_COMPLEXITY_REQUEST, 0, &v, sizeof(v));
			}
		}
	} else {
		AooFormatPcm pf;
		AooFormatPcm_init(&pf, channels_, sampleRate_, blockSize_, kAooPcmFloat32);
		err = source_->setFormat(pf.header);
	}

	if (err != kAooOk)
		Napi::Error::New(env, std::string("setFormat: ") + aoo_strerror(err))
			.ThrowAsJavaScriptException();
	return env.Undefined();
}

Napi::Value AooSourceWrap::SetOpusBitrate(const Napi::CallbackInfo& info) 
{
	opus_int32 v = info[0].As<Napi::Number>().Int32Value();
	source_->codecControl(kAooCodecOpus, OPUS_SET_BITRATE_REQUEST, 0, &v, sizeof(v));
	return info.Env().Undefined();
}

Napi::Value AooSourceWrap::SetOpusComplexity(const Napi::CallbackInfo& info) 
{
	opus_int32 v = info[0].As<Napi::Number>().Int32Value();
	source_->codecControl(kAooCodecOpus, OPUS_SET_COMPLEXITY_REQUEST, 0, &v, sizeof(v));
	return info.Env().Undefined();
}

Napi::Value AooSourceWrap::SetOpusSignalType(const Napi::CallbackInfo& info) 
{
	std::string s = info[0].As<Napi::String>().Utf8Value();
	opus_int32 sig = (s == "voice") ? OPUS_SIGNAL_VOICE
	               : (s == "music") ? OPUS_SIGNAL_MUSIC : OPUS_AUTO;
	source_->codecControl(kAooCodecOpus, OPUS_SET_SIGNAL_REQUEST, 0, &sig, sizeof(sig));
	return info.Env().Undefined();
}

Napi::Value AooSourceWrap::Delete(const Napi::CallbackInfo& info) 
{
	source_.reset();
	return info.Env().Undefined();
}