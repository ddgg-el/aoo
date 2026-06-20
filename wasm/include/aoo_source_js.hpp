#pragma once

#include "aoo.h"
#include "aoo_events.h"
#include "aoo_source.hpp"
#include "aoo_types.h"
#include "codec/aoo_pcm.h"
#include <cstddef>
#include <cstdint>
#include <emscripten/val.h>
#include <emscripten/wire.h>
#include <string>
#include <vector>


class AooSourceJS {
public:
	AooSourceJS(AooId id) : source_(AooSource::create(id)) {
		source_->setEventHandler([](void *user, const AooEvent* event, AooThreadLevel) {
			static_cast<AooSourceJS*>(user)->handle_event(*event);
		}, this, kAooEventModeCallback);
	}

	int setup(int channels, double sr, int blocksize) {
		nchannels_ = channels;
		samplerate_ = sr;
		blocksize_ = blocksize;
		channels_.assign(channels, std::vector<AooSample>(blocksize, 0.0f));
		chanPtrs_.resize(channels);
		for (int i = 0; i< channels; i++) {
			chanPtrs_[i] = channels_[i].data();
		}
		
		return source_->setup(channels, sr, blocksize, 0);
	}

	int setFormat() {
		AooFormatPcm fmt;
		AooFormatPcm_init(&fmt, nchannels_, samplerate_, blocksize_, kAooPcmFloat32);
		return source_->setFormat(fmt.header);
	}

	int addSink(std::string ip, int port, AooId id) {
		AooSockAddrStorage addr;
		AooAddrSize len = sizeof(addr);

		if(aoo_ipEndpointToSockAddr(ip.c_str(), (AooUInt16) port, kAooSocketIPv4, &addr, &len) != kAooOk) {
			return kAooErrorBadArgument;
		}
		AooEndpoint ep { &addr, len, id};
		return source_->addSink(ep, kAooTrue);
	}

	int startStream() {
		return source_->startStream(0, nullptr);
	}

	int process(emscripten::val input) {
		std::vector<float> inter = emscripten::convertJSArrayToNumberVector<float>(input);
		for (int i = 0; i < blocksize_; ++i) {
			for (int c = 0; c < nchannels_; ++c) {
				channels_[c][i] = inter[(size_t) i * nchannels_ + c];
			}
		}
		return source_->process(chanPtrs_.data(), blocksize_, aoo_getCurrentNtpTime());
	}

	int handleMessage(emscripten::val data, std::string ip, int port) {
		std::vector<uint8_t> bytes = emscripten::convertJSArrayToNumberVector<uint8_t>(data);
		AooSockAddrStorage addr;
		AooAddrSize len = sizeof(addr);

		if(aoo_ipEndpointToSockAddr(ip.c_str(), (AooUInt16) port, kAooSocketIPv4, &addr, &len) != kAooOk) {
			return kAooErrorBadArgument;
		}
		return source_->handleMessage(bytes.data(), (AooInt32) bytes.size(), &addr, len);
	}
	
	int send(emscripten::val cb) {
		sendCb_ = cb;
		return source_->send(&AooSourceJS::emitPacket, this);
	}
	
private:
	AooSource::Ptr source_;
	int nchannels_ = 0;
	double samplerate_ = 0;
	int blocksize_ = 0;
	std::vector<std::vector<AooSample>> channels_;
	std::vector<AooSample*> chanPtrs_;

	emscripten::val sendCb_ = emscripten::val::undefined();
	
	void handle_event(const AooEvent& event) {
		switch (event.type) {
		default:
			break;
		}
	}

	static AooInt32 emitPacket(void* user, const AooByte* data, AooInt32 size, const void* addr, AooAddrSize addrlen, AooFlag) {
		auto* self = static_cast<AooSourceJS*>(user);
		char ipbuf[64];
		AooSize ipsize = sizeof(ipbuf);
		AooUInt16 port = 0;
		aoo_sockAddrToIpEndpoint(addr, addrlen, ipbuf, &ipsize, &port, nullptr);
		self->sendCb_(emscripten::val(emscripten::typed_memory_view(size, data)), std::string(ipbuf, ipsize), (int)port);
		return size;
	}
};