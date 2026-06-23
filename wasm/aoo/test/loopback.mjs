// @ts-check
/** @import { AooSourceEvent, AooSinkEvent, AooStreamMessage } from 'aoo' */
import { aoo_initialize, AooSource, AooSink } from 'aoo'

export const SR = 48000
export const BLOCK = 256
export const CHANNELS = 1

export const SOURCE_ADDR = { ip: "127.0.0.1", port: 9000, id: 1}
export const SINK_ADDR = { ip: "127.0.0.1", port: 9001, id: 1}

export async function makeLoopback() {
	await aoo_initialize()

	const source = new AooSource(SOURCE_ADDR.id)
	const sink = new AooSink(SINK_ADDR.id)

	source.setup(CHANNELS, SR, BLOCK)
	source.setFormat()
	sink.setup(CHANNELS, SR, BLOCK)
	sink.setLatency(0.05)

	/** @type AooSourceEvent[]  */
	const sourceEvents = []
	/** @type AooSinkEvent[] */
	const sinkEvents = []
	/** @type AooStreamMessage[] */
	const messages = []
	source.setEventHandler((ev) => sourceEvents.push(ev))
	sink.setEventHandler((ev) => sinkEvents.push(ev))
	sink.setStreamMessageHandler((msg) => messages.push({...msg, data: new Uint8Array(msg.data)}))

	/**
	 * 
	 * @param {Uint8Array} bytes 
	 */
	function deliverSourcePacketToSink(bytes) {
		const copy = bytes.slice()
		sink.handleMessage(copy, SOURCE_ADDR.ip, SOURCE_ADDR.port)
	}
	/**
	 * 
	 * @param {Uint8Array} bytes 
	 */
	function deliverSinkPacketToSource(bytes) {
		const copy = bytes.slice()
		source.handleMessage(copy, SINK_ADDR.ip, SINK_ADDR.port)
	}

	source.addSink(SINK_ADDR.ip, SINK_ADDR.port, SINK_ADDR.id)
	source.startStream()

	const block = new Float32Array(CHANNELS * BLOCK)

	function tick() {
		source.process(block)
		source.send(deliverSourcePacketToSink)
		sink.process()
		sink.send(deliverSinkPacketToSource)

		source.pollEvents()
		sink.pollEvents()
		sink.pollStreamMessages()
	}

	async function run(ms = 500, intervalMs = 5) {
		const end = Date.now() + ms
		while(Date.now() < end) {
			tick()
			await new Promise(r => setTimeout(r, intervalMs))
		}
	}

	function dispose() {
		source.delete()
		sink.delete()
	}

	return {
		source, sink, sourceEvents, sinkEvents, messages, block, tick, run, dispose
	}
}