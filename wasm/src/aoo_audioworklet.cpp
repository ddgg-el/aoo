// Browser-only glue (compiled into the threaded `aoo.web` build): drives an
// AooSinkJS from an emscripten Wasm AudioWorklet. AooSink::processWorklet() runs on
// the dedicated audio thread; send/handleMessage/poll* stay on the main thread.

#include "aoo_sink_js.hpp"
#include "aoo_source_js.hpp"
#include "aoo_types.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <emscripten/bind.h>
#include <emscripten/em_js.h>
#include <emscripten/em_macros.h>
#include <emscripten/val.h>
#include <emscripten/webaudio.h>
#include <vector>

static uint8_t g_audioStack[256 * 1024] __attribute__((aligned(16)));    ///< stack for the audio worklet thread
static AooSample g_silence[4096] = {0};
static std::atomic<int>g_processCalls{0};                             ///< debug: number of render callbacks run

static EMSCRIPTEN_WEBAUDIO_T g_ctx = 0;
static bool g_threadStarting = false;
static bool g_threadReady = false;

struct AooNodeSetup {
	void* object; // AooSinkJS or AooSourceJs
	int channels;
	bool isOutput;
	emscripten::val onNodeReady;
};

static std::vector<AooNodeSetup*> g_pending;

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

/// AudioWorklet render callback (runs on the audio thread). Builds planar channel
/// pointers into the output frame and renders one quantum through the sink.
static bool renderOutput(int numInputs, const AudioSampleFrame* inputs, int numOutputs, AudioSampleFrame* outputs, int numParams, const AudioParamFrame* params, void* user) {
	auto* sink = static_cast<AooSinkJS*>(user);
	if(!sink || numOutputs < 1) return true;
	AudioSampleFrame &out = outputs[0];
	AooSample* planar[32];
	int nChannels = out.numberOfChannels < 32 ? out.numberOfChannels : 32;
	for (int ch = 0; ch < nChannels; ch++) {
		planar[ch] = &out.data[ch * out.samplesPerChannel];
	}
	sink->processWorklet(planar, out.samplesPerChannel);
	g_processCalls.fetch_add(1, std::memory_order_relaxed);
	return true;
}


static bool renderInput(int numInputs, const AudioSampleFrame* inputs, int numOutputs, AudioSampleFrame* outputs, int numParams, const AudioParamFrame* params, void* user) {
	auto* source = static_cast<AooSourceJS*>(user);
	if(!source || numInputs < 1) return true;
	
	const AudioSampleFrame& in = inputs[0];
	int nframes = in.samplesPerChannel;

	AooSample* planar[32];
	int eCh = source->channels() < 32 ? source->channels() : 32;
	// int n = in.numberOfChannels * in.samplesPerChannel;
	for (int ch = 0; ch < eCh; ch++) {
		planar[ch] = ch < in.numberOfChannels ? const_cast<AooSample*>(&in.data[ch * nframes]): g_silence;
	}
	
	source->processWorklet(planar, nframes);
	g_processCalls.fetch_add(1, std::memory_order_relaxed);

	return true;
}



/// Worklet-processor-created callback: create the worklet node and connect it.
static void onProcessorCreated(EMSCRIPTEN_WEBAUDIO_T ctx, bool ok, void* user) {
	auto* setup = static_cast<AooNodeSetup*>(user);
	if(!ok) { printf("%s: processor FAILED\n", setup->isOutput ? "AooSink" : "AooSource"); return; };
	
	int counts[1] = { setup->channels };
	EmscriptenAudioWorkletNodeCreateOptions opts = {};
	const char* name;
	EmscriptenWorkletNodeProcessCallback render;
	if(setup->isOutput) {
		opts.numberOfInputs = 0;
		// TODO: vhy numberOfOutput = 1 and outputChannelCounts = counts?
		opts.numberOfOutputs = 1;
		opts.outputChannelCounts = counts;
		name = "aoo-sink";
		render = &renderOutput;
	} else {
		opts.numberOfInputs = 1;
		opts.numberOfOutputs = 0;
		name = "aoo-source";
		render = &renderInput;
	}
	EMSCRIPTEN_WEBAUDIO_T node = emscripten_create_wasm_audio_worklet_node(ctx, name, &opts, render, setup->object);

	// connectNode(node, ctx);
	if(!setup->onNodeReady.isUndefined()) {
		setup->onNodeReady(emscripten::val::take_ownership(js_getAudioObject(node)));
		delete setup;
	}
}

static void createProcessor(EMSCRIPTEN_WEBAUDIO_T ctx, AooNodeSetup* setup) {
	WebAudioWorkletProcessorCreateOptions popts = {};
	popts.name = setup->isOutput ? "aoo-sink": "aoo-source";
	emscripten_create_wasm_audio_worklet_processor_async(ctx, &popts, onProcessorCreated, setup);
}

/// Worklet-thread-ready callback: create the "aoo-sink" processor.
static void onThreadReady(EMSCRIPTEN_WEBAUDIO_T ctx, bool ok, void* user) {
	if(!ok) {
		for (auto* s : g_pending) {
			delete s;
		}
		g_pending.clear();
		g_threadStarting = false;
		return;
	} 
	g_threadReady = true;
	for (auto* s : g_pending) {
		createProcessor(ctx, s);
	}
	g_pending.clear();
}

static void startNode(EMSCRIPTEN_WEBAUDIO_T ctx, AooNodeSetup* setup) {
	if(g_threadReady) {
		createProcessor(ctx, setup);
	} else {
		g_pending.push_back(setup);
		if(!g_threadStarting) {
			g_threadStarting = true;
			emscripten_start_wasm_audio_worklet_thread_async(ctx, g_audioStack, sizeof(g_audioStack), onThreadReady, nullptr);
		}
	}
}

static EMSCRIPTEN_WEBAUDIO_T registerCtxOnce(emscripten::val ctx) {
	if(g_ctx == 0) {
		g_ctx = js_registerAudioContext(ctx.as_handle());
	}
	return g_ctx;
}

/// Build an "aoo-sink" AudioWorkletNode on the caller's AudioContext, set the sink up
/// at that context's rate, and deliver the node to onNodeReady(node) once it exists.
void createOutputNode(AooSinkJS& sink, int channels, emscripten::val ctx, emscripten::val onNodeReady) {
	EMSCRIPTEN_WEBAUDIO_T handle = registerCtxOnce(ctx);
	sink.setup(channels, emscripten_audio_context_sample_rate(handle), emscripten_audio_context_quantum_size(handle));
	startNode(handle, new AooNodeSetup{ &sink, channels, true, onNodeReady});
}

void createInputNode(AooSourceJS& source, int channels, emscripten::val ctx, emscripten::val onNodeReady) {
	EMSCRIPTEN_WEBAUDIO_T handle = registerCtxOnce(ctx);
	source.setup(channels, emscripten_audio_context_sample_rate(handle), emscripten_audio_context_quantum_size(handle));
	startNode(handle, new AooNodeSetup{ &source, channels, false, onNodeReady});
}

// /// Bound to JS: debug counter of how many worklet render callbacks have run.
int getProcessCount() {
	return g_processCalls.load(std::memory_order_relaxed);
}

EMSCRIPTEN_BINDINGS(aoo_audioworklet) {
	emscripten::function("createOutputNode", &createOutputNode);
	emscripten::function("createInputNode",&createInputNode);
	emscripten::function("getProcessCount", &getProcessCount);
}
