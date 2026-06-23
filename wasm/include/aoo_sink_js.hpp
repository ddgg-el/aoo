#pragma once

#include "aoo.h"
#include "aoo_common_js.hpp"
#include "aoo_events.h"
#include "aoo_sink.hpp"
#include "aoo_types.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <emscripten/val.h>
#include <emscripten/wire.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

/**
 * embind wrapper around AOO's AooSink.
 *
 * Owns an AooSink and exposes a selected subset of its API to JavaScript.
 * There are two audio paths:
 *   - process()        : single-threaded (Node). Renders a block, returns an
 *                        interleaved view, and dispatches stream messages
 *                        synchronously on the calling (main) thread.
 *   - processWorklet() : threaded (browser). Runs on the AudioWorklet thread,
 *                        writes into caller-provided planar buffers, and queues
 *                        stream messages into a lock-free ring for the main thread.
 *
 * Events use AOO's polling mode: handle_event() builds JS event objects, but they
 * are only delivered when pollEvents() is called (on the main thread).
 */

 // TODO: implement resampling
class AooSinkJS {
public:
	/// Create the underlying AooSink and register the (poll-mode) event handler.
	AooSinkJS(AooId id) : sink_(AooSink::create(id)) {
		sink_->setEventHandler([](void *user, const AooEvent* event, AooThreadLevel) {
			static_cast<AooSinkJS*>(user)->handle_event(*event);
		}, this, kAooEventModePoll);
	}

	/// Configure channel count, sample rate and block size; (re)allocates the
	/// planar/interleaved audio buffers. Returns the AooError from AooSink::setup.
	int setup(int c, double sr, int n) {
		nchannels_ = c;
		blocksize_ = n;
		samplerate_ = sr;
		channels_.assign(c, std::vector<AooSample>(n, 0.0f));
		chanPtrs_.resize(c);

		for (int i = 0; i < c; i++) {
			chanPtrs_[i] = channels_[i].data();
		}

		interleaved_.assign((size_t) c * n, 0.0f);

		return sink_->setup(c, sr, n, 0);
	}

	int reset() {
		return sink_->reset();
	}

	int setId(int id) {
		return sink_->setId(id);
	}

	int setPacketSize(int bytes) {
		return sink_->setPacketSize(bytes);
	}

	int setPingInterval(double seconds) {
		return sink_->setPingInterval(seconds);
	}

	int setDllBandwidth(double q) {
		float bw = (float)q;
		// return sink_->setDllBandwidth(q);
		// FIXME: bug in in sink.cpp
		return sink_->control(kAooCtlSetDllBandwidth, 0, &bw, sizeof(bw));
	}

	int setResendData(bool enabled) {
		return sink_->setResendData(enabled);
	}

	int setResendInterval(double seconds) {
		return sink_->setResendInterval(seconds);
	}

	int setResendLimit(int n) {
		return sink_->setResendLimit(n);
	}

	int setBinaryFormat(bool enabled) {
		return sink_->setBinaryFormat(enabled);
	}

	int setResampleMethod(int mode) {
		return sink_->setResampleMethod((AooResampleMethod) mode);
	}

	/// Set the jitter-buffer latency in seconds.
	int setLatency(double s) {
		return sink_->setLatency(s);
	}

	/// Store the JS callback that receives outgoing packets, then flush the sink's
	/// outbox through it. Call on the main thread.
	int send(emscripten::val cb) {
		sendCb_ = cb;
		return sink_->send(&AooSinkJS::emitPacket, this);
	}

	/// Register the JS callback invoked for each incoming stream message.
	int setStreamMessageHandler(emscripten::val cb) {
		msgCb_ = cb;
		return kAooOk;
	}

	/// Register the JS callback invoked for each event drained by pollEvents().
	int setEventHandler(emscripten::val cb) {
		eventCb_ = cb;
		return kAooOk;
	}

	/// Drain queued AOO events to the event handler. Call on the main thread.
	int pollEvents() { return sink_->pollEvents(); }

