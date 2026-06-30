// @ts-check
import { AooClient, AooDataType, AooSink } from "aoo-native";

const SAMPLE_RATE= 48000
const BLOCK_SIZE = 256;
const SERVER_ADDR = {ip: "localhost", port: 7078}
const GROUP = "test-group"
const USER = "node-" + process.pid

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
	for (const m of sink.pollStreamMessages()) {
		console.log("stream msg:", m.type, Buffer.from(m.data).toString(), "from", m.source)
	}
}, 1000)

setInterval(() => { 
	for (const ev of client.pollEvents()) {
		switch (ev.type) {
			case "peerJoin":
				console.log(ev)
				sink.inviteSource({ip: ev.endpoint.ip, port: ev.endpoint.port, id:1})
				client.sendMessage(ev.user, { type: AooDataType.text, data: Buffer.from(`Hello ${ev.user} from ${USER}!`) }, true)
				break;
			case "notification":
				console.log("server says:", Buffer.from(ev.data).toString())
				break;
			default:
				break;
		}
		
		console.log("sink event:", ev.type) 
	}
}, 200)