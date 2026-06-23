// Browser-only glue (compiled into the threaded `aoo.web` build): drives an
// AooSinkJS from an emscripten Wasm AudioWorklet. AooSink::processWorklet() runs on
// the dedicated audio thread; send/handleMessage/poll* stay on the main thread.

#include "aoo_sink_js.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <emscripten/bind.h>
#include <emscripten/em_js.h>
#include <emscripten/em_macros.h>
#include <emscripten/val.h>
#include <emscripten/webaudio.h>

static uint8_t g_audioStack[256 * 1024] __attribute__((aligned(16)));    ///< stack for the audio worklet thread
static AooSinkJS *g_sink = nullptr;                                      ///< the sink currently driving output
static std::atomic<int>g_processCalls{0};                             ///< debug: number of render callbacks run
static int g_channels = 1;                                              ///< output channel count
static EMSCRIPTEN_WEBAUDIO_T g_ctx = 0;
static emscripten::val g_nodeReadyCb = emscripten::val::undefined();                                ///< the created AudioContext handle

/// AudioWorklet render callback (runs on the audio thread). Builds planar channel
/// pointers into the output frame and renders one quantum through the sink.
static bool processWorklet(int numInputs, const AudioSampleFrame* input, int numOutputs, AudioSampleFrame* output, int numParams, const AudioParamFrame* params, void* user) {
	if(!g_sink) return true;
	AudioSampleFrame &out = output[0];
	AooSample* planar[32];
	int nChannels = output->numberOfChannels < 32 ? output->numberOfChannels : 32;
	for (int ch = 0; ch < nChannels; ch++) {
		planar[ch] = &out.data[ch * out.samplesPerChannel];
	}
	g_sink->processWorklet(planar, out.samplesPerChannel);
	g_processCalls.fetch_add(1, std::memory_order_relaxed);
	return true;
}

/// explicitly declared dependencies 
EM_JS_DEPS(aoo_audio_worklet_deps, "$emscriptenRegisterAudioObject,$emscriptenGetAudioObject")

/// Register a JS AudioContext into emscripten's audio-object table; return its handle.
EM_JS(EMSCRIPTEN_WEBAUDIO_T, js_registerAudioContext, (emscripten::EM_VAL ctx), {
	return emscriptenRegisterAudioObject(Emval.toValue(ctx));
})

/// Fetch the JS object for an audio handle, as an emval handle.
EM_JS(emscripten::EM_VAL, js_getAudioObject, (EMSCRIPTEN_WEBAUDIO_T handle), {
	return Emval.toHandle(emscriptenGetAudioObject(handle));
})

/// Worklet-processor-created callback: create the worklet node and connect it.
static void onProcessorCreated(EMSCRIPTEN_WEBAUDIO_T ctx, bool ok, void* user) {
	if(!ok) { printf("processor FAILED\n"); return; };
	int counts[1] = { g_channels };
	EmscriptenAudioWorkletNodeCreateOptions opts = {};
	opts.numberOfInputs = 0;
	opts.numberOfOutputs = 1;
	opts.outputChannelCounts = counts;
	EMSCRIPTEN_WEBAUDIO_T node = emscripten_create_wasm_audio_worklet_node(ctx, "aoo-sink", &opts, &processWorklet, nullptr);
	// connectNode(node, ctx);
	if(!g_nodeReadyCb.isUndefined()) {
		g_nodeReadyCb(emscripten::val::take_ownership(js_getAudioObject(node)));
		g_nodeReadyCb = emscripten::val::undefined();
	}
}

/// Worklet-thread-ready callback: create the "aoo-sink" processor.
static void onThreadReady(EMSCRIPTEN_WEBAUDIO_T ctx, bool ok, void* user) {
	if(!ok) return;
	WebAudioWorkletProcessorCreateOptions popts = {};
	popts.name = "aoo-sink";
	emscripten_create_wasm_audio_worklet_processor_async(ctx, &popts, onProcessorCreated, nullptr);
}

/// Build an "aoo-sink" AudioWorkletNode on the caller's AudioContext, set the sink up
/// at that context's rate, and deliver the node to onNodeReady(node) once it exists.
void createOutputNode(AooSinkJS& sink, int channels, emscripten::val ctx, emscripten::val onNodeReady) {
	g_sink = &sink;
	g_channels = channels;
	g_nodeReadyCb = onNodeReady;
	g_ctx = js_registerAudioContext(ctx.as_handle());
	int rate = emscripten_audio_context_sample_rate(g_ctx);
	int quantum = emscripten_audio_context_quantum_size(g_ctx);
	sink.setup(channels, rate, quantum);
	emscripten_start_wasm_audio_worklet_thread_async(g_ctx, g_audioStack, sizeof(g_audioStack), onThreadReady, nullptr);
}

/// Bound to JS: debug counter of how many worklet render callbacks have run.
int getProcessCount() {
	return g_processCalls.load(std::memory_order_relaxed);
}

EMSCRIPTEN_BINDINGS(aoo_audioworklet) {
	emscripten::function("createOutputNode", &createOutputNode);
	emscripten::function("getProcessCount", &getProcessCount);
}
