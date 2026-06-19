#include "aoo.h"
#include "aoo_events.h"
#include "aoo_sink.hpp"
#include "aoo_types.h"
#include <cstddef>
#include <cstdint>
#include <emscripten/val.h>
#include <emscripten/wire.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

class AooSinkJS {
public:
	AooSinkJS(AooId id) : sink_(AooSink::create(id)) {
		sink_->setEventHandler([](void *user, const AooEvent* event, AooThreadLevel) {
			static_cast<AooSinkJS*>(user)->handle_event(*event);
		}, this, kAooEventModeCallback);
	}

	int setup(int c, double sr, int n) { 
		nchannels_ = c;
		blocksize_ = n;
		channels_.assign(c, std::vector<AooSample>(n, 0.0f));
		chanPtrs_.resize(c);

		for (int i = 0; i < c; i++) {
			chanPtrs_[i] = channels_[i].data();
		}

		interleaved_.assign((size_t) c * n, 0.0f);

		return sink_->setup(c, sr, n, 0); 
	};

	int setLatency(double s) { 
		return sink_->setLatency(s); 
	}

	int send(emscripten::val cb) {
		sendCb_ = cb;
		return sink_->send(&AooSinkJS::emitPacket, this);
	}

	int inviteSource(std::string ip, int port, AooId id) {
		AooSockAddrStorage addr;
		AooAddrSize len = sizeof(addr);
		// TODO: check how to use IPV6
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

	emscripten::val processNow() {
		AooNtpTime t = aoo_getCurrentNtpTime();
		sink_->process(chanPtrs_.data(), blocksize_, t, nullptr, nullptr);
		for (int i = 0; i < blocksize_; ++i) {
			for (int c = 0; c < nchannels_; ++c) {
				interleaved_[(size_t) i * nchannels_ + c] = channels_[c][i];
			}
		}
		// std::vector<std::vector<AooSample>>bufs(nchannels_,std::vector<AooSample>(blocksize_, 0.0f));
		// std::vector<AooSample*> ptrs;
		// ptrs.reserve(nchannels_);
		// for(auto& b:bufs) {
		// 	ptrs.push_back(b.data());
		// }

		return emscripten::val(emscripten::typed_memory_view(interleaved_.size(), interleaved_.data()));
	}

private:
	AooSink::Ptr sink_;
	int nchannels_ = 0;
	int blocksize_ = 0;
	std::vector<std::vector<AooSample>> channels_;
	std::vector<AooSample*> chanPtrs_;
	std::vector<AooSample> interleaved_;

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

	// forward each outgoing AOO packet to the JS `send` callback
	static AooInt32 emitPacket(void* user, const AooByte *data, AooInt32 size, const void* addr, AooAddrSize addrlen, AooFlag) {
		auto *self = static_cast<AooSinkJS*>(user);

		char ipbuf[64];
		AooSize ipsize = sizeof(ipbuf);
		AooUInt16 port = 0;
		aoo_sockAddrToIpEndpoint(addr, addrlen, ipbuf, &ipsize, &port, nullptr);
		self->sendCb_(emscripten::val(emscripten::typed_memory_view(size, data)), std::string(ipbuf, ipsize), (int)port);
		return size;
	}
};