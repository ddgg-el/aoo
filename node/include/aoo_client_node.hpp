#pragma once

#include <atomic>
#include <napi.h>
#include <thread>
#include "aoo_client.hpp"
#include "aoo_types.h"

class AooClientWrap: public Napi::ObjectWrap<AooClientWrap> {
public:
	static void Register(Napi::Env env, Napi::Object exports);
	AooClientWrap(const Napi::CallbackInfo& info);
	~AooClientWrap();

private: 
	Napi::Value Start(const Napi::CallbackInfo& info);
	Napi::Value Stop(const Napi::CallbackInfo& info);
	Napi::Value Connect(const Napi::CallbackInfo& info);
	Napi::Value JoinGroup(const Napi::CallbackInfo& info);
	Napi::Value PollEvents(const Napi::CallbackInfo& info);
	Napi::Value SendPacket(const Napi::CallbackInfo& info);
	
	static void HandleEvent(void* user, const AooEvent* e, AooThreadLevel level);
	void stopThreads();
	
	
	struct PollCtx { 
		Napi::Env env; 
		Napi::Array arr;
		uint32_t n;
	};

	PollCtx* pollCtx_ = nullptr;
	AooClient::Ptr client_;
	bool running_ = false;

	std::thread send_thread_;
	std::thread receive_thread_;
	std::thread run_thread_;

	std::string host_;
	std::string group_;
	std::string user_;;

	std::atomic<bool> connected_{false};
};