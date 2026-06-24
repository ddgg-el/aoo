import { aoo_initialize, AooSink } from "aoo"
import "./style.css"

const CHANNELS = 1
// const BLOCK = 256
// const CAPACITY = BLOCK * CHANNELS * 4 // ring capacity 4 blocks
// const TARGET = BLOCK * CHANNELS * 4

const WS_URL = "ws://localhost:8081"
const SOURCE_ADDR = { ip: "127.0.0.1", port: 9001, id: 1 }

await aoo_initialize()

const aooSink = new AooSink(1)
aooSink.setLatency(0.05)

const ctx = new AudioContext()
const sinkNode = await aooSink.createOutputNode(ctx, CHANNELS)
const gain = new GainNode(ctx, { gain: 0.1 })
sinkNode.connect(gain).connect(ctx.destination)

const dec = new TextDecoder()
aooSink.setEventHandler((ev) => console.log(ev.type))
aooSink.setStreamMessageHandler((msg) => {
	console.log(`msg "${dec.decode(msg.data)}" @${msg.time?.toFixed(3) ?? "?"}s `)
})

/**
 * WebSocket setup
*/
const ws = new WebSocket(WS_URL)
ws.binaryType = "arraybuffer"
ws.onopen = () => console.log("WS connected")
ws.onmessage = (ev) => {
	aooSink.handleMessage(new Uint8Array(ev.data as ArrayBuffer), SOURCE_ADDR.ip, SOURCE_ADDR.port)
}

/**
 * Audio Setup
 */
// const dac = new AudioContext()
// await dac.audioWorklet.addModule("/aoo-receive.worklet.js")
// const node = new AudioWorkletNode(dac, "aoo-receive", {
// 	outputChannelCount: [CHANNELS],
// 	processorOptions: {
// 		dataSab, ctrlSab, channels: CHANNELS, capacity: CAPACITY
// 	}
// })

// node.connect(dac.destination)
// // await dac.resume()

const tickFunc = () => {
	aooSink.send((bytes:Uint8Array) => {
		if(ws.readyState === 1) ws.send(bytes.slice())
	})
	aooSink.pollEvents()
	aooSink.pollStreamMessages()
}

const btn = document.getElementById("start-audio") as HTMLButtonElement
let timer:ReturnType<typeof setInterval>|null = null

btn.onclick = async (ev) => {
	await ctx.resume()	
	timer ??= setInterval(tickFunc, 5)
	btn.innerText = "Stop Audio"
}

// aooSink.inviteSource("127.0.0.1", 9000, 1);
// let n = 0

// const drain = () => aooSink.send((bytes:Uint8Array, ip:string, port:number) => {
// 	n++
// 	log.push(`  send #${n}: ${bytes.length} B -> ${ip}:${port}  [${Array.from(bytes).map((ascii) => String.fromCharCode(ascii)).join("")}]`)
// })

// Beat the clock forward past INVITE_INTERVAL (0.1 s), draining each tick.
// for (let i = 0; i <= 10; i++) {
//   aooSink.process(i * 0.05)   // elapsed: 0, 0.05, 0.10, ...
//   drain()
// }
// log.push(`drained ${n} packet(s)`)
// console.log("drained", n, "packet(s)");





