import { test } from 'node:test'
import assert from "node:assert/strict"
import { makeLoopback } from './loopback.mjs'

test("a source->sink session establishes over the in-memory loopback", async () => {
	const lb = await makeLoopback()
	await lb.run(500, 5)

	const sinkTypes = lb.sinkEvents.map(e => e.type)
	const states = lb.sinkEvents.filter(e => e.type === "streamState").map(e => e.state)

	if(!sinkTypes.includes("streamStart")) {
		console.log("Sink events:", sinkTypes)
		console.log("source events:", lb.sourceEvents.map(e => e.type))
	}

	assert.ok(sinkTypes.includes("sourceAdd"), `expected sourceAdd; got [${sinkTypes}]`)
	assert.ok(sinkTypes.includes("streamStart"), `expected streamStart; got [${sinkTypes}]`)
	assert.ok(states.length > 0, `expected at least one streamState; states [${states}]`)

	lb.dispose()
})