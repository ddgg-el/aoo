#include <emscripten/bind.h>
#include "aoo_sink_js.hpp"



EMSCRIPTEN_BINDINGS(AooSink) {
	emscripten::class_<AooSinkJS>("AooSink")
		.constructor<AooId>()
		.function("setup", &AooSinkJS::setup)
		.function("setLatency", &AooSinkJS::setLatency)
		.function("handleMessage", &AooSinkJS::handleMessage)
		.function("send", &AooSinkJS::send)
		.function("inviteSource", &AooSinkJS::inviteSource)
		.function("process", &AooSinkJS::process)
		.function("setEventHandler", &AooSinkJS::setEventHandler)
		.function("pollEvents", &AooSinkJS::pollEvents)
		.function("pollStreamMessages", &AooSinkJS::pollStreamMessages)
		.function("playbackSample", &AooSinkJS::playbackSample)
		.function("playbackTime",   &AooSinkJS::playbackTime)
		.function("streamMessagesDropped", &AooSinkJS::streamMessagesDropped)
		.function("setStreamMessageHandler", &AooSinkJS::setStreamMessageHandler);

}