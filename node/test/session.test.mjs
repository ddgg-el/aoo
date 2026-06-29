// @ts-check
import test from "node:test"
import assert from "node:assert/strict"
import { makeLoopback } from "./loopback.mjs"

test("a source->sink session establishes over the loopback", async () => {
	const lb = makeLoopback()
	await lb.run(500, 5)
	const types = lb.sinkEvents.map((e) => e.type)
	assert.ok(types.includes("sourceAdd"), `expected sourceAdd; got [${types}]`)
	assert.ok(types.includes("streamStart"), `expected streamStart; got [${types}]`)
	assert.ok(types.includes("formatChange"), `expected formatChange; got [${types}]`)
	assert.ok(lb.sinkEvents.some((e) => e.type === "streamState"), "expected a streamState")
	lb.dispose()
})