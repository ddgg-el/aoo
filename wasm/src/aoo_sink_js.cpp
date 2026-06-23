#include <emscripten/bind.h>
#include "aoo_sink_js.hpp"



EMSCRIPTEN_BINDINGS(AooSink) {
	emscripten::class_<AooSinkJS>("AooSink")
		.constructor<AooId>()
		.function("setup", &AooSinkJS::setup)
		.function("reset", &AooSinkJS::reset)
		.function("setId", &AooSinkJS::setId)
		.function("setPacketSize", &AooSinkJS::setPacketSize)
		.function("setPingInterval", &AooSinkJS::setPingInterval)
		.function("setDllBandwidth", &AooSinkJS::setDllBandwidth)
		.function("setResendData", &AooSinkJS::setResendData)
		.function("setResendInterval", &AooSinkJS::setResendInterval)
		.function("setResendLimit", &AooSinkJS::setResendLimit)
		.function("setBinaryFormat", &AooSinkJS::setBinaryFormat)
		.function("setResampleMethod", &AooSinkJS::setResampleMethod)
		.function("setLatency", &AooSinkJS::setLatency)
		.function("handleMessage", &AooSinkJS::handleMessage)
		.function("send", &AooSinkJS::send)
		.function("inviteSource", &AooSinkJS::inviteSource)
		.function("uninviteSource", &AooSinkJS::uninviteSource)
		.function("resetSource", &AooSinkJS::resetSource)
		.function("getBufferFillRatio", &AooSinkJS::getBufferFillRatio)
		.function("uninviteAll", &AooSinkJS::uninviteAll)
		.function("setDynamicResampling", &AooSinkJS::setDynamicResampling)
		.function("setBufferSize", &AooSinkJS::setBufferSize)
		.function("getRealSampleRate", &AooSinkJS::getRealSampleRate)
		.function("eventsAvailable", &AooSinkJS::eventsAvailable)
		.function("process", &AooSinkJS::process)
		.function("setEventHandler", &AooSinkJS::setEventHandler)
		.function("pollEvents", &AooSinkJS::pollEvents)
		.function("pollStreamMessages", &AooSinkJS::pollStreamMessages)
		.function("playbackSample", &AooSinkJS::playbackSample)
		.function("playbackTime",   &AooSinkJS::playbackTime)
		.function("streamMessagesDropped", &AooSinkJS::streamMessagesDropped)
		.function("setStreamMessageHandler", &AooSinkJS::setStreamMessageHandler);

}