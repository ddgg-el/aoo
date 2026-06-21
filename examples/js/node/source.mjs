// @ts-check
/**
 * @import { AooSendCallback } from "aoo"
 */

import * as aoo from "aoo"
import dgram from "node:dgram"
import portaudio from "naudiodon2"
import { chooseAudioDevice } from "./utils.mjs"

const SOURCE_ID = 1
const SINK_HOST = "localhost"
const SINK_PORT = 10001
const SINK_ID = 1

const CHANNELS = 1
const SR = 48000
const BLOCK = 256
const DEVICE = chooseAudioDevice("MacBook Pro Microphone")

await aoo.initialize()

const source = new aoo.AooSource(SOURCE_ID)
source.setup(CHANNELS, SR, BLOCK)
source.setFormat()


source.setEventHandler( (ev) => {
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

// const sock = dgram.createSocket({ type: "udp6", ipv6Only: false });
const sock = dgram.createSocket("udp4");

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

/** @type {AooSendCallback} */
const forward = (bytes, ip, port) => {
	sock.send(Buffer.from(bytes), port, ip, (err) => {
		if(err) console.error("send failed:", ip, err.message)
	})
}

const enc = new TextEncoder()
const BLOCKS_PER_SEC = Math.round(SR/BLOCK)
let blockCount = 0
let msgCount = 0

pa.on('data', () => {
	fillTone()

	if(blockCount % BLOCKS_PER_SEC === 0) {
		const payload = enc.encode(`CIAO #${msgCount++}!`)
		source.addStreamMessage(aoo.DataType.text, payload,0,0)
	}
	blockCount++

	source.process(block)
	source.send(forward)
}) 

let exiting = false
function shutdown(code = 0) {
	if(exiting) process.exit(1)

	exiting = true
	try {pa.quit()} catch (e) {console.error("pa.quit error: ", e)}
	try {sock.close()} catch (e) {console.error("sock close error: ", e)}
	try {source.delete()} catch (e) {console.error("delete source error: ", e)}
	try {aoo.terminate()} catch (e) {console.error("error terminating aoo: ", e)}
	process.exit(code)
}

process.on("SIGINT", () => shutdown(0) )

sock.bind(()=> {
	try{
		source.addSink(SINK_HOST, SINK_PORT, SINK_ID)
		source.startStream()
		pa.start()
	} catch (err) {
		console.error("setup failed:", err instanceof Error ? err.message : err)
		shutdown(1)
	}
})