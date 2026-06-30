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

client.on("peerJoin", ev => src.addSink(ev.endpoint));
client.on("peerLeave", ev => src.removeSink(ev.endpoint));
client.on("disconnect", ev => console.log("disconnected:", ev))
client.on("peerMessage", ev => console.log("peer message:", Buffer.from(ev.data).toString()))

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

src.on("invite", ev => src.handleInvite(ev.endpoint, ev.token, true))
src.on("uninvite", ev => {
	src.handleUninvite(ev.endpoint, ev.token, true)
	src.removeSink(ev.endpoint)
})


setInterval(() => console.log("emitted", packets, "packets", bytes, "bytes"), 1000)