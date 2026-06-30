#include "aoo_client_node.hpp"
#include "aoo.h"
#include "aoo_client.hpp"
#include "aoo_defines.h"
#include "aoo_endpoint_wrap.hpp"
#include "aoo_events.h"
#include "aoo_sink_node.hpp"
#include "aoo_source_node.hpp"
#include "aoo_types.h"
#include "common/net_utils.hpp"
#include "napi.h"
#include "net/udp_server.hpp"
#include "utils_node.hpp"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <mutex>
#include <string>
#include <sys/socket.h>
#include <vector>



void AooClientWrap::Register(Napi::Env env, Napi::Object exports) 
{
	Napi::Function func = DefineClass(env, "AooClient", {
		InstanceMethod("start", &AooClientWrap::Start),
		InstanceMethod("stop", &AooClientWrap::Stop),
		InstanceMethod("addSource", &AooClientWrap::AddSource),
		InstanceMethod("addSink", &AooClientWrap::AddSink),
		InstanceMethod("notify", &AooClientWrap::Notify),
		InstanceMethod("connect", &AooClientWrap::Connect),
		InstanceMethod("joinGroup", &AooClientWrap::JoinGroup),
		InstanceMethod("join", &AooClientWrap::Join),
		InstanceMethod("pollEvents", &AooClientWrap::PollEvents),
		InstanceMethod("sendPacket", &AooClientWrap::SendPacket),
		InstanceMethod("pollPackets", &AooClientWrap::PollPackets),
		InstanceMethod("sendMessage", &AooClientWrap::SendMessage),
		InstanceMethod("userId", &AooClientWrap::UserId),
		InstanceMethod("groupId", &AooClientWrap::GroupId),
		InstanceMethod("removeSource", &AooClientWrap::RemoveSource),
		InstanceMethod("removeSink", &AooClientWrap::RemoveSink),
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
	int port = info[0].As<Napi::Number>().Int32Value();
	bool external = info.Length() > 1 && info[1].As<Napi::Boolean>().Value();
	return external ? StartExternal(info.Env(), port) 
					: StartInternal(info.Env(), port);
}

Napi::Value AooClientWrap::StartInternal(Napi::Env env, int port)
{
	AooClientSettings settings;
	settings.portNumber = port;

	AooError err = client_->setup(settings);
	if(err != kAooOk) {
		Napi::Error::New(env, std::string("setup failed: ") + aoo_strerror(err)).ThrowAsJavaScriptException();
		return env.Undefined();
	}

	running_ = true;
	send_thread_    = std::thread([this]() { client_->send(kAooInfinite); });
	run_thread_     = std::thread([this]() { client_->run(kAooInfinite); });
	receive_thread_ = std::thread([this]() { client_->receive(kAooInfinite); });

	return Napi::Number::New(env, settings.portNumber);
}

Napi::Value AooClientWrap::StartExternal(Napi::Env env, int port)
{
	udp_server_ = std::make_unique<aoo::udp_server>();
	try {
		udp_server_->start(port, [this](const AooByte* data, AooSize size, const aoo::ip_address& addr) {
			AooMsgType type;
			AooId id;
			AooInt32 offset;
			bool isAudio = (aoo_parsePattern(data, size, &type, &id, &offset) == kAooOk) && (type == kAooMsgTypeSource || type == kAooMsgTypeSink);
			if(isAudio) {
				std::lock_guard<std::mutex> lock(inMutex_);
				inQueue.push_back({std::vector<AooByte>(data, data+size), std::string(addr.name()), (uint16_t)addr.port()});
			} else {
				client_->handlePacket(data, size, addr.address(), addr.length());
			}
		});
	} catch (const std::exception& e) {
		Napi::Error::New(env, std::string("udp_server start failed: ") + e.what()).ThrowAsJavaScriptException();
		return env.Undefined();
	}
	AooClientSettings settings;
	settings.portNumber = udp_server_->port();
	settings.socketType = kAooSocketDualStack;
	settings.options = kAooClientExternalUDPSocket;
	settings.userData = this;
	settings.sendFunc = &AooClientWrap::SendFunc;

	AooError err = client_->setup(settings);
	if(err != kAooOk) {
		Napi::Error::New(env, std::string("setup failed: ") + aoo_strerror(err)).ThrowAsJavaScriptException();
		return env.Undefined();
	}
	running_ = true;
	send_thread_ = std::thread([this]() { client_->send(kAooInfinite); });
	run_thread_ = std::thread([this]() {client_->run(kAooInfinite); });
	receive_thread_ = std::thread([this]() {udp_server_->run(-1); });

	return Napi::Number::New(env,settings.portNumber);
}

void AooClientWrap::startThreads(bool external)
{
	
}

Napi::Value AooClientWrap::Stop(const Napi::CallbackInfo& info) 
{
	stopThreads();
	return info.Env().Undefined();
}

Napi::Value AooClientWrap::AddSink(const Napi::CallbackInfo& info)
{
	auto* sink = AooSinkWrap::Unwrap(info[0].As<Napi::Object>());
	client_->addSink(sink->native());
	return info.Env().Undefined();
}

Napi::Value AooClientWrap::AddSource(const Napi::CallbackInfo& info)
{
	auto* src =AooSourceWrap::Unwrap(info[0].As<Napi::Object>());
	client_->addSource(src->native());
	return info.Env().Undefined();
}

Napi::Value AooClientWrap::Notify(const Napi::CallbackInfo& info)
{
	client_->notify();
	return info.Env().Undefined();
}

Napi::Value AooClientWrap::Connect(const Napi::CallbackInfo& info) 
{
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

Napi::Value AooClientWrap::JoinGroup(const Napi::CallbackInfo& info) 
{
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

Napi::Value AooClientWrap::Join(const Napi::CallbackInfo& info)
{
	host_  = info[0].As<Napi::String>().Utf8Value();
	int port = info[1].As<Napi::Number>().Int32Value();
	group_ = info[2].As<Napi::String>().Utf8Value();
	user_  = info[3].As<Napi::String>().Utf8Value();

	AooClientConnect args;
	args.hostName = host_.c_str();
	args.port = (AooUInt16)port;

	client_->connect(args,
		[](void* user, const AooRequest*, AooError result, const AooResponse*) {
			auto* self = static_cast<AooClientWrap*>(user);
			if (result != kAooOk) {
				printf("[client] connect failed: %s\n", aoo_strerror(result));
				return;
			}
			self->connected_.store(true);
			AooClientJoinGroup jargs;
			jargs.groupName = self->group_.c_str();
			jargs.userName  = self->user_.c_str();
			self->client_->joinGroup(jargs,
				[](void* user, const AooRequest*, AooError r, const AooResponse* resp) {
					auto* self = static_cast<AooClientWrap*>(user);
					if(r == kAooOk && resp) {
						self->userId_.store(resp->groupJoin.userId);
						self->groupId_.store(resp->groupJoin.groupId);
					}
					printf("[client] joinGroup: %s\n", r == kAooOk ? "OK" : aoo_strerror(r));
				}, self);
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

Napi::Value AooClientWrap::SendPacket(const Napi::CallbackInfo& info) 
{
	Napi::Env env = info.Env();

	auto bytes = info[0].As<Napi::Buffer<uint8_t>>();
	std::string ip = info[1].As<Napi::String>().Utf8Value();
	AooUInt16 port = (AooUInt16) info[2].As<Napi::Number>().Uint32Value();

	AooSockAddrStorage addr;
	AooAddrSize addrlen = sizeof(addr);
	AooError err = aoo_ipEndpointToSockAddr(ip.c_str(), port, kAooSocketDualStack, &addr, &addrlen);
	if(err != kAooOk) {
		Napi::Error::New(env, std::string("bad address: ") + aoo_strerror(err)).ThrowAsJavaScriptException();
		return env.Undefined();
	}

	client_->sendPacket((const AooByte*)bytes.Data(), (AooInt32) bytes.Length(), &addr, addrlen);
	return env.Undefined();
}

Napi::Value AooClientWrap::PollPackets(const Napi::CallbackInfo& info) 
{
	Napi::Env env = info.Env();
	std::vector<InPacket> packets;
	{
		std::lock_guard<std::mutex> lock(inMutex_);
		packets.swap(inQueue);
	}

	Napi::Array arr = Napi::Array::New(env, packets.size());
	for (uint32_t i = 0; i < packets.size(); ++i) {	
		auto& p = packets[i];
		Napi::Object o = Napi::Object::New(env);
		o.Set("bytes", Napi::Buffer<uint8_t>::Copy(env, p.data.data(), p.data.size()));
		o.Set("ip", Napi::String::New(env, p.ip));
		o.Set("port", Napi::Number::New(env, p.port));
		arr.Set(i, o);
	}
	return arr;
}

AooInt32 AOO_CALL AooClientWrap::SendFunc(void* user, const AooByte* data, AooInt32 size, const void* address, AooAddrSize addrlen, AooFlag) 
{
	auto* self = static_cast<AooClientWrap*>(user);
	if(!self->udp_server_) return 0;
	aoo::ip_address addr((const struct sockaddr*) address, (socklen_t)addrlen);
	return self->udp_server_->send(addr, data, size);
}

Napi::Value AooClientWrap::SendMessage(const Napi::CallbackInfo& info) 
{
	Napi::Env env = info.Env();
	AooId user;
	if(info[0].IsString()) {
		std::string name = info[0].As<Napi::String>().Utf8Value();
		auto it = peerIds_.find(name);
		if(it == peerIds_.end()) {
			Napi::Error::New(env, "sendMessage: unknown user '" + name + "'").ThrowAsJavaScriptException();
			return env.Undefined();
		}
		user = it->second;
	} else {
		user = info[0].As<Napi::Number>().Int32Value();
	}
	Napi::Object m = info[1].As<Napi::Object>();
	auto data = m.Get("data").As<Napi::Buffer<uint8_t>>();
	bool reliable = info.Length() > 2 && info[2].As<Napi::Boolean>().Value();

	AooData d {
		(AooDataType) m.Get("type").As<Napi::Number>().Int32Value(),
		(const AooByte*) data.Data(),
		(AooSize) data.Length()
	};
	client_->sendMessage(groupId_.load(), user, d, 0, reliable ? kAooMessageReliable : 0);
	return info.Env().Undefined();
}

Napi::Value AooClientWrap::GroupId(const Napi::CallbackInfo& info)
{
	AooId id = groupId_.load();
	return Napi::Number::New(info.Env(), id == kAooIdInvalid ? -1 : id);
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
		if(e->type == kAooEventPeerJoin) {
			self->peerIds_[p->userName] = p->userId;
			self->peerNames_[p->userId] = p->userName;
		} else {
			self->peerIds_.erase(p->userName);
			self->peerNames_.erase(p->userId);
		}
		AooEndpoint peerEp { p->address.data, p->address.size, p->userId};
		char ipbuf[64];
		AooSize ipsize = sizeof(ipbuf);
		AooUInt16 port = 0;
		aoo_sockAddrToIpEndpoint(p->address.data, p->address.size, ipbuf, &ipsize, &port, nullptr);
		o.Set("type", e->type == kAooEventPeerJoin ? "peerJoin" : "peerLeave");
		o.Set("group", Napi::String::New(env, p->groupName));
		o.Set("user", Napi::String::New(env, p->userName));
		o.Set("endpoint", aoo_node_util::endpointToObject(env, peerEp));
		// o.Set("ip", Napi::String::New(env, std::string(ipbuf,ipsize)));
		// o.Set("port", Napi::Number::New(env, port));
		// o.Set("userId", Napi::Number::New(env, p->userId));
		break;
	}
	case kAooEventPeerMessage: {
		auto& p = e->peerMessage;
		auto it = self->peerNames_.find(p.userId);

		o.Set("type", Napi::String::New(env, "peerMessage"));
		o.Set("group", Napi::Number::New(env, p.groupId));
		o.Set("userId", Napi::Number::New(env, p.userId));
		o.Set("user", Napi::String::New(env, it != self->peerNames_.end() ? it->second : std::string()));
		o.Set("msgType", Napi::Number::New(env, p.data.type));
		o.Set("data", Napi::Buffer<uint8_t>::Copy(env, p.data.data, p.data.size));
		break;
	}
	case kAooEventDisconnect:
		o.Set("type", Napi::String::New(env, "disconnect"));
		break;
	case kAooEventNotification: {
		auto& n = e->notification;
		o.Set("type", Napi::String::New(env, "notification"));
		o.Set("msgType", Napi::Number::New(env, n.message.type));
		o.Set("data", Napi::Buffer<uint8_t>::Copy(env, n.message.data, n.message.size));
		break;
	}
	default:
		o.Set("type", Napi::String::New(env, aoo_node_util::eventTypeName(e->type)));
		break;
	}
	self->pollCtx_->arr.Set(self->pollCtx_->n++, o);
}

void AooClientWrap::stopThreads() 
{
	if(!running_) return;
	running_ = false;
	if(udp_server_) udp_server_->stop();
	client_->stop();

	if(send_thread_.joinable()) send_thread_.join();
	if(run_thread_.joinable()) run_thread_.join();
	if(receive_thread_.joinable()) receive_thread_.join();

	udp_server_.reset();
}

Napi::Value AooClientWrap::UserId(const Napi::CallbackInfo& info) 
{
	AooId id = userId_.load();
	return Napi::Number::New(info.Env(), id == kAooIdInvalid ? -1 : id);
}

Napi::Value AooClientWrap::RemoveSource(const Napi::CallbackInfo& info)
{
	client_->removeSource(AooSourceWrap::Unwrap(info[0].As<Napi::Object>())->native());
	return info.Env().Undefined();
}

Napi::Value AooClientWrap::RemoveSink(const Napi::CallbackInfo& info)
{
	client_->removeSink(AooSinkWrap::Unwrap(info[0].As<Napi::Object>())->native());
	return info.Env().Undefined();
}