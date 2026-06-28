// @ts-check
import dgram from "node:dgram"
import { AooClient, AooSink } from "aoo-native";

const SAMPLE_RATE= 48000
const BLOCK_SIZE = 256;
const PORT = 10000
// const SOURCE_ADDR = { ip: "127.0.0.1", port: 9999 }

const client = new AooClient()
client.start(PORT)

const sink = new AooSink(1)
sink.setup(1, SAMPLE_RATE, BLOCK_SIZE)
sink.setLatency(0.05)
client.addSink(sink)

let rxPeak = 0

setInterval(() => {
	const out = sink.process()
	for (let i = 0; i < out.length; i++) {
		rxPeak = Math.max(rxPeak, Math.abs(out[i]));	
	}
	client.notify()
}, 5)

setInterval(() => { 
	console.log("rxPeak", rxPeak.toFixed(4))
	rxPeak = 0
}, 1000)