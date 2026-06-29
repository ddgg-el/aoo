// @ts-check
import test from "node:test"
import assert from "node:assert/strict"
import { makeLoopback, SOURCE_ADDR, SINK_ADDR } from "./loopback.mjs"
import { AooResampleMethod } from "aoo-native"

test("getRealSampleRate / eventsAvailable", async () => {
	const lb = makeLoopback()
	await lb.run(400, 5)
	assert.ok(lb.source.getRealSampleRate() > 0)
	assert.ok(lb.sink.getRealSampleRate() > 0)
	assert.equal(typeof lb.source.eventsAvailable(), "boolean")
	lb.dispose()
})

test("scalar setters are callable and don't break the session", async () => {
	const lb = makeLoopback()
	lb.source.setPacketSize(512)
	lb.source.setPingInterval(1.0)
	lb.source.setDllBandwidth(0.1)
	lb.source.setRedundancy(1)
	lb.source.setResendBufferSize(1.0)
	lb.source.setBinaryFormat(false)
	lb.source.setStreamTimeSendInterval(1.0)
	lb.source.setDynamicResampling(true)
	lb.source.setBufferSize(0.02)
	lb.source.setResampleMethod(AooResampleMethod.linear)

	lb.sink.setPacketSize(512)
	lb.sink.setResendData(true)
	lb.sink.setResendInterval(0.05)
	lb.sink.setResendLimit(16)
	lb.sink.setResampleMethod(AooResampleMethod.cubic)

	await lb.run(400, 5)
	assert.ok(lb.sinkEvents.some((e) => e.type === "streamStart"), "session should still establish")
	lb.dispose()
})

test("reset / setId / removeAllSinks are callable", async () => {
	const lb = makeLoopback()
	assert.doesNotThrow(() => lb.source.setId(42))
	assert.doesNotThrow(() => lb.sink.setId(42))
	assert.doesNotThrow(() => lb.source.removeAllSinks())
	assert.doesNotThrow(() => lb.source.reset())
	assert.doesNotThrow(() => lb.sink.reset())
	lb.dispose()
})

test("opus format establishes a session", async () => {
	const lb = makeLoopback()
	lb.source.setFormat({ codec: "opus", application: "lowdelay", bitrate: 32000, complexity: 5 })
	lb.source.startStream()
	await lb.run(500, 5)
	const fmt = lb.sinkEvents.find((e) => e.type === "formatChange")
	assert.ok(fmt, "expected formatChange")
	assert.equal(fmt.codec, "opus", `expected opus; got ${fmt.codec}`)
	lb.source.setOpusBitrate(16000)   // live change, no throw
	lb.dispose()
})

test("stopStream makes the sink see streamStop", async () => {
	const lb = makeLoopback()
	await lb.run(300, 5)
	assert.ok(lb.sinkEvents.some((e) => e.type === "streamStart"))
	lb.source.stopStream()
	await lb.run(250, 5)
	assert.ok(lb.sinkEvents.some((e) => e.type === "streamStop"), "expected streamStop")
	lb.dispose()
})

test("endpoint-taking methods are callable; getBufferFillRatio in [0,1]", async () => {
	const lb = makeLoopback()
	await lb.run(400, 5)
	const add = lb.sinkEvents.find((e) => e.type === "sourceAdd")
	assert.ok(add, "need a sourceAdd")
	const ratio = lb.sink.getBufferFillRatio(add.endpoint)
	assert.ok(ratio >= 0 && ratio <= 1, `fill ratio in [0,1], got ${ratio}`)
	assert.doesNotThrow(() => lb.source.activate(SINK_ADDR, true))
	assert.doesNotThrow(() => lb.source.setSinkChannelOffset(SINK_ADDR, 0))
	assert.doesNotThrow(() => lb.sink.resetSource(SOURCE_ADDR))
	assert.doesNotThrow(() => lb.source.removeSink(SINK_ADDR))
	lb.dispose()
})