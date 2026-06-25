import { aoo_initialize, AooSink } from "aoo"
import "./style.css"

const CHANNELS = 1

const WS_URL = "ws://localhost:8081"
const SOURCE_ADDR = { ip: "127.0.0.1", port: 9001, id: 1 }

const out = document.getElementById("out") as HTMLPreElement

await aoo_initialize()

const aooSink = new AooSink(1)
aooSink.setLatency(0.15)

const ctx = new AudioContext()
const sinkNode = await aooSink.createOutputNode(ctx, CHANNELS)
const gain = new GainNode(ctx, { gain: 0.1 })
sinkNode.connect(gain).connect(ctx.destination)

const dec = new TextDecoder()

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

let bytes = 0
let packets = 0

const tickFunc = () => {
	aooSink.send((b:Uint8Array) => {
		if(ws.readyState === 1) {
			ws.send(b.slice())
		}
		bytes += b.length
		packets++
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

	setInterval(() => { out.textContent = `bytes ${bytes} packets ${packets} B`}, 5)
}



