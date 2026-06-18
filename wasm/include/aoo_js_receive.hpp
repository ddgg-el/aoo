#include "aoo.h"
#include "aoo_events.h"
#include "aoo_sink.hpp"
#include "aoo_types.h"
#include <cstdint>
#include <emscripten/val.h>
#include <emscripten/wire.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

class AooReceiveJS {
public:
	AooReceiveJS(AooId id) : sink_(AooSink::create(id)) {
		sink_->setEventHandler([](void *user, const AooEvent* event, AooThreadLevel) {
			static_cast<AooReceiveJS*>(user)->handle_event(*event);
		}, this, kAooEventModeCallback);
	}

	int setup(int c, double sr, int n) { 
		nchannels_ = c;
		blocksize_ = n;
		return sink_->setup(c, sr, n, 0); 
	};
	int setLatency(double s) { return sink_->setLatency(s); }
	int send(emscripten::val cb) {
		sendCb_ = cb;
		return sink_->send(&AooReceiveJS::sendTrampoline, this);
	}

	int inviteSource(std::string ip, int port, AooId id) {
		AooSockAddrStorage addr;
		AooAddrSize len = sizeof(addr);
		if(aoo_ipEndpointToSockAddr(ip.c_str(), (AooUInt16) port, kAooSocketIPv4, &addr, &len) != kAooOk) return kAooErrorBadArgument;

		AooEndpoint ep { &addr, len, id};
		return sink_->inviteSource(ep, nullptr);
	}

	int handleMessage(emscripten::val data, std::string ip, int port) {
		std::vector<uint8_t> bytes = emscripten::convertJSArrayToNumberVector<uint8_t>(data);

		AooSockAddrStorage addr;
		AooAddrSize len = sizeof(addr);
		if(aoo_ipEndpointToSockAddr(ip.c_str(), (AooUInt16) port, kAooSocketIPv4, &addr, &len) != kAooOk) return kAooErrorBadArgument;

		return sink_->handleMessage(bytes.data(), (AooInt32)bytes.size(), &addr, len);
	}

	int process(double timeSeconds) {
		AooNtpTime t = aoo_ntpTimeFromSeconds(timeSeconds);
		std::vector<std::vector<AooSample>>bufs(nchannels_,std::vector<AooSample>(blocksize_, 0.0f));
		std::vector<AooSample*> ptrs;
		ptrs.reserve(nchannels_);
		for(auto& b:bufs) {
			ptrs.push_back(b.data());
		}

		return sink_->process(ptrs.data(), blocksize_, t, nullptr, nullptr);
	}

	int processNow() {
		AooNtpTime t = aoo_getCurrentNtpTime();
		std::vector<std::vector<AooSample>>bufs(nchannels_,std::vector<AooSample>(blocksize_, 0.0f));
		std::vector<AooSample*> ptrs;
		ptrs.reserve(nchannels_);
		for(auto& b:bufs) {
			ptrs.push_back(b.data());
		}

		return sink_->process(ptrs.data(), blocksize_, t, nullptr, nullptr);
	}

private:
	AooSink::Ptr sink_;
	int nchannels_ = 0;
	int blocksize_ = 0;

	emscripten::val sendCb_ = emscripten::val::undefined();
	void handle_event(const AooEvent& event) {
		switch (event.type) {
		case kAooEventStreamStart: {
			std::cout << "start stream from source " << event.streamStart.endpoint.address << std::endl;
        	break;
		}
		default:
			break;
		}
	};

	static AooInt32 AOO_CALL sendTrampoline(void* user, const AooByte *data, AooInt32 size, const void* addr, AooAddrSize addrlen, AooFlag) {
		auto *self = static_cast<AooReceiveJS*>(user);

		char ipbuf[64];
		AooSize ipsize = sizeof(ipbuf);
		AooUInt16 port = 0;
		aoo_sockAddrToIpEndpoint(addr, addrlen, ipbuf, &ipsize, &port, nullptr);
		self->sendCb_(emscripten::val(emscripten::typed_memory_view(size, data)), std::string(ipbuf, ipsize), (int)port);
		return size;
	}
};