#include <emscripten/bind.h>
#include "aoo_js_receive.hpp"



EMSCRIPTEN_BINDINGS(AooSink) {
	emscripten::class_<AooReceiveJS>("AooReceive")
		.constructor<AooId>()
		.function("setup", &AooReceiveJS::setup)
		.function("setLatency", &AooReceiveJS::setLatency)
		.function("handleMessage", &AooReceiveJS::handleMessage)
		.function("send", &AooReceiveJS::send)
		.function("inviteSource", &AooReceiveJS::inviteSource)
		.function("process", &AooReceiveJS::process)
		.function("processNow", &AooReceiveJS::processNow);

}