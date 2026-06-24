import { aoo_initialize, AooMsgType, AooSink, AooSource, messageType } from "aoo"
import "./style.css"

const CHANNELS = 1
const WS_URL = "ws://localhost:8081"
const REMOTE = { ip: "127.0.0.1", port: 9001, id: 1}

const out = document.getElementById("out") as HTMLPreElement
const btn = document.getElementById("start") as HTMLButtonElement
const freq = document.getElementById("freq") as HTMLInputElement

const ws = new WebSocket(WS_URL)
ws.binaryType = "arraybuffer"
ws.onopen = () => console.log("WS connected")

btn.onclick = async () => {
	btn.disabled = true
	btn.innerText = "STOP"
	await aoo_initialize()

	console.log("msgtypes:", AooMsgType.source, AooMsgType.sink)

	const ctx = new AudioContext()
	const source = new AooSource(1)
	const sink = new AooSink(1)
	sink.setLatency(0.05)

	const sinkNode = await sink.createOutputNode(ctx, CHANNELS)
	const gain = new GainNode(ctx, { gain: 0.3})
	sinkNode.connect(gain).connect(ctx.destination)
	
	const srcNode = await source.createInputNode(ctx, CHANNELS)
	const osc = new OscillatorNode(ctx, { frequency: parseFloat(freq.value)})
	osc.connect(srcNode)
	osc.start()

	freq.oninput = (ev) => {
		osc.frequency.value = parseFloat(freq.value)
	}

	let inB = 0
	ws.onmessage = (ev) => {
		const bytes = new Uint8Array(ev.data as ArrayBuffer)
		inB += bytes.length
		const t = messageType(bytes)
		if(t === AooMsgType.sink) sink.handleMessage(bytes, REMOTE.ip, REMOTE.port)
		else if(t === AooMsgType.source) source.handleMessage(bytes, REMOTE.ip, REMOTE.port)
	}

	await ctx.resume()
	source.addSink(REMOTE)
	source.startStream()

	sink.setEventHandler((ev) => console.log('sink:', ev.type))

	let outB = 0
	setInterval(() => {
		
		source.send((b:Uint8Array) => { 
			outB += b.length
			if(ws.readyState === 1) {
				ws.send(b.slice())
			}
		})
		sink.send((b:Uint8Array) => {
			if(ws.readyState === 1) {
				ws.send(b.slice())
			}
		})
		source.pollEvents()
		sink.pollEvents()
		sink.pollStreamMessages()
	}, 5)

	setInterval(() => { out.textContent = `out ${outB} B in ${inB} B`}, 200)
}