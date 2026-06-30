#include "aoo_server_node.hpp"
#include "utils_node.hpp"

void AooServerWrap::Register(Napi::Env env, Napi::Object exports)
{
	Napi::Function func = DefineClass(env, "AooServer", {
		InstanceMethod("start", &AooServerWrap::Start),
		InstanceMethod("stop", &AooServerWrap::Stop),
		InstanceMethod("pollEvents", &AooServerWrap::PollEvents),
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
	case kAooEventGroupAdd: {
		auto& g = e->groupAdd;
		o.Set("type", Napi::String::New(env, "groupAdd"));
		o.Set("id", Napi::Number::New(env, g.id));
		o.Set("name", Napi::String::New(env, g.name));
		break;
	}
	default:
		o.Set("type", Napi::String::New(env, aoo_node_util::eventTypeName(e->type)));
		break;
	}
	self->pollCtx_->arr.Set(self->pollCtx_->n++, o);
}

void AooServerWrap::stopThreads() {
	if(!running_) return;
	running_ = false;
	server_->stop();
	if(run_thread_.joinable()) run_thread_.join();
	if(receive_thread_.joinable()) receive_thread_.join();
}