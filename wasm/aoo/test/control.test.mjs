// @ts-check
import test from "node:test";
import { makeLoopback, SOURCE_ADDR, SINK_ADDR } from "./loopback.mjs";
import assert from "node:assert/strict";
import { AooResampleMethod } from "aoo";


test("getRealSampleRate / eventsAvailable return same values", async () => {
	const lb = await makeLoopback()
	await lb.run(400, 5)

	const srcRate = lb.source.getRealSampleRate()
	const sinkRate = lb.sink.getRealSampleRate()
	assert.ok(Number.isFinite(srcRate) && srcRate > 0, `source real SR finite>0, got ${srcRate}`)
	assert.ok(Number.isFinite(sinkRate) && sinkRate > 0, `sink real SR finite>0, got ${sinkRate}`)

	assert.equal(typeof lb.source.eventsAvailable(), "boolean")
	assert.equal(typeof lb.sink.eventsAvailable(), "boolean")

	lb.dispose()
})

test("tuning setters return ok and don't break the session", async () => {
	const lb = await makeLoopback()
	lb.source.setDynamicResampling(true)
	lb.source.setBufferSize(0.02)
	lb.sink.setDynamicResampling(true)
	lb.sink.setBufferSize(0.05)
	await lb.run(400, 5)
	assert.ok(lb.sinkEvents.some(e => e.type === "streamStart"), "session should still establish")

	lb.dispose()
})

test("stopStream makes the sink see streamStop", async() => {
	const lb = await makeLoopback()
	await lb.run(300, 5)
	assert.ok(lb.sinkEvents.some(e => e.type === "streamStart"), "stream should start first")

	lb.source.stopStream(0)
	await lb.run(250,5)

	const types = lb.sinkEvents.map(e => e.type)
	assert.ok(types.includes("streamStop"), `expected streamStop; got[${types}]`)

	lb.dispose()
})

test("scalar setters are callable and don't break the session", async () => {
	const lb = await makeLoopback()

	lb.source.setPacketSize(512)
	lb.source.setPingInterval(1.0)
	lb.source.setDllBandwidth(0.1)
	lb.source.setRedundancy(1)
	lb.source.setResendBufferSize(1.0)
	lb.source.setBinaryFormat(false)
	lb.source.setStreamTimeSendInterval(1.0)
	lb.source.setResampleMethod(AooResampleMethod.linear)

	lb.sink.setPacketSize(512)
	lb.sink.setPingInterval(1.0)
	lb.sink.setDllBandwidth(0.1)
	lb.sink.setResendData(true)
	lb.sink.setResendInterval(0.05)
	lb.sink.setResendLimit(16)
	lb.sink.setBinaryFormat(false)
	lb.sink.setResampleMethod(AooResampleMethod.cubic)

	await lb.run(400, 5)
	assert.ok(lb.sinkEvents.some((e) => e.type === "streamStart"), "session should still establish")
	lb.dispose()
})

test("reset / setId / removeAllSinks are callable", async () => {
	const lb = await makeLoopback()
	assert.doesNotThrow(() => lb.source.setId(42))
	assert.doesNotThrow(() => lb.sink.setId(42))
	assert.doesNotThrow(() => lb.source.removeAllSinks())
	assert.doesNotThrow(() => lb.source.reset())
	assert.doesNotThrow(() => lb.sink.reset())
	lb.dispose()
})

test("endpoint-taking methods are callable", async () => {
	const lb = await makeLoopback()
	await lb.run(300, 5)
	assert.doesNotThrow(() => lb.source.activate(SINK_ADDR, true))
	assert.doesNotThrow(() => lb.source.setSinkChannelOffset(SINK_ADDR, 1))
	assert.doesNotThrow(() => lb.sink.resetSource(SOURCE_ADDR))
	assert.doesNotThrow(() => lb.source.removeSink(SINK_ADDR)) // last — removes the sink
	lb.dispose()
})

test("getBufferFillRatio returns a 0..1 ratio for a connected source", async () => {
	const lb = await makeLoopback()
	await lb.run(400, 5)
	const add = lb.sinkEvents.find((e) => e.type === "sourceAdd")
	assert.ok(add, "need a sourceAdd to know the source endpoint")
	const ratio = lb.sink.getBufferFillRatio(add.endpoint)
	assert.ok(ratio >= 0 && ratio <= 1, `fill ratio in [0,1], got ${ratio}`)
	lb.dispose()
})