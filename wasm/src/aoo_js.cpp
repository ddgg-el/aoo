#include <cstdint>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <string>
#include <vector>
#include "aoo.h"
#include "aoo_types.h"
#include "opus_defines.h"


static int aooInitialize() {
	return aoo_initialize(nullptr);
}

static std::string aooVersion() {
	return aoo_getVersionString();
}

static std::string aooStrerror(int err) {
	return aoo_strerror((AooError) err);
}

static int messageType(emscripten::val data) {
	std::vector<uint8_t> bytes = emscripten::convertJSArrayToNumberVector<uint8_t>(data);
	AooMsgType type;
	AooId id;
	AooInt32 offset;
	if(aoo_parsePattern((const AooByte*)bytes.data(), (AooInt32)bytes.size(), &type, &id, &offset) != kAooOk) return -1;
	return (int)type;
}

EMSCRIPTEN_BINDINGS(aoo_core) {
	emscripten::function("initialize", &aooInitialize);
	emscripten::function("terminate", &aoo_terminate);
	emscripten::function("versionString", &aooVersion);

	emscripten::function("strerror", &aooStrerror);

	emscripten::function("messageType", &messageType);

	emscripten::constant("kAooDataRaw", (int)kAooDataRaw);
	emscripten::constant("kAooDataText", (int)kAooDataText);
	emscripten::constant("kAooDataOSC", (int)kAooDataOSC);
	emscripten::constant("kAooDataMIDI", (int)kAooDataMIDI);
	emscripten::constant("kAooDataJSON", (int)kAooDataJSON);

	emscripten::constant("kAooResampleHold",   (int) kAooResampleHold);
	emscripten::constant("kAooResampleLinear", (int) kAooResampleLinear);
	emscripten::constant("kAooResampleCubic",  (int) kAooResampleCubic);

	emscripten::constant("kAooMsgTypeSource", (int)kAooMsgTypeSource);
	emscripten::constant("kAooMsgTypeSink",   (int)kAooMsgTypeSink);

	emscripten::constant("OPUS_APPLICATION_AUDIO", (int)OPUS_APPLICATION_AUDIO);
	emscripten::constant("OPUS_APPLICATION_RESTRICTED_LOWDELAY", (int)OPUS_APPLICATION_RESTRICTED_LOWDELAY);
	emscripten::constant("OPUS_APPLICATION_VOIP", (int)OPUS_APPLICATION_VOIP);

	emscripten::constant("OPUS_SIGNAL_MUSIC", (int)OPUS_SIGNAL_MUSIC);
	emscripten::constant("OPUS_SIGNAL_VOICE", (int)OPUS_SIGNAL_VOICE);
	emscripten::constant("OPUS_AUTO", (int)OPUS_AUTO);
	
}