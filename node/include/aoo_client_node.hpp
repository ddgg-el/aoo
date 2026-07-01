#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <napi.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "aoo_client.hpp"
#include "aoo_types.h"
#include "net/udp_server.hpp"
#include "aoo_utils_node.hpp"

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
	Napi::Value Connected(const Napi::CallbackInfo& info);
	Napi::Value GroupId(const Napi::CallbackInfo& info);
	Napi::Value SendMessage(const Napi::CallbackInfo& info);

	Napi::Value RemoveSource(const Napi::CallbackInfo& info); 
	Napi::Value RemoveSink(const Napi::CallbackInfo& info);

	Napi::Value LeaveGroup(const Napi::CallbackInfo& info);
	Napi::Value Disconnect(const Napi::CallbackInfo& info);

	void startThreads(bool external);

	static AooInt32 AOO_CALL SendFunc(void* user, const AooByte* data, AooInt32 size, const void* address, AooAddrSize addrlen, AooFlag flags);
	std::unique_ptr<aoo::udp_server> udp_server_;

	static void AOO_CALL OnResponse(void* user, const AooRequest*, AooError result, const AooResponse* resp);
	static void AOO_CALL OnJoinConnected(void* user, const AooRequest*, AooError result, const AooResponse* resp);
	void ResolvePending(Napi::Env env);
	void RejectPending(Napi::Env env, const char* reason);
	
	static void HandleEvent(void* user, const AooEvent* e, AooThreadLevel level);
	void stopThreads();

	AooNodeUtils::PollCtx* pollCtx_ = nullptr;
	AooClient::Ptr client_;
	bool running_ = false;

	std::thread send_thread_;
	std::thread receive_thread_;
	std::thread run_thread_;

	std::string host_;
	std::string group_;
	std::string user_;
	std::string password_ = "_";
	std::atomic<AooId> userId_{kAooIdInvalid};
	std::atomic<AooId> groupId_{ kAooIdInvalid };
	std::unordered_map<std::string, AooId> peerIds_;
	std::unordered_map<AooId, std::string> peerNames_;

	std::mutex inMutex_;
	std::vector<AooNodeUtils::InPacket> inQueue;


	AooId nextReqId_ = 0;
	std::unordered_map<AooId, Napi::Promise::Deferred> pending_;
	std::vector<AooNodeUtils::CompletedRequest> completed_;
	std::mutex reqMutex_;

	std::atomic<bool> connected_{false};
};