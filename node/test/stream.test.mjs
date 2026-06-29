// @ts-check
import test from "node:test"
import assert from "node:assert/strict"
import { makeLoopback } from "./loopback.mjs"
import { AooDataType, messageType, AooMsgType } from "aoo-native"

test("stream messages travel source -> sink", async () => {
	const lb = makeLoopback()
	await lb.run(600, 5, () =>
		lb.source.addStreamMessage({ type: AooDataType.text, data: Buffer.from("ping") }))
	assert.ok(lb.messages.length > 0, "sink should receive stream messages")
	const m = lb.messages.find((x) => Buffer.from(x.data).toString() === "ping")
	assert.ok(m, `expected a 'ping' message; got [${lb.messages.map((x) => Buffer.from(x.data))}]`)
	assert.equal(m.type, AooDataType.text)
	assert.ok(m.source && typeof m.source.id === "number", "message carries a source endpoint")
	lb.dispose()
})

test("messageType() identifies a source's outgoing packet as sink-typed", () => {
	const lb = makeLoopback()
	let seen = -1
	lb.source.process(lb.block)
	lb.source.send((bytes) => { if (seen < 0) seen = messageType(bytes) })
	assert.equal(seen, AooMsgType.sink, `audio packets are addressed to the sink; got ${seen}`)
	lb.dispose()
})