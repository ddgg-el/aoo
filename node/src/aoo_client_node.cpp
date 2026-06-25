#include "aoo_client_node.hpp"
#include "aoo.h"
#include "aoo_events.h"
#include "aoo_types.h"
#include <cstdint>
#include <cstdio>



void AooClientWrap::Register(Napi::Env env, Napi::Object exports) 
{
	Napi::Function func = DefineClass(env, "AooClient", {
		InstanceMethod("start", &AooClientWrap::Start),
		InstanceMethod("stop", &AooClientWrap::Stop),
		InstanceMethod("connect", &AooClientWrap::Connect),
		InstanceMethod("joinGroup", &AooClientWrap::JoinGroup),
		InstanceMethod("pollEvents", &AooClientWrap::PollEvents)
	});
	exports.Set("AooClient", func);
	
}

AooClientWrap::AooClientWrap(const Napi::CallbackInfo& info) 
: Napi::ObjectWrap<AooClientWrap>(info) 
{
	client_ = AooClient::create();
	client_->setEventHandler(&AooClientWrap::HandleEvent, this, kAooEventModePoll);
}

AooClientWrap::~AooClientWrap() 
{ 
	stopThreads(); 
}


Napi::Value AooClientWrap::Start(const Napi::CallbackInfo& info) 
{
	Napi::Env env = info.Env();
	AooClientSettings settings;
	settings.portNumber = info[0].As<Napi::Number>().Int32Value();

	AooError err = client_->setup(settings);
	if(err != kAooOk) {
		Napi::Error::New(env, std::string("setup failed: ", aoo_strerror(err)))
		.ThrowAsJavaScriptException();
		return env.Undefined();
	}
	running_ = true;
	send_thread_ = std::thread([this]() { client_->send(kAooInfinite); });
	receive_thread_ = std::thread([this]() {client_->receive(kAooInfinite); });
	run_thread_ = std::thread([this]() {client_->run(kAooInfinite); });

	return Napi::Number::New(env,settings.portNumber);
}

Napi::Value AooClientWrap::Stop(const Napi::CallbackInfo& info) 
{
	stopThreads();
	return info.Env().Undefined();
}

Napi::Value AooClientWrap::Connect(const Napi::CallbackInfo& info) {
	host_ = info[0].As<Napi::String>().Utf8Value();
	int32_t port = info[1].As<Napi::Number>().Int32Value();
	AooClientConnect args;
	args.hostName = host_.c_str();
	args.port = (AooUInt16)port;

	client_->connect(args, 
		[](void* user, const AooRequest*, AooError result, const AooResponse*) {
			printf("[client] connect: %s\n", result == kAooOk ? "OK" : aoo_strerror(result));
			static_cast<AooClientWrap*>(user)->connected_.store(result == kAooOk);
		}, this);
	return info.Env().Undefined();
}

Napi::Value AooClientWrap::JoinGroup(const Napi::CallbackInfo& info) {
	group_ = info[0].As<Napi::String>().Utf8Value();
	user_ = info[1].As<Napi::String>().Utf8Value();
	AooClientJoinGroup args;
	args.groupName = group_.c_str();
	args.userName = user_.c_str();

	client_->joinGroup(args, 
		[](void*, const AooRequest*, AooError result, const AooResponse*) {
			printf("[client] joinGroup: %s\n", result == kAooOk ? "OK" : aoo_strerror(result));
	}, this);
	return info.Env().Undefined();
}

Napi::Value AooClientWrap::PollEvents(const Napi::CallbackInfo& info)
{
	Napi::Env env = info.Env();
	Napi::Array arr = Napi::Array::New(env);
	PollCtx ctx { env, arr, 0};
	pollCtx_ = &ctx;
	client_->pollEvents();
	pollCtx_ = nullptr;
	return arr;
}

void AooClientWrap::HandleEvent(void* user, const AooEvent* e, AooThreadLevel)
{
	auto* self = static_cast<AooClientWrap*>(user);
	if(!self->pollCtx_) return;

	Napi::Env env = self->pollCtx_->env;
	Napi::Object o = Napi::Object::New(env);

	switch (e->type) {
	case kAooEventPeerJoin:
	case kAooEventPeerLeave: {
		auto* p = reinterpret_cast<const AooEventPeer*>(e);
		o.Set("type", e->type == kAooEventPeerJoin ? "peerJoin" : "peerLeave");
		o.Set("group", Napi::String::New(env, p->groupName));
		o.Set("user", Napi::String::New(env, p->userName));
		o.Set("userId", Napi::Number::New(env, p->userId));
		break;
	}
	case kAooEventDisconnect:
		o.Set("type", Napi::String::New(env, "disconnect"));
		break;
	default:
		o.Set("type", Napi::Number::New(env, (double) e->type));
		break;
	}
	self->pollCtx_->arr.Set(self->pollCtx_->n++, o);
}

void AooClientWrap::stopThreads() 
{
	if(!running_) return;
	running_ = false;
	client_->stop();

	if(send_thread_.joinable()) send_thread_.join();
	if(receive_thread_.joinable()) receive_thread_.join();
	if(run_thread_.joinable()) run_thread_.join();
}
