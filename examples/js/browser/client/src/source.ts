import { aoo_initialize, AooSource } from "aoo";
import "./style.css"

const CHANNELS = 1
const SINK_ID = 1
const peers = new Map<string, { ip: string; port: number}>()
// const SINK_ADDRESS = { ip: "127.0.0.1", port: 9003, id: 1}
const SERVER_ADDRESS = { ip: "127.0.0.1", port: 7078}
const WS_URL = "ws://localhost:8081"

const out = document.getElementById("out") as HTMLPreElement
const btn = document.getElementById("start") as HTMLButtonElement
const vol = document.getElementById("vol") as HTMLInputElement

const ws = new WebSocket(WS_URL)
ws.binaryType = "arraybuffer"
ws.onopen = () => console.log("WS connected")

function frame(ip:string, port: number, payload: Uint8Array<ArrayBufferLike>): BufferSource {
	const ipb = new TextEncoder().encode(ip)
	const buf = new Uint8Array(3 + ipb.length + payload.length)
	new DataView(buf.buffer).setUint16(0, port)
	buf[2] = ipb.length
	buf.set(ipb, 3)
	buf.set(payload, 3 + ipb.length)
	return buf
}

function unframe(buf:Uint8Array) {
	const dv = new DataView(buf.buffer, buf.byteOffset, buf.byteLength)
	const port = dv.getInt16(0)
	const ipLen = buf[2]
	const ip = new TextDecoder().decode(buf.subarray(3, 3 + ipLen))
	return { ip, port, payload: buf.subarray(3 + ipLen)}
}

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

	ws.send(JSON.stringify({
		type: "join",
		ip: SERVER_ADDRESS.ip,
		port: SERVER_ADDRESS.port,
		group: "test-group",
		username: "browser-" + Math.floor(Math.random()* 1000)
	}))

	aooSource.startStream()

	ws.onmessage = (ev) => {
		if (typeof ev.data === "string") {
			const msg = JSON.parse(ev.data)
			if (msg.kind === "event") console.log("peer event:", msg)  // expect peerJoin/peerLeave
			if (msg.kind === "event" && msg.type === "peerJoin") {
				peers.set(`${msg.ip}:${msg.port}`, {ip: msg.ip, port: msg.port})
				aooSource.addSink({ip: msg.ip, port: msg.port, id: SINK_ID})
				console.log("addSink", msg.ip, msg.port)
			} else if (msg.kind === "event" && msg.type === "peerLeave") {
				peers.delete(`${msg.ip}:${msg.port}`)
				aooSource.removeSink({ip: msg.ip, port: msg.port, id: SINK_ID})
			}
			return
		}
		const { ip, port, payload } = unframe(new Uint8Array(ev.data))
		aooSource.handleMessage(payload, ip, port)
	}

	aooSource.setEventHandler((ev) => {
		console.log(ev)
	})


	vol.oninput = (ev) => {
		gain.gain.value = parseFloat(vol.value)
	}

	let bytes = 0
	let packets = 0
	// streaming loop
	setInterval(() => {
			aooSource.send((b, ip, port) => {
			if (ws.readyState === 1) ws.send(frame(ip, port, b))
			bytes += b.length
			packets++ 

		})
		
		aooSource.pollEvents()
	}, 5)

	setInterval(() => { out.textContent = `bytes ${bytes} packets ${packets} B`}, 200)
}