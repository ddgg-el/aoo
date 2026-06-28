// @ts-check
import { AooClient, AooSource } from "aoo-native"

const BLOCK_SIZE = 256
const SAMPLE_RATE = 48000
const SINK_ADDRESS = {ip: "127.0.0.1", port: 10000, id: 1}

const client = new AooClient()
console.log("source client on", client.start(0))

const src = new AooSource(1)
src.setup(1, SAMPLE_RATE, BLOCK_SIZE)
src.setFormat()
client.addSource(src)

src.addSink(SINK_ADDRESS)
src.startStream()

let bytes = 0
let packets = 0
let phase = 0
const buf = new Float32Array(BLOCK_SIZE)

setInterval(() => {
	for (let i = 0; i < BLOCK_SIZE; i++) {
		buf[i] = Math.sin(phase) * 0.25;
		phase += 2 * Math.PI* 300/ SAMPLE_RATE
	}
	src.process(buf)
	client.notify()
	bytes += buf.length
	packets++
},5)

setInterval(() => { for (const ev of src.pollEvents()) console.log("source event:", ev) }, 200)

setInterval(() => console.log("emitted", packets, "packets", bytes, "bytes"), 1000)