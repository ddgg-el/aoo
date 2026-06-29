#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <napi.h>
#include <string>
#include <thread>
#include <vector>
#include "aoo_client.hpp"
#include "aoo_types.h"
#include "net/udp_server.hpp"

namespace aoo { class udp_server; }

class AooClientWrap: public Napi::ObjectWrap<AooClientWrap> {
public:
	static void Register(Napi::Env env, Napi::Object exports);
	AooClientWrap(const Napi::CallbackInfo& info);
	~AooClientWrap();

private: 
	Napi::Value Start(const Napi::CallbackInfo& info);
	Napi::Value StartInternal(Napi::Env env, int port);
	Napi::Value StartExternal(Napi::Env env, int port);

	Napi::Value Stop(const Napi::CallbackInfo& info);
	Napi::Value AddSink(const Napi::CallbackInfo& info);
	Napi::Value AddSource(const Napi::CallbackInfo& info);
	Napi::Value Notify(const Napi::CallbackInfo& info);
	Napi::Value Connect(const Napi::CallbackInfo& info);
	Napi::Value JoinGroup(const Napi::CallbackInfo& info);
	Napi::Value Join(const Napi::CallbackInfo& info);

	Napi::Value PollEvents(const Napi::CallbackInfo& info);
	Napi::Value SendPacket(const Napi::CallbackInfo& info);

	Napi::Value PollPackets(const Napi::CallbackInfo& info);
	Napi::Value UserId(const Napi::CallbackInfo& info);

	Napi::Value RemoveSource(const Napi::CallbackInfo& info); 
	Napi::Value RemoveSink(const Napi::CallbackInfo& info);

	void startThreads(bool external);

	static AooInt32 AOO_CALL SendFunc(void* user, const AooByte* data, AooInt32 size, const void* address, AooAddrSize addrlen, AooFlag flags);
	std::unique_ptr<aoo::udp_server> udp_server_;
	
	static void HandleEvent(void* user, const AooEvent* e, AooThreadLevel level);
	void stopThreads();
	
	struct InPacket {
		std::vector<AooByte> data;
		std::string ip;
		uint16_t port;
	};

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
	std::string user_;
	std::atomic<AooId> userId_{kAooIdInvalid};

	std::mutex inMutex_;
	std::vector<InPacket> inQueue;

	std::atomic<bool> connected_{false};
};