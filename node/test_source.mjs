// @ts-check
import { AooClient, AooDataType, AooResampleMethod, AooSource, messageType } from "aoo-native"

const BLOCK_SIZE = 256
const SAMPLE_RATE = 48000
const SERVER_ADDR = {ip: "localhost", port: 7078}
const GROUP = "test-group"
const USER = "node-" + process.pid

const client = new AooClient()
console.log("source client on", client.start(0))

const src = new AooSource(1)
src.setup(1, SAMPLE_RATE, BLOCK_SIZE)
// src.setFormat()
src.setFormat({ codec: "opus", application: "lowdelay", bitrate: 64000, complexity: 5 })
src.setResampleMethod(AooResampleMethod.cubic)
src.setRedundancy(2)
client.addSource(src)

src.setBufferSize(0.03)
console.log("real sampleRate", src.getRealSampleRate())

src.startStream()

client.join(SERVER_ADDR.ip, SERVER_ADDR.port, GROUP, USER)
console.log(`joined "${GROUP}" as ${USER} with id ${client.userId()}`)

let bytes = 0
let packets = 0
let phase = 0
let tick = 0
const buf = new Float32Array(BLOCK_SIZE)

setInterval(() => {
	for (let i = 0; i < BLOCK_SIZE; i++) {
		buf[i] = Math.sin(phase) * 0.25;
		phase += 2 * Math.PI* 300/ SAMPLE_RATE
	}
	if(++tick % 200 === 0) {
		src.addStreamMessage({type:AooDataType.text, data: Buffer.from("hello!!")})
	}
	src.process(buf)
	client.notify()
	bytes += buf.length
	packets++
},5)

setInterval(() => { 
	for (const ev of client.pollEvents()) {
		switch (ev.type) {
			case "peerJoin": 
				console.log("User ", ev.user,  "joined")
				break
			case "peerLeave":
				console.log("User ", ev.user,  "leaved")
				break
			case "disconnect":
				console.log("disconnected:", ev) 
				break
			case "peerMessage":
				console.log("peer message from user", ev.user, ":", Buffer.from(ev.data).toString())
			default:
				break
		}
	} 
}, 200)

setInterval(() => { 
	for (const ev of src.pollEvents()) {
		switch (ev.type) {
			case "invite":
				src.handleInvite(ev.endpoint, ev.token, true)
				// src.addSink(ev.endpoint)
				break
			case "uninvite":
				src.handleUninvite(ev.endpoint, ev.token, true)
				src.removeSink(ev.endpoint)
				break
			default:
				break
		}
	} 
}, 200)

setInterval(() => console.log("emitted", packets, "packets", bytes, "bytes"), 1000)