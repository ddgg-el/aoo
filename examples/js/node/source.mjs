// @ts-check
/**
 * @typedef {(bytes: Uint8Array, ip: string, port: number) => void} AooSendCallback
 * @typedef {{ ip: string, port: number, id: number }} AooEndpoint
 * @typedef {(
 *   | { type: "sinkAdd" | "sinkRemove", endpoint: AooEndpoint }
 *   | { type: "sinkPing", endpoint: AooEndpoint, rtt: number, packetLoss: number }
 *   | { type: "invite" | "uninvite", endpoint: AooEndpoint, token: number }
 *   | { type: "frameResend", endpoint: AooEndpoint, count: number }
 * )} AooSourceEvent
 * @typedef {(ev: AooSourceEvent) => void} AooEventHandler
 */

import createModule from "aoo"
import dgram from "node:dgram"
import portaudio from "naudiodon2"
import { chooseAudioDevice } from "./utils.mjs"

const SOURCE_ID = 1
const SINK_HOST = "127.0.0.1"
const SINK_PORT = 9001
const SINK_ID = 1

const CHANNELS = 1
const SR = 48000
const BLOCK = 256
const DEVICE = chooseAudioDevice("MacBook Pro Microphone")

const aoo = await createModule()
aoo.initialize()

const source = new aoo.AooSource(SOURCE_ID)
source.setup(CHANNELS, SR, BLOCK)
source.setFormat()


source.setEventHandler( /** @type {AooEventHandler} */ (ev) => {
	switch (ev.type) {
		case "sinkAdd":
			console.log(`sinkAdd -> ${ev.endpoint.ip}:${ev.endpoint.port} id=${ev.endpoint.id}`)
			break;
		case "sinkRemove":
			console.log(`sinkRemove -> ${ev.endpoint.ip}:${ev.endpoint.port} id=${ev.endpoint.id}`)
			break;
		case "sinkPing":
			console.log(`sinkPing -> rtt=${(ev.rtt * 1000).toFixed(2)} ms loss=${(ev.packetLoss * 100).toFixed(1)}%`)
			break;
		default:
			console.log("event:", ev.type)
			break;
	}
})

const sock = dgram.createSocket("udp4")

sock.on("message", (msg, rinfo) => {
	source.handleMessage(new Uint8Array(msg), rinfo.address, rinfo.port)
})

const pa = portaudio.AudioIO({
	inOptions: {
		channelCount: CHANNELS,
		sampleRate: SR,
		sampleFormat: portaudio.SampleFormatFloat32,
		deviceId: DEVICE,
		closeOnError: false,
		framesPerBuffer: BLOCK,
		highwaterMark: BLOCK * CHANNELS * 4
	}
})

const block = new Float32Array(CHANNELS*BLOCK)
let phase = 0

const fillTone = () => {
	for (let i = 0; i < BLOCK; i++) {
		const sine = Math.sin(phase) * 0.2
		phase += 2 * Math.PI * 300 / SR
		for (let c = 0; c < CHANNELS; c++) {
			block[i * CHANNELS + c] = sine
		}
	}
}

/**
 * @type {AooSendCallback}
 */
const forward = (bytes, ip, port) => {
	sock.send(Buffer.from(bytes), port, ip)
}

pa.on('data', () => {
	fillTone()
	source.process(block)
	source.send(forward)
}) 

process.on("SIGINT", () => {
	pa.quit()
	sock.close()
	source.delete()
	aoo.terminate()
	process.exit(0)
})

sock.bind(()=> {
	console.log("addSink",    source.addSink(SINK_HOST, SINK_PORT, SINK_ID));   // pretend a sink id 2
	console.log("startStream",source.startStream());
	pa.start()
})