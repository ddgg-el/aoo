// @ts-check
import { AooClient, AooDataType, AooSink } from "aoo-native";
import portAudio from "naudiodon2"
import { chooseAudioDevice } from "./devices.mjs";

const CHANNELS = 1
const SAMPLE_RATE = 48000
const BLOCK_SIZE = 256;
const DEVICE = chooseAudioDevice("MacBook Pro Speakers")

const SERVER_ADDR = {ip: "localhost", port: 7078}
const GROUP = "test-group"
const USER = "node-" + process.pid

const client = new AooClient()
client.start(0)
try {
  await client.join(SERVER_ADDR.ip, SERVER_ADDR.port, GROUP, USER, "_")
  console.log(`joined "${GROUP}" as ${USER}`)
} catch (error) {
  // @ts-ignore
  console.error("join failed:", error.message)
  client.stop()
  process.exit(1)
}

const sink = new AooSink(1)
sink.setup(1, SAMPLE_RATE, BLOCK_SIZE)
sink.setLatency(0.05)
client.addSink(sink)

const pa = portAudio.AudioIO({
  outOptions: {
	channelCount: CHANNELS,
	sampleFormat: portAudio.SampleFormatFloat32,
	sampleRate: SAMPLE_RATE,
	deviceId: DEVICE,
	closeOnError: false
  }
})

function renderAudioBlock() {
  const audio = sink.process()
  client.notify()
  const buf = Buffer.from(audio.buffer.slice(audio.byteOffset, audio.byteOffset + audio.byteLength))
  if(pa.write(buf)) {
	setImmediate(renderAudioBlock)
  } else {
	pa.once("drain", renderAudioBlock)
  }
}

pa.start()
renderAudioBlock()

sink.on("streamMessage", m => {
	console.log("stream msg:", m.type, Buffer.from(m.data).toString(), "from", m.source)
})

sink.on("sourcePing", (ev) => console.log("ping"))

// client events
client.on("peerJoin", ev => {
  console.log(ev)
  sink.inviteSource({ ip: ev.endpoint.ip, port: ev.endpoint.port, id: 1 })
  client.sendMessage(ev.user, { type: AooDataType.text, data: Buffer.from(`Hello ${ev.user} from ${USER}!`) }, true)
})
client.on("peerLeave", (ev) => {
  console.log("peer leaving")
  sink.uninviteSource(ev.endpoint)
})
client.on("notification", ev => console.log("server says:", Buffer.from(ev.data).toString()))
client.on("event", ev => console.log("client event:", ev.type)) 