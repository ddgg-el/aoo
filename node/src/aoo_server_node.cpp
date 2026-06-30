#include "aoo_server_node.hpp"
#include "aoo.h"
#include "aoo_events.h"
#include "aoo_types.h"
#include "napi.h"
#include "utils_node.hpp"
#include <cstdint>

void AooServerWrap::Register(Napi::Env env, Napi::Object exports)
{
	Napi::Function func = DefineClass(env, "AooServer", {
		InstanceMethod("start", &AooServerWrap::Start),
		InstanceMethod("stop", &AooServerWrap::Stop),
		InstanceMethod("pollEvents", &AooServerWrap::PollEvents),
		InstanceMethod("findGroup", &AooServerWrap::FindGroup),
		InstanceMethod("addGroup", &AooServerWrap::AddGroup),
		InstanceMethod("removeGroup", &AooServerWrap::RemoveGroup),
		InstanceMethod("findUserInGroup", &AooServerWrap::FindUserInGroup),
		InstanceMethod("removeUserFromGroup", &AooServerWrap::RemoveUserFromGroup),
		InstanceMethod("notifyClient", &AooServerWrap::NotifyClient),
		InstanceMethod("notifyGroup", &AooServerWrap::NotifyGroup)
	});
	exports.Set("AooServer", func);
}

AooServerWrap::AooServerWrap(const Napi::CallbackInfo& info)
: Napi::ObjectWrap<AooServerWrap>(info)
{
	server_ = AooServer::create();
	server_->setEventHandler(&AooServerWrap::HandleEvent, this, kAooEventModePoll);
}

AooServerWrap::~AooServerWrap() 
{
	stopThreads();
}

Napi::Value AooServerWrap::Start(const Napi::CallbackInfo& info)
{
	Napi::Env env = info.Env();
	int port = info[0].As<Napi::Number>().Int32Value();
	AooServerSettings settings;
	settings.portNumber = (AooUInt16) port;
	AooError err = server_->setup(settings);
	if(err != kAooOk) {
		Napi::Error::New(env, std::string("server setup failed: ") + aoo_strerror(err)).ThrowAsJavaScriptException();
		return env.Undefined();
	}

	running_ = true;
	run_thread_ = std::thread([this]() { server_->run(kAooInfinite); });
	receive_thread_ = std::thread([this]() { server_->receive(kAooInfinite); });
	return Napi::Number::New(env, settings.portNumber);
}

Napi::Value AooServerWrap::Stop(const Napi::CallbackInfo& info)
{
	stopThreads();
	return info.Env().Undefined();
}

Napi::Value AooServerWrap::FindGroup(const Napi::CallbackInfo& info)
{
	std::string name = info[0].As<Napi::String>().Utf8Value();
	AooId id = kAooIdInvalid;
	server_->findGroup(name.c_str(), &id);
	return Napi::Number::New(info.Env(), id == kAooIdInvalid ? -1 : id);
}

Napi::Value AooServerWrap::AddGroup(const Napi::CallbackInfo& info)
{
	std::string name = info[0].As<Napi::String>().Utf8Value();
	std::string pwd = (info.Length() > 1 && info[1].IsString()) ? info[1].As<Napi::String>().Utf8Value(): std::string();
	AooId groupId = kAooIdInvalid;
	AooError err = server_->addGroup(name.c_str(), pwd.empty() ? nullptr : pwd.c_str(), nullptr, nullptr, 0, &groupId);
	if(err != kAooOk) {
		Napi::Error::New(info.Env(), std::string("addGroup: ") + aoo_strerror(err)).ThrowAsJavaScriptException();
		return info.Env().Undefined();
	}
	return Napi::Number::New(info.Env(), groupId);
	
}

Napi::Value AooServerWrap::RemoveGroup(const Napi::CallbackInfo& info)
{
	server_->removeGroup(info[0].As<Napi::Number>().Int32Value());
	return info.Env().Undefined();
}

Napi::Value AooServerWrap::FindUserInGroup(const Napi::CallbackInfo& info)
{
	AooId group = info[0].As<Napi::Number>().Int32Value();
	std::string name = info[1].As<Napi::String>().Utf8Value();
	AooId userId = kAooIdInvalid;
	server_->findUserInGroup(group, name.c_str(), &userId);
	return Napi::Number::New(info.Env(), userId == kAooIdInvalid ? -1 : userId);
}

Napi::Value AooServerWrap::RemoveUserFromGroup(const Napi::CallbackInfo& info)
{

	AooId group = info[0].As<Napi::Number>().Int32Value();
	AooId user = info[1].As<Napi::Number>().Int32Value();
	server_->removeUserFromGroup(group, user);
	return info.Env().Undefined();
}

