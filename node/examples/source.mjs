// @ts-check
import portAudio from "naudiodon2"
import { AooClient, AooDataType, AooSource } from "aoo-native"
import { chooseAudioDevice } from "./devices.mjs"

const CHANNELS = 1
const BLOCK_SIZE = 256
const BLOCK_BYTES = BLOCK_SIZE * CHANNELS * 4 // Float32
const SAMPLE_RATE = 48000
const DEVICE = chooseAudioDevice("MacBook Pro Microphone")


const SERVER_ADDR = {ip: "localhost", port: 7078}
const GROUP = "test-group"
const USER = "node-" + process.pid

const pa = portAudio.AudioIO({
	inOptions: {
		channelCount: CHANNELS,
		sampleRate: SAMPLE_RATE,
		sampleFormat: portAudio.SampleFormatFloat32,
		deviceId: DEVICE,
		closeOnError: true,
		framesPerBuffer: BLOCK_SIZE,
		highwaterMark: BLOCK_BYTES
	}
})

const client = new AooClient()
console.log("source client on", client.start(0))

const src = new AooSource(1)
src.setup(1, SAMPLE_RATE, BLOCK_SIZE)
src.setFormat({ codec: "opus", application: "lowdelay", bitrate: 64000, complexity: 5 })
client.addSource(src)
src.startStream()

await client.join(SERVER_ADDR.ip, SERVER_ADDR.port, GROUP, USER)
console.log(`joined "${GROUP}" as ${USER} with id ${client.userId()}`)

client.on("peerJoin", ev => src.addSink(ev.endpoint));
client.on("peerLeave", ev => src.removeSink(ev.endpoint));
client.on("disconnect", ev => console.log("disconnected:", ev))
client.on("peerMessage", ev => console.log("peer message:", Buffer.from(ev.data).toString()))
client.on("peerPing", ev => console.log("peer ping:", ev.user))

setInterval(()=>{
	for(const ev of client.pollEvents()) {
		console.log(ev)
	}
}, 5)

let bytes = 0
let packets = 0
let phase = 0
let tick = 0
const chunk = new Float32Array(BLOCK_SIZE)

function fillBlock(){
	for (let i = 0; i < BLOCK_SIZE; i++) {
		chunk[i] = Math.sin(phase) * 0.25;
		phase += 2 * Math.PI* 300/ SAMPLE_RATE
	}
	src.process(chunk)
	client.notify()
	bytes += chunk.length
	packets++
}

pa.on("data", () => {
	if(++tick % 200 === 0) {
		src.addStreamMessage({ type: AooDataType.text, data: Buffer.from("hello!")})
	}
	fillBlock()
})

pa.start()

src.on("invite", ev => src.handleInvite(ev.endpoint, ev.token, true))
src.on("uninvite", ev => {
	src.handleUninvite(ev.endpoint, ev.token, true)
	src.removeSink(ev.endpoint)
})