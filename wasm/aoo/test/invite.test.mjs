// @ts-check
/** @import { AooSourceEvent, AooSinkEvent } from 'aoo'  */
import { aoo_initialize, AooSource, AooSink } from 'aoo'
import { equal, ok } from "node:assert/strict"
import { findEvent } from "./utils.mjs"
import test from 'node:test'

const SR = 48000
const BLOCK = 256
const CHANNELS = 1
const SOURCE_ADDR = { ip: "127.0.0.1", port: 9000, id: 1}
const SINK_ADDR = { ip: "127.0.0.1", port: 9001, id: 1}

/** @type (ms:number) => Promise<ReturnType<setTimeout>> */
const delay = (ms) => new Promise(r => setTimeout(r, ms))

async function makeUnconnectedPair() {
	await aoo_initialize()

	const source = new AooSource(SOURCE_ADDR.id)
	const sink = new AooSink(SINK_ADDR.id)

	source.setup(CHANNELS, SR, BLOCK)
	source.setFormat()
	sink.setup(CHANNELS, SR, BLOCK)
	sink.setLatency(0.05)

	/** @type {AooSourceEvent[]} */ const sourceEvents = []
	/** @type {AooSinkEvent[]}   */ const sinkEvents = []
	source.setEventHandler(ev => sourceEvents.push(ev))
	sink.setEventHandler(ev => sinkEvents.push(ev))

	const block = new Float32Array(CHANNELS * BLOCK)
	for (let i = 0; i < block.length; i++) {
		block[i] = Math.sin(i * 0.1)
	}

	
	/** @param {Uint8Array} bytes */
	function deliverSourcePacketToSink(bytes) {
		sink.handleMessage(bytes.slice(), SOURCE_ADDR.ip, SOURCE_ADDR.port)
	}
	/** @param {Uint8Array} bytes */
	function deliverSinkPacketToSource(bytes) {
		source.handleMessage(bytes.slice(), SINK_ADDR.ip, SINK_ADDR.port)
	}

	function tick() {
		source.process(block)
		source.send(deliverSourcePacketToSink)
		sink.process()
		sink.send(deliverSinkPacketToSource)
		source.pollEvents()
		sink.pollEvents()
	}
	
	/**
	 * @param {number} ms 
	 * @param {() => void} onTick 
	 */
	async function runTick (ms, onTick) {
		const end = Date.now() + ms
		while(Date.now() < end) {
			tick()
			onTick?.()
			await delay(5)
		}
	}

	function dispose() {
		source.delete()
		sink.delete()
	}

	return { source, sink, sourceEvents, sinkEvents, runTick, dispose }
}

test('sink invites source -> source accepts -> stream flows', async () => {
	const h = await makeUnconnectedPair()	
	try {
		h.source.startStream()

		h.sink.inviteSource(SOURCE_ADDR)

		let accepted = false 
		
		await h.runTick(800, () => {
			const invite = findEvent(h.sourceEvents, 'invite')
			if(invite && !accepted) {
				h.source.handleInvite(invite.endpoint, invite.token, true)
				accepted = true
			}
		})
		const invite = findEvent(h.sourceEvents, 'invite')
		ok(invite, 'source received an invite event')
		equal(typeof invite.token, 'number')
		ok(accepted, 'source accepted the invitation')
		ok(h.sinkEvents.some(e => e.type === 'streamStart'), 'sink saw the stream start')
	} finally {
		h.dispose()
		
	}
});

test('sink uninvites an active source → source accepts', async () => {
	const h = await makeUnconnectedPair()
	try {
		h.source.startStream()
		h.sink.inviteSource(SOURCE_ADDR)

		let accepted = false
		
		await h.runTick(800, () => {
			const invite = findEvent(h.sourceEvents, 'invite')
			if (invite && !accepted) {
				h.source.handleInvite(invite.endpoint, invite.token, true)
				accepted = true
			}
		})
		
		ok(h.sinkEvents.some((e) => e.type === 'sourceAdd'), 'precondition: source is active')

		h.sink.uninviteSource(SOURCE_ADDR)
		let released = false
		
		await h.runTick(800, () => {
			const uninvite = findEvent(h.sourceEvents, 'uninvite')
			if (uninvite && !released) {
				h.source.handleUninvite(uninvite.endpoint, uninvite.token, true)
				released = true
			}
		})
		

		const uninvite = h.sourceEvents.find((e) => e.type === 'uninvite')
		ok(uninvite, 'source received an uninvite event')
		ok(released, 'source accepted the uninvitation')
		ok(h.sinkEvents.some((e) => e.type === 'streamStop'), 'sink stopped the stream after uninvite')
	} finally {
		h.dispose()
	}
})