Napi::Value AooServerWrap::PollEvents(const Napi::CallbackInfo& info)
{
	Napi::Env env = info.Env();
	Napi::Array arr = Napi::Array::New(env);
	PollCtx ctx { env, arr, 0};
	pollCtx_ = &ctx;
	server_->pollEvents();
	pollCtx_ = nullptr;
	return arr;
}

void AooServerWrap::HandleEvent(void* user, const AooEvent* e, AooThreadLevel)
{
	auto* self = static_cast<AooServerWrap*>(user);
	if(!self->pollCtx_) return;
	Napi::Env env = self->pollCtx_->env;
	Napi::Object o = Napi::Object::New(env);

	switch (e->type) {
	case kAooEventClientLogin: {
		auto& c = e->clientLogin;
		o.Set("type", Napi::String::New(env, "clientLogin"));
		o.Set("id", Napi::Number::New(env, c.id));
		o.Set("error", Napi::Number::New(env, c.error));
		break;
	}
	case kAooEventClientLogout: {
		auto& c = e->clientLogout;
		o.Set("type", Napi::String::New(env, "clientLogout"));
		o.Set("id", Napi::Number::New(env, c.id));
		o.Set("error", Napi::Number::New(env, c.errorCode));
		break;
	}
	case kAooEventGroupAdd: {
		auto& g = e->groupAdd;
		o.Set("type", Napi::String::New(env, "groupAdd"));
		o.Set("id", Napi::Number::New(env, g.id));
		o.Set("name", Napi::String::New(env, g.name));
		break;
	}
	case kAooEventGroupRemove: {
		auto& g = e->groupRemove;
		o.Set("type", Napi::String::New(env, "groupRemove"));
		o.Set("id", Napi::Number::New(env, g.id));
		if(g.name) o.Set("name", Napi::String::New(env, g.name));
		break;
	}
	case kAooEventGroupJoin: {
		auto& g = e->groupJoin;
		o.Set("type", Napi::String::New(env, "groupJoin"));
		o.Set("groupId", Napi::Number::New(env, g.groupId));
		o.Set("userId", Napi::Number::New(env, g.userId));
		o.Set("clientId", Napi::Number::New(env, g.clientId));
		if(g.groupName) o.Set("group", Napi::String::New(env, g.groupName));
		if(g.userName) o.Set("user", Napi::String::New(env, g.userName));
		break;
	}
	case kAooEventGroupLeave: {
		auto& g = e->groupLeave;
		o.Set("type", Napi::String::New(env, "groupLeave"));
		o.Set("groupId", Napi::Number::New(env, g.groupId));
		o.Set("userId", Napi::Number::New(env, g.userId));
		if(g.groupName) o.Set("group", Napi::String::New(env, g.groupName));
		if(g.userName) o.Set("user", Napi::String::New(env, g.userName));
		break;
	}
	default:
		o.Set("type", Napi::String::New(env, aoo_node_util::eventTypeName(e->type)));
		break;
	}
	self->pollCtx_->arr.Set(self->pollCtx_->n++, o);
}

Napi::Value AooServerWrap::NotifyClient(const Napi::CallbackInfo& info)
{
	AooId client = info[0].As<Napi::Number>().Int32Value();
	Napi::Object m = info[1].As<Napi::Object>();
	auto data = m.Get("data").As<Napi::Buffer<uint8_t>>();
	AooData d {
		(AooDataType) m.Get("type").As<Napi::Number>().Int32Value(),
		(const AooByte*) data.Data(),
		(AooSize) data.Length()
	};
	server_->notifyClient(client, d);
	return info.Env().Undefined();
}

Napi::Value AooServerWrap::NotifyGroup(const Napi::CallbackInfo& info)
{
	AooId group = info[0].As<Napi::Number>().Int32Value();
	AooId user = info[1].As<Napi::Number>().Int32Value();

	Napi::Object m = info[2].As<Napi::Object>();
	auto data = m.Get("data").As<Napi::Buffer<uint8_t>>();
	AooData d {
		(AooDataType) m.Get("type").As<Napi::Number>().Int32Value(),
		(const AooByte*) data.Data(),
		(AooSize) data.Length()
	};
	server_->notifyGroup(group, user, d);
	return info.Env().Undefined();

}

void AooServerWrap::stopThreads() {
	if(!running_) return;
	running_ = false;
	server_->stop();
	if(run_thread_.joinable()) run_thread_.join();
	if(receive_thread_.joinable()) receive_thread_.join();
}