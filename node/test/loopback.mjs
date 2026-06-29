// @ts-check
import { AooSource, AooSink } from "aoo-native"

export const SR = 48000
export const BLOCK = 256
export const CHANNELS = 1
export const SOURCE_ADDR = { ip: "127.0.0.1", port: 9000, id: 1 }
export const SINK_ADDR   = { ip: "127.0.0.1", port: 9001, id: 1 }

export function makeLoopback() {
	const source = new AooSource(SOURCE_ADDR.id)
	const sink = new AooSink(SINK_ADDR.id)

	source.setup(CHANNELS, SR, BLOCK)
	source.setFormat()
	sink.setup(CHANNELS, SR, BLOCK)
	sink.setLatency(0.05)

	const sourceEvents = []
	const sinkEvents = []
	const messages = []

	const toSink   = (bytes) => sink.handleMessage(bytes, SOURCE_ADDR.ip, SOURCE_ADDR.port)
	const toSource = (bytes) => source.handleMessage(bytes, SINK_ADDR.ip, SINK_ADDR.port)

	source.addSink(SINK_ADDR)
	source.startStream()

	const block = new Float32Array(CHANNELS * BLOCK)
	for (let i = 0; i < block.length; i++) block[i] = Math.sin(i * 0.1) * 0.25

	function tick() {
		source.process(block)
		source.send(toSink)
		sink.process()
		sink.send(toSource)
		sourceEvents.push(...source.pollEvents())
		sinkEvents.push(...sink.pollEvents())
		messages.push(...sink.pollStreamMessages())
	}

	async function run(ms = 500, intervalMs = 5, onTick) {
		const end = Date.now() + ms
		while (Date.now() < end) {
			tick()
			onTick?.()
			await new Promise((r) => setTimeout(r, intervalMs))
		}
	}

	const dispose = () => { source.delete(); sink.delete() }

	return { source, sink, sourceEvents, sinkEvents, messages, block, tick, run, dispose }
}