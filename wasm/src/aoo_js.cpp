#include <emscripten/bind.h>
#include <string>
#include "aoo.h"
#include "aoo_types.h"


static int aooInitialize() {
	return aoo_initialize(nullptr);
}

static std::string aooVersion() {
	return aoo_getVersionString();
}

EMSCRIPTEN_BINDINGS(aoo_core) {
	emscripten::function("initialize", &aooInitialize);
	emscripten::function("terminate", &aoo_terminate);
	emscripten::function("versionString", &aooVersion);
	
	emscripten::constant("kAooDataRaw", (int)kAooDataRaw);
	emscripten::constant("kAooDataText", (int)kAooDataText);
	emscripten::constant("kAooDataOSC", (int)kAooDataOSC);
	emscripten::constant("kAooDataMIDI", (int)kAooDataMIDI);
	emscripten::constant("kAooDataJSON", (int)kAooDataJSON);
}