	/// Consumer side of the stream-message ring: drain queued messages to msgCb_.
	/// Call on the main thread (next to pollEvents).
	int pollStreamMessages() {
		uint32_t r = msgRead_.load(std::memory_order_relaxed);
		uint32_t w = msgWrite_.load(std::memory_order_acquire);
		while(r !=w) {
			MsgSlot& s = msgRing_[r & (kMsgRingSize - 1)];
			if(!msgCb_.isUndefined()) {
				char ipbuf[64];
				AooSize ipsize = sizeof(ipbuf);
				AooUInt16 port = 0;
				aoo_sockAddrToIpEndpoint(&s.addr, s.addrlen, ipbuf, &ipsize, &port, nullptr);
				auto o = emscripten::val::object();
				o.set("sampleOffset", s.sampleOffset);
				o.set("channel", s.channel);
				o.set("type", s.type);
				o.set("data", emscripten::typed_memory_view((size_t)s.size, s.data));
				o.set("streamSample", (double)s.dueSample);
				o.set("time", samplerate_ > 0 ? (double) s.dueSample / samplerate_ : 0);
				auto src = emscripten::val::object();
				src.set("ip", std::string(ipbuf, ipsize));
				src.set("port", (int)port);
				src.set("id", s.id);
				o.set("source", src);
				msgCb_(o);
			}
			msgRead_.store(++r, std::memory_order_release);
		}
		return kAooOk;
	}

	/// Total samples rendered by the worklet so far (the shared playback clock).
	double playbackSample() {
		return (double)playSamples_.load(std::memory_order_acquire);
	}

	/// Current playback position in seconds (playbackSample / sample rate).
	double playbackTime() {
		return samplerate_ > 0 ? playbackSample() / samplerate_ : 0.0;
	}

	/// Number of stream messages dropped on the producer side
	/// (ring full or payload larger than kMsgMaxBytes).
	int streamMessagesDropped() {
		return (int)msgDropped_.load(std::memory_order_relaxed);
	}

	/// Actively invite a source endpoint. `ip` must be an IPv4/IPv6 literal (no DNS).
	int inviteSource(std::string ip, int port, AooId id) {
		AooSockAddrStorage addr;
		AooAddrSize len = sizeof(addr);

		if(aoo_ipEndpointToSockAddr(ip.c_str(), (AooUInt16) port, kAooSocketAnyFamily, &addr, &len) != kAooOk) return kAooErrorBadArgument;

		AooEndpoint ep { &addr, len, id};
		return sink_->inviteSource(ep, nullptr);
	}

	int uninviteSource(std::string ip, int port, AooId id) {
		AooSockAddrStorage addr; AooAddrSize len;
		if (!ipToSockAddr(ip, port, addr, len)) return kAooErrorBadArgument;
		AooEndpoint ep { &addr, len, id };
		return sink_->uninviteSource(ep);
	}
	int resetSource(std::string ip, int port, AooId id) {
		AooSockAddrStorage addr; AooAddrSize len;
		if (!ipToSockAddr(ip, port, addr, len)) return kAooErrorBadArgument;
		AooEndpoint ep { &addr, len, id };
		return sink_->resetSource(ep);
	}
	double getBufferFillRatio(std::string ip, int port, AooId id) {
		AooSockAddrStorage addr; AooAddrSize len;
		if (!ipToSockAddr(ip, port, addr, len)) return 0.0;
		AooEndpoint ep { &addr, len, id };
		double ratio = 0.0;
		sink_->getBufferFillRatio(ep, ratio);
		return ratio;
	}

	int uninviteAll() {
		return sink_->uninviteAll();
	}

	int setDynamicResampling(bool enabled) {
		return sink_->setDynamicResampling(enabled);
	}

	int setBufferSize(double seconds) {
		return sink_->setBufferSize(seconds);
	}

	double getRealSampleRate() {
		AooSampleRate sr = 0;
		sink_->getRealSampleRate(sr);
		return sr;
	}

	bool eventsAvailable() {
		return sink_->eventsAvailable();
	}

