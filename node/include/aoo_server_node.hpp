#pragma once

#include <napi.h>
#include <thread>
#include "aoo_server.hpp"
#include "aoo_types.h"

class AooServerWrap : public Napi::ObjectWrap<AooServerWrap> {
public:
	static void Register(Napi::Env env, Napi::Object exports);
	AooServerWrap(const Napi::CallbackInfo& info);
	~AooServerWrap();
private:
	Napi::Value Start(const Napi::CallbackInfo& info);
	Napi::Value Stop(const Napi::CallbackInfo& info);
	Napi::Value PollEvents(const Napi::CallbackInfo& info);

	static void HandleEvent(void* user, const AooEvent* e, AooThreadLevel);

	void stopThreads();

	struct PollCtx {
		Napi::Env env;
		Napi::Array arr;
		uint32_t n;
	};

	PollCtx* pollCtx_ = nullptr;
	AooServer::Ptr server_;
	bool running_ = false;
	std::thread run_thread_;
	std::thread receive_thread_;
};