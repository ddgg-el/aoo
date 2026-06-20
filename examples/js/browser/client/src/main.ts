import createModule from "aoo"

const CHANNELS = 2
const BLOCK = 256
const CAPACITY = BLOCK * CHANNELS * 4 // ring capacity 4 blocks
const TARGET = BLOCK * CHANNELS * 4

const WS_URL = "ws://localhost:8081"
const PD_PORT = 9000 
/**
 * Ring Buffers
 */
const dataSab = new SharedArrayBuffer(CAPACITY * 4)
const ctrlSab = new SharedArrayBuffer(8)
const data = new Float32Array(dataSab)
const ctrl = new Int32Array(ctrlSab)

/**
 * Audio Setup
 */
const dac = new AudioContext()
await dac.audioWorklet.addModule("/aoo-receive.worklet.js")
const node = new AudioWorkletNode(dac, "aoo-receive", {
	outputChannelCount: [CHANNELS],
	processorOptions: {
		dataSab, ctrlSab, channels: CHANNELS, capacity: CAPACITY
	}
})

node.connect(dac.destination)
// await dac.resume()

/**
 * AOO Setup
 */
const aoo = await createModule()

const log = [`AOO version -> ${aoo.versionString()}`]
log.push("\nInitializing AOO...")
log.push(`AOO -> ${aoo.initialize() === 0 ? "Initialized" : "Error"}`)

// TODO: intercept debug messages from AOO
// aoo.setLogHandler((level, msg) => { /* show it */ })

const aooSink = new aoo.AooSink(1)
log.push("\nInitializing aooSink...")
log.push(`aooSink -> ${aooSink.setup(CHANNELS, dac.sampleRate, BLOCK) === 0 ? "Initialized" : "Error Initializing aooSink"}`)

/**
 * WebSocket setup
*/
const ws = new WebSocket(WS_URL)
ws.binaryType = "arraybuffer"
ws.onopen = () => console.log("WS connected")
ws.onmessage = (ev) => {
	aooSink.handleMessage(new Uint8Array(ev.data as ArrayBuffer), "127.0.0.1", PD_PORT)
}


const tickFunc = () => {
	const read = Atomics.load(ctrl, 0)
	let write = Atomics.load(ctrl, 1)
	let used = (write - read + CAPACITY) % CAPACITY
	
	while(used < TARGET && (CAPACITY - 1 - used) >= BLOCK * CHANNELS) {
		try {
			const audio = aooSink.process()
			for (let i = 0; i < audio.length; i++) {
				data[write] = audio[i]
				write = (write + 1) % CAPACITY
			}
			used += BLOCK * CHANNELS
			aooSink.send((bytes:Uint8Array) => {
				if(ws.readyState === 1) {
					ws.send(bytes.slice())
				}
			})
			Atomics.store(ctrl, 1, write)
		} catch (error) {
			throw error;
			
		}
	}
}
let time:ReturnType<typeof setInterval>|null = null

const btn = document.getElementById("start-audio") as HTMLButtonElement

btn.onclick = async (ev) => {
	if(dac.state === "suspended") {
		try {
			await dac.resume()
			time = setInterval(tickFunc, 5)
			const b = ev.target as HTMLButtonElement
			b.innerText = "Stop Audio"
			// this.innerText("Stop Audio")
		} catch (error) {
			if(time) clearInterval(time)
		}
	} else {
		await dac.suspend()
		if(time) clearInterval(time)
		const b = ev.target as HTMLButtonElement
		b.innerText = "Start Audio"	
		// aooSink.delete()
		// console.log(aooSink)
		// log.push("\nTerminating AOO")
		// log.push(`AOO -> ${aoo.terminate() == undefined ? "Terminated" : "Could not terminate AOO" }`)
	}
}
document.querySelector("#out")!.textContent = log.join("\n");
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