	/// Feed an incoming packet into the sink. `ip` must be an IPv4/IPv6 literal (no DNS).
	int handleMessage(emscripten::val data, std::string ip, int port) {
		std::vector<uint8_t> bytes = emscripten::convertJSArrayToNumberVector<uint8_t>(data);

		AooSockAddrStorage addr;
		AooAddrSize len = sizeof(addr);
		if(aoo_ipEndpointToSockAddr(ip.c_str(), (AooUInt16) port, kAooSocketAnyFamily, &addr, &len) != kAooOk) return kAooErrorBadArgument;

		return sink_->handleMessage(bytes.data(), (AooInt32)bytes.size(), &addr, len);
	}

	/// Single-threaded (Node) audio tick: render one block, interleave it, and return
	/// a view of the interleaved buffer. Stream messages are dispatched synchronously
	/// via handleStreamMessage.
	emscripten::val process() {
		AooNtpTime t = aoo_getCurrentNtpTime();
		sink_->process(chanPtrs_.data(), blocksize_, t, &AooSinkJS::handleStreamMessage, this);
		for (int i = 0; i < blocksize_; ++i) {
			for (int c = 0; c < nchannels_; ++c) {
				interleaved_[(size_t) i * nchannels_ + c] = channels_[c][i];
			}
		}
		return emscripten::val(emscripten::typed_memory_view(interleaved_.size(), interleaved_.data()));
	}
	/// AudioWorklet-thread audio tick: render `nframes` directly into the caller's
	/// planar output buffers, queue any stream messages, and advance the shared
	/// playback sample clock.
	// TODO: return AooError when possible
	AooError processWorklet(AooSample** planar, int nframes) {
		blockStartSample_ = streamSamples_;
		AooError err = sink_->process(planar, nframes, aoo_getCurrentNtpTime(), &AooSinkJS::queueStreamMessage, this);
		streamSamples_ += nframes;
		playSamples_.store(streamSamples_, std::memory_order_release);
		return err;
	}
#pragma region "PRIVATE MEMBERS"
private:
	AooSink::Ptr sink_;                             ///< the owned AOO sink
	int nchannels_ = 0;                             ///< channel count (from setup)
	int blocksize_ = 0;                             ///< block size (from setup)
	double samplerate_ = 0;                         ///< sample rate (from setup)
	std::vector<std::vector<AooSample>> channels_;  ///< planar output buffers (Node path)
	std::vector<AooSample*> chanPtrs_;              ///< pointers into channels_ (Node path)
	std::vector<AooSample> interleaved_;            ///< interleaved scratch returned by process()

	int64_t streamSamples_ = 0;                     ///< worklet-only: total samples rendered
	int64_t blockStartSample_ = 0;                  ///< worklet-only: first sample of the current block
	std::atomic<int64_t>playSamples_{0};            ///< published playback clock (worklet -> main)

	emscripten::val eventCb_ = emscripten::val::undefined();   ///< JS event handler
	emscripten::val sendCb_ = emscripten::val::undefined();    ///< JS outgoing-packet handler
	emscripten::val msgCb_ = emscripten::val::undefined();     ///< JS stream-message handler

