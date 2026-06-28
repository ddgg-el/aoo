#include "aoo_sink_node.hpp"
#include "aoo.h"

void AooSinkWrap::Register(Napi::Env env, Napi::Object exports)
{
 Napi::Function func = DefineClass(env, "AooSink", {
		InstanceMethod("setup",         &AooSinkWrap::Setup),
		InstanceMethod("setLatency",    &AooSinkWrap::SetLatency),
		InstanceMethod("handleMessage", &AooSinkWrap::HandleMessage),
		InstanceMethod("process",       &AooSinkWrap::Process),
		InstanceMethod("send",          &AooSinkWrap::Send),
	});
	exports.Set("AooSink", func);
}

AooSinkWrap::AooSinkWrap(const Napi::CallbackInfo& info)
: Napi::ObjectWrap<AooSinkWrap>(info)
{
	AooId id = info[0].As<Napi::Number>().Int32Value();
	sink_ = AooSink::create(id);
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
