#pragma once

#include "aoo.h"
#include "aoo_common_js.hpp"
#include "aoo_controls.h"
#include "aoo_events.h"
#include "aoo_source.hpp"
#include "aoo_types.h"
#include "codec/aoo_pcm.h"
#include "opus_defines.h"
#include <cstddef>
#include <cstdint>
#include <emscripten/val.h>
#include <emscripten/wire.h>
#include <string>
#include <vector>
#include "codec/aoo_opus.h"


class AooSourceJS {
public:
	AooSourceJS(AooId id) : source_(AooSource::create(id)) {
		source_->setEventHandler([](void *user, const AooEvent* event, AooThreadLevel) {
			static_cast<AooSourceJS*>(user)->handle_event(*event);
		}, this, kAooEventModePoll);
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

	int reset() {
		return source_->reset();
	}

	int setId(int id) {
		return source_->setId(id);
	}

	int removeAllSinks() {
		return source_->removeAllSinks();
	}

	int removeSink(std::string ip, int port, AooId id) {
		auto e = resolveEndpoint(ip, port, id);
		if(!e.ok) return kAooErrorBadArgument;
		return source_->removeSink(e.endpoint());
	}

	int activate(std::string ip, int port, AooId id, bool active) {
		auto e = resolveEndpoint(ip, port, id);
		if(!e.ok) return kAooErrorBadArgument;
		return source_->activate(e.endpoint(), active ? kAooTrue : kAooFalse);
	}

	int setSinkChannelOffset(std::string ip, int port, AooId id, int onset) {
		auto e = resolveEndpoint(ip, port, id);
		if(!e.ok) return kAooErrorBadArgument;
		return source_->setSinkChannelOffset(e.endpoint(), onset);
	}

	int setPacketSize(int bytes) {
		return source_->setPacketSize(bytes);
	} 

	int setPingInterval(double seconds) {
		return source_->setPingInterval(seconds);
	}

	int setDllBandwidth(double q) {
		float bw = (float)q;
		// return source_->setDllBandwidth(q);
		// FIXME: bug in source.cpp
		return source_->control(kAooCtlSetDllBandwidth, 0, &bw, sizeof(bw));
	}

	int setRedundancy(int n) {
		return source_->setRedundancy(n);
	}

	int setResendBufferSize(double seconds) {
		return source_->setResendBufferSize(seconds);
	}

	int setBinaryFormat(bool enabled) {
		return source_->setBinaryFormat(enabled);
	}

	int setStreamTimeSendInterval(double seconds) {
		return source_->setStreamTimeSendInterval(seconds);
	}

	int setResampleMethod(int mode) {
		return source_->setResampleMethod((AooResampleMethod)mode);
	}

	// TODO: implement OPUS and resampling!
	int setFormat() {
		AooFormatPcm fmt;
		AooFormatPcm_init(&fmt, nchannels_, samplerate_, blocksize_, kAooPcmFloat32);
		return source_->setFormat(fmt.header);
	}

	int addSink(std::string ip, int port, AooId id) {
		auto e = resolveEndpoint(ip, port, id);
		if(!e.ok) return kAooErrorBadArgument;
		return source_->addSink(e.endpoint(), kAooTrue);
	}

	int handleInvite(std::string ip, int port, AooId id, AooId token, bool accept) {
		auto e = resolveEndpoint(ip, port, id);
		if(!e.ok) return kAooErrorBadArgument;
		return source_->handleInvite(e.endpoint(), token, accept ? kAooTrue : kAooFalse);
	}

	int handleUninvite(std::string ip, int port, AooId id, AooId token, bool accept) {
		auto e = resolveEndpoint(ip, port, id);
		if(!e.ok) return kAooErrorBadArgument;
		return source_->handleUninvite(e.endpoint(), token, accept ? kAooTrue : kAooFalse);
	}

	int startStream() {
		return source_->startStream(0, nullptr);
	}

	/// Stop the current stream (sampleOffset: where within the next block to stop).
	int stopStream(int sampleOffset) {
		return source_->stopStream(sampleOffset);
	}
	/// Enable/disable dynamic resampling (compensates source/sink clock drift).
	int setDynamicResampling(bool enabled) {
		return source_->setDynamicResampling(enabled ? kAooTrue : kAooFalse);
	}
	/// Time buffer size, in seconds.
	int setBufferSize(double seconds) {
		return source_->setBufferSize(seconds);
	}
	/// Measured real sample rate (Hz).
	double getRealSampleRate() {
		AooSampleRate sr = 0;
		source_->getRealSampleRate(sr);
		return sr;
	}
	/// Whether queued events are pending (poll mode).
	/// should we add this feature to the examples? I mean in the pool loop?
	bool eventsAvailable() {
		return source_->eventsAvailable();
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

	int addStreamMessage(int type, emscripten::val data, int sampleOffset, int channel) {
		std::vector<uint8_t> bytes = emscripten::convertJSArrayToNumberVector<uint8_t>(data);
		AooStreamMessage msg;
		msg.sampleOffset = sampleOffset;
		msg.channel = channel;
		msg.type = (AooDataType) type;
		msg.size = (AooInt32) bytes.size();
		msg.data = bytes.data();
		return source_->addStreamMessage(msg);
	}

	int setEventHandler(emscripten::val cb) {
		eventCb_ = cb;
		return kAooOk;
	}

	int pollEvents() { return source_->pollEvents(); }

	AooError processWorklet(AooSample** planar, int nframes) {
		return source_->process(planar, nframes, aoo_getCurrentNtpTime());
	}

	int channels() const { return nchannels_; }

#pragma region OPUS
	int setFormatOpus(int applicationType, int blockSize, int bitrate, int complexity) {
		AooFormatOpus fmt;
		AooFormatOpus_init(&fmt, nchannels_, 48000, 480, applicationType);
		AooError err = source_->setFormat(fmt.header);
		if(err != kAooOk) { return err; };
		if(bitrate > 0) setOpusBitrate(bitrate);
		if(complexity >=0) setOpusComplexity(complexity);
		return kAooOk;
	}

	int setOpusBitrate(int bitrate) {
		return source_->codecControl(kAooCodecOpus, OPUS_SET_BITRATE_REQUEST, 0, &bitrate, sizeof(bitrate));
	}

	int setOpusComplexity(int complexity) {
		return source_->codecControl(kAooCodecOpus, OPUS_SET_COMPLEXITY_REQUEST, 0, &complexity, sizeof(complexity));
	}

	int setOpusSignalType(int signalType) {
		return source_->codecControl(kAooCodecOpus, OPUS_SET_SIGNAL_REQUEST, 0, &signalType, sizeof(signalType));
	}

#pragma region PRIVATE MEMBERS
private:
	AooSource::Ptr source_;
	int nchannels_ = 0;
	double samplerate_ = 0;
	int blocksize_ = 0;
	std::vector<std::vector<AooSample>> channels_;
	std::vector<AooSample*> chanPtrs_;

	emscripten::val eventCb_ = emscripten::val::undefined();
	emscripten::val sendCb_ = emscripten::val::undefined();
#pragma region PRIVATE METHODS
	void handle_event(const AooEvent& event) {
		if(eventCb_.isUndefined()) return;
		auto ev = emscripten::val::object();
		switch (event.type) {
		case kAooEventSinkAdd:
			ev.set("type", std::string("sinkAdd"));
			ev.set("endpoint", endPointToVal(event.sinkAdd.endpoint));
			break;
		case kAooEventSinkRemove:
			ev.set("type", std::string("sinkRemove"));
			ev.set("endpoint", endPointToVal(event.sinkRemove.endpoint));
			break;
		case kAooEventSinkPing: {
			const auto& p = event.sinkPing;
			double rtt = aoo_ntpTimeToSeconds(p.t4 - p.t1) - aoo_ntpTimeToSeconds(p.t3 - p.t2);
			ev.set("type", std::string("sinkPing"));
			ev.set("endpoint", endPointToVal(p.endpoint));
			ev.set("rtt", rtt);
			ev.set("packetLoss", p.packetLoss);
			break;
		}
		case kAooEventInvite:
			ev.set("type", std::string("invite"));
			ev.set("endpoint", endPointToVal(event.invite.endpoint));
			ev.set("token", event.invite.token);
			break;
		case kAooEventUninvite:
			ev.set("type", std::string("uninvite"));
			ev.set("endpoint", endPointToVal(event.uninvite.endpoint));
			ev.set("token", event.uninvite.token);
			break;
		case kAooEventFrameResend:
			ev.set("type", std::string("frameResend"));
			ev.set("endpoint", endPointToVal(event.frameResend.endpoint));
			ev.set("count", event.frameResend.count);
			break;
		default:
			ev.set("type", (int)event.type);
			return;
		}
		eventCb_(ev);
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