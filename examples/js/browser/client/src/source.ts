import { aoo_initialize, AooSource } from "aoo";
import "./style.css"

const CHANNELS = 1
const SINK_ADDRESS = { ip: "127.0.0.1", port: 9001, id: 1}
const WS_URL = "ws://localhost:8081"

const out = document.getElementById("out") as HTMLPreElement
const btn = document.getElementById("start") as HTMLButtonElement
const vol = document.getElementById("vol") as HTMLInputElement

const ws = new WebSocket(WS_URL)
ws.binaryType = "arraybuffer"
ws.onopen = () => console.log("WS connected")

btn.onclick = async () => {
	btn.disabled = true
	await aoo_initialize()

	const aooSource = new AooSource(1)
	
	const ctx = new AudioContext()
	
	const node = await aooSource.createInputNode(ctx, CHANNELS, {codec: "opus", bitrate: 6400, complexity: 5})
	const osc = new OscillatorNode(ctx, { frequency: 200 })
	const gain = new GainNode(ctx, { gain: parseFloat(vol.value) })
	
	osc.connect(gain).connect(node)
	osc.start()
	await ctx.resume()

	aooSource.addSink(SINK_ADDRESS)
	aooSource.startStream()

	ws.onmessage = (ev) => {
		aooSource.handleMessage(new Uint8Array(ev.data as ArrayBuffer), SINK_ADDRESS.ip, SINK_ADDRESS.port)
	}


	vol.oninput = (ev) => {
		gain.gain.value = parseFloat(vol.value)
	}

	let bytes = 0
	let packets = 0
	// streaming loop
	setInterval(() => {
		aooSource.send((b: Uint8Array) => { 
			if(ws.readyState === 1) {

				ws.send(b.slice())
			}
			bytes += b.length
			packets++ 
		})
		aooSource.pollEvents()
	}, 5)

	setInterval(() => { out.textContent = `bytes ${bytes} packets ${packets} B`}, 200)
}