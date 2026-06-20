#pragma once

#include "aoo.h"
#include "aoo_common_js.hpp"
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

	int setEventHandler(emscripten::val cb) {
		eventCb_ = cb;
		return kAooOk;
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

	emscripten::val process() {
		AooNtpTime t = aoo_getCurrentNtpTime();
		sink_->process(chanPtrs_.data(), blocksize_, t, nullptr, nullptr);
		for (int i = 0; i < blocksize_; ++i) {
			for (int c = 0; c < nchannels_; ++c) {
				interleaved_[(size_t) i * nchannels_ + c] = channels_[c][i];
			}
		}
		return emscripten::val(emscripten::typed_memory_view(interleaved_.size(), interleaved_.data()));
	}

private:
	AooSink::Ptr sink_;
	int nchannels_ = 0;
	int blocksize_ = 0;
	std::vector<std::vector<AooSample>> channels_;
	std::vector<AooSample*> chanPtrs_;
	std::vector<AooSample> interleaved_;

	emscripten::val eventCb_ = emscripten::val::undefined();
	emscripten::val sendCb_ = emscripten::val::undefined();

	void handle_event(const AooEvent& event) {
		if(eventCb_.isUndefined()) return;
		auto ev = emscripten::val::object();

		switch (event.type) {
		case kAooEventSourceAdd:
			ev.set("type", std::string("sourceAdd"));
			ev.set("endpoint", endPointToVal(event.sourceAdd.endpoint));
			break;
		case kAooEventSourceRemove:
			ev.set("type", std::string("sourceRemove"));
			ev.set("endpoint", endPointToVal(event.sourceRemove.endpoint));
			break;
		case kAooEventSourcePing: {
			const auto& p = event.sourcePing;
			double rtt = aoo_ntpTimeToSeconds(p.t4 - p.t1) - aoo_ntpTimeToSeconds(p.t3 - p.t2);
			ev.set("type", std::string("sourcePing"));
			ev.set("endpoint", endPointToVal(p.endpoint));
			ev.set("rtt", rtt);
			break;
		}
		case kAooEventStreamStart:
			ev.set("type", std::string("streamStart"));
			ev.set("endpoint", endPointToVal(event.streamStart.endpoint));
			break;
		case kAooEventStreamStop:
			ev.set("type", std::string("streamStop"));
			ev.set("endpoint", endPointToVal(event.streamStop.endpoint));
			break;
		case kAooEventStreamState: {
			const auto& s = event.streamState;
			const char* name = s.state == kAooStreamStateActive ? "active" : s.state == kAooStreamStateBuffering ? "buffering" : "inactive";
			ev.set("type", std::string("streamState"));
			ev.set("endpoint", endPointToVal(s.endpoint));
			ev.set("state", std::string(name));
			ev.set("sampleOffset", s.sampleOffset);
			break;
		}
		case kAooEventStreamLatency: {
			const auto& l = event.streamLatency;
			ev.set("type", std::string("streamLatency"));
			ev.set("endpoint", endPointToVal(l.endpoint));
			ev.set("sourceLatency", l.sourceLatency);
			ev.set("sinkLatency", l.sinkLatency);
			ev.set("bufferLatency", l.bufferLatency);
			break;
		}
		case kAooEventFormatChange: {
			const AooFormat* f = event.formatChange.format;
			ev.set("type", std::string("formatChange"));
			ev.set("endpoint", endPointToVal(event.formatChange.endpoint));
			ev.set("codec", f->codecName);
			ev.set("channels", f->numChannels);
			ev.set("sampleRate", f->sampleRate);
			ev.set("blockSize", f->blockSize);
			break;
		}
		case kAooEventStreamTime:
		case kAooEventBufferUnderrun:
		case kAooEventBufferOverrun:
		case kAooEventBlockDrop:
		case kAooEventBlockResend:
		case kAooEventBlockXRun:
		default:
			ev.set("type", (int)event.type);
			return;
		}
		eventCb_(ev);
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