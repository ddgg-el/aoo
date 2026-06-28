// @ts-check
import dgram from "node:dgram"
import { AooClient, AooSink } from "aoo-native";

const SAMPLE_RATE= 48000
const BLOCK_SIZE = 256;
const SERVER_ADDR = {ip: "localhost", port: 7078}
const GROUP = "test-group"
const USER = "node-" + process.pid

// const SOURCE_ADDR = { ip: "127.0.0.1", port: 9999 }

const client = new AooClient()
client.start(0)
client.join(SERVER_ADDR.ip, SERVER_ADDR.port, GROUP, USER)
console.log(`joined "${GROUP}" as ${USER}`)

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

setInterval(() => { 
	for (const ev of client.pollEvents()) {
		if(ev.type === "peerJoin") {
			sink.inviteSource({ip: ev.endpoint.ip, port: ev.endpoint.port, id:1})
		}
		console.log("sink event:", ev.type) 
	}
}, 200)