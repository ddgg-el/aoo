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
		.function("processNow", &AooSinkJS::processNow);

}