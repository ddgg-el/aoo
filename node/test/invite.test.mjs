// @ts-check
import test from "node:test"
import { ok, equal } from "node:assert/strict"
import { AooSource, AooSink } from "aoo-native"
import { findEvent } from "./utils.mjs"

const SR = 48000, BLOCK = 256, CH = 1
const SOURCE_ADDR = { ip: "127.0.0.1", port: 9000, id: 1 }
const SINK_ADDR   = { ip: "127.0.0.1", port: 9001, id: 1 }
const delay = (ms) => new Promise((r) => setTimeout(r, ms))

function makeUnconnectedPair() {
	const source = new AooSource(SOURCE_ADDR.id)
	const sink = new AooSink(SINK_ADDR.id)
	source.setup(CH, SR, BLOCK); source.setFormat()
	sink.setup(CH, SR, BLOCK); sink.setLatency(0.05)

	const sourceEvents = [], sinkEvents = []
	const block = new Float32Array(CH * BLOCK)
	for (let i = 0; i < block.length; i++) block[i] = Math.sin(i * 0.1) * 0.25

	function tick() {
		source.process(block)
		source.send((b) => sink.handleMessage(b, SOURCE_ADDR.ip, SOURCE_ADDR.port))
		sink.process()
		sink.send((b) => source.handleMessage(b, SINK_ADDR.ip, SINK_ADDR.port))
		sourceEvents.push(...source.pollEvents())
		sinkEvents.push(...sink.pollEvents())
	}
	async function run(ms, onTick) {
		const end = Date.now() + ms
		while (Date.now() < end) { tick(); onTick?.(); await delay(5) }
	}
	const dispose = () => { source.delete(); sink.delete() }
	return { source, sink, sourceEvents, sinkEvents, run, dispose }
}

test("sink invites source -> source accepts -> stream flows", async () => {
	const h = makeUnconnectedPair()
	try {
		h.source.startStream()
		h.sink.inviteSource(SOURCE_ADDR)
		let accepted = false
		await h.run(800, () => {
			const invite = findEvent(h.sourceEvents, "invite")
			if (invite && !accepted) { h.source.handleInvite(invite.endpoint, invite.token, true); accepted = true }
		})
		const invite = findEvent(h.sourceEvents, "invite")
		ok(invite, "source received an invite")
		equal(typeof invite.token, "number")
		ok(accepted, "source accepted")
		ok(h.sinkEvents.some((e) => e.type === "streamStart"), "sink saw streamStart")
	} finally { h.dispose() }
})

test("sink uninvites an active source -> source accepts -> streamStop", async () => {
	const h = makeUnconnectedPair()
	try {
		h.source.startStream()
		h.sink.inviteSource(SOURCE_ADDR)
		let accepted = false
		await h.run(800, () => {
			const inv = findEvent(h.sourceEvents, "invite")
			if (inv && !accepted) { h.source.handleInvite(inv.endpoint, inv.token, true); accepted = true }
		})
		ok(h.sinkEvents.some((e) => e.type === "sourceAdd"), "precondition: source active")

		h.sink.uninviteSource(SOURCE_ADDR)
		let released = false
		await h.run(800, () => {
			const un = findEvent(h.sourceEvents, "uninvite")
			if (un && !released) { h.source.handleUninvite(un.endpoint, un.token, true); released = true }
		})
		ok(released, "source accepted the uninvite")
		ok(h.sinkEvents.some((e) => e.type === "streamStop"), "sink stopped after uninvite")
	} finally { h.dispose() }
})