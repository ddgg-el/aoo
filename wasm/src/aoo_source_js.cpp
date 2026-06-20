#include <emscripten/bind.h>
#include "aoo_source_js.hpp"



EMSCRIPTEN_BINDINGS(AooSource) {
	emscripten::class_<AooSourceJS>("AooSource")
		.constructor<AooId>()
		.function("setup", &AooSourceJS::setup)
		.function("setFormat", &AooSourceJS::setFormat)
		.function("addSink", &AooSourceJS::addSink)
		.function("startStream", &AooSourceJS::startStream)
		.function("process", &AooSourceJS::process)
		.function("handleMessage", &AooSourceJS::handleMessage)
		.function("send", &AooSourceJS::send);
}