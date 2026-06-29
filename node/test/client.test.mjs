// @ts-check
import test from "node:test"
import assert from "node:assert/strict"
import { AooClient, AooSource, AooSink, aoo_version } from "aoo-native"

test("aoo_version returns a non-empty string", () => {
	assert.equal(typeof aoo_version(), "string")
	assert.ok(aoo_version().length > 0)
})

test("client lifecycle: start/add/remove/stop don't throw (no server needed)", () => {
	const client = new AooClient()
	const port = client.start(0)            // internal-socket mode
	assert.ok(port > 0, `start should return a bound port, got ${port}`)
	assert.equal(client.userId(), -1)       // -1 until a group is joined

	const src = new AooSource(1); src.setup(1, 48000, 256); src.setFormat()
	const sink = new AooSink(1);  sink.setup(1, 48000, 256)
	assert.doesNotThrow(() => client.addSource(src))
	assert.doesNotThrow(() => client.addSink(sink))
	assert.doesNotThrow(() => client.sendPacket(Buffer.from([1, 2, 3]), "127.0.0.1", 9999))
	assert.doesNotThrow(() => client.removeSource(src))
	assert.doesNotThrow(() => client.removeSink(sink))
	assert.doesNotThrow(() => client.stop())
	src.delete(); sink.delete()
})