	MsgSlot msgRing_[kMsgRingSize];                 ///< lock-free SPSC ring of stream messages
	std::atomic<uint32_t> msgWrite_{0};             ///< ring producer index (audio thread)
	std::atomic<uint32_t> msgRead_{0};              ///< ring consumer index (main thread)
	std::atomic<uint32_t> msgDropped_{0};           ///< dropped-message counter

#pragma region "PRIVATE METHODS"
	/// Build a JS object for one AooEvent and pass it to eventCb_ (poll-mode delivery).
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
		// remaining event types: not individually marshalled to JS
		case kAooEventStreamTime: {
			const auto& t = event.streamTime;
			ev.set("type", std::string("streamTime"));
			ev.set("endpoint", endPointToVal(t.endpoint));
			ev.set("sourceTime", aoo_ntpTimeToSeconds(t.sourceTime));
			ev.set("sinkTime", aoo_ntpTimeToSeconds(t.sinkTime));
			ev.set("sampleOffset", t.sampleOffset);
			break;
		}	
		case kAooEventBufferUnderrun:
			ev.set("type", std::string("bufferUnderrun"));
			ev.set("endpoint", endPointToVal(event.bufferUnderrun.endpoint));
			break;
		case kAooEventBufferOverrun:
			ev.set("type", std::string("bufferOverrun"));
			ev.set("endpoint", endPointToVal(event.bufferOverrrun.endpoint)); // NB: AOO's union field is spelled with 3 r's
			break;
		case kAooEventBlockDrop:
			ev.set("type", std::string("blockDrop"));
			ev.set("endpoint", endPointToVal(event.blockDrop.endpoint));
			ev.set("count", event.blockDrop.count);
			break;
		case kAooEventBlockResend:
			ev.set("type", std::string("blockResend"));
			ev.set("endpoint", endPointToVal(event.blockResend.endpoint));
			ev.set("count", event.blockResend.count);
			break;
		case kAooEventBlockXRun:
			ev.set("type", std::string("blockXRun"));
			ev.set("endpoint", endPointToVal(event.blockXRun.endpoint));
			ev.set("count", event.blockXRun.count);
			break;
		default:
			ev.set("type", (int)event.type);
			return;
		}
		eventCb_(ev);
	};

	/// AOO send trampoline: forward one outgoing packet to sendCb_(bytes, ip, port).
	static AooInt32 emitPacket(void* user, const AooByte *data, AooInt32 size, const void* addr, AooAddrSize addrlen, AooFlag) {
		auto *self = static_cast<AooSinkJS*>(user);

		char ipbuf[64];
		AooSize ipsize = sizeof(ipbuf);
		AooUInt16 port = 0;
		aoo_sockAddrToIpEndpoint(addr, addrlen, ipbuf, &ipsize, &port, nullptr);
		self->sendCb_(emscripten::val(emscripten::typed_memory_view(size, data)), std::string(ipbuf, ipsize), (int)port);
		return size;
	}

	/// Stream-message handler for the single-threaded (Node) path: deliver to msgCb_
	/// immediately (runs on the main thread during process()).
	static void handleStreamMessage(void* user, const AooStreamMessage* m, const AooEndpoint* source) {
		auto* self = static_cast<AooSinkJS*>(user);
		if(self->msgCb_.isUndefined()) return;
		auto o = emscripten::val::object();
		o.set("sampleOffset", m->sampleOffset);
		o.set("channel", m->channel);
		o.set("type", (int)m->type);
		o.set("data", emscripten::val(emscripten::typed_memory_view(m->size, m->data)));
		o.set("source", endPointToVal(*source));
		self->msgCb_(o);
	}

	/// Stream-message handler for the worklet path: trampoline into enqueueStreamMessage.
	static void queueStreamMessage(void* user, const AooStreamMessage* msg, const AooEndpoint* ep) {
		static_cast<AooSinkJS*>(user)->enqueueStreamMessage(*msg, *ep);
	}

	/// Producer side of the ring (audio/worklet thread): copy the message + endpoint
	/// into a free slot and publish it. RT-safe — no allocation, no locks. Drops the
	/// message (incrementing msgDropped_) if it is too large or the ring is full.
	void enqueueStreamMessage(const AooStreamMessage& msg, const AooEndpoint& ep) {
		if(msg.size > kMsgMaxBytes) {
			msgDropped_.fetch_add(1, std::memory_order_relaxed);
			return;
		}
		uint32_t w = msgWrite_.load(std::memory_order_relaxed);
		uint32_t r = msgRead_.load(std::memory_order_acquire);
		if(w - r >= kMsgRingSize) {
			msgDropped_.fetch_add(1, std::memory_order_relaxed);
			return;
		}

		MsgSlot& s = msgRing_[w & (kMsgRingSize - 1)];
		s.sampleOffset = msg.sampleOffset;
		s.channel = msg.channel;
		s.type = msg.type;
		s.size = msg.size;
		std::memcpy(s.data, msg.data, msg.size);
		std::memcpy(&s.addr, ep.address, ep.addrlen);
		s.dueSample = blockStartSample_ + msg.sampleOffset;
		s.addrlen = ep.addrlen;
		s.id = ep.id;
		msgWrite_.store(w+1, std::memory_order_release);
	}
};
