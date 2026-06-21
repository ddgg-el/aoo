import createCore from "./dist/aoo.mjs"
import type {
	MainModule,
	AooSink as Sink,
	AooSource as Source
} from "./dist/aoo.mjs"

/* ----------------------------- shared types ----------------------------- */

export interface AooEndpoint {
	ip: string
	port: number
	id: number
}

/** AooDataType values, sourced from the C++ constants */
export interface AooDataTypes {
	raw: number
	text: number
	osc: number
	midi: number
	json: number
}

/* ------------------------------- events --------------------------------- */

export type AooSourceEvent = 
	| { type: "sinkAdd" | "sinkRemove", endpoint: AooEndpoint }
	| { type: "sinkPing", endpoint: AooEndpoint, rtt: number, packetLoss: number }
	| { type: "invite" | "uninvite", endpoint: AooEndpoint, token: number }
 	| { type: "frameResend", endpoint: AooEndpoint, count: number }

export type AooSinkEvent = 
	| { type: "sourceAdd" | "sourceRemove", endpoint: AooEndpoint }
 	| { type: "sourcePing", endpoint: AooEndpoint, rtt: number }
 	| { type: "streamStart" | "streamStop", endpoint: AooEndpoint }
 	| { type: "streamState", state: string, sampleOffset: number }
 	| { type: "streamLatency", sourceLatency: number, sinkLatency: number, bufferLatency: number }
 	| { type: "formatChange", codec: string, channels: number, sampleRate: number, blockSize: number }
 	| { type: "invite" | "uninvite", endpoint: AooEndpoint, token: number }
 	| { type: "frameResend", endpoint: AooEndpoint, count: number }
 	| { type: "bufferUnderrun" | "bufferOverrun" | "streamTime" | "blockDrop" | "blockResend" | "blockXRun"}
 	| { type: "frameResend", endpoint: AooEndpoint, count: number }

export type AooSourceEventHandler = (ev: AooSourceEvent) => void
export type AooSinkEventHandler = (ev: AooSinkEvent) => void

/* --------------------------- stream messages ---------------------------- */

export interface AooStreamMessage {
	sampleOffset:number
	channel:number
	type:number, 
	data:Uint8Array
	source:AooEndpoint
}

export type AooMessageHandler = (msg: AooStreamMessage) => void

export type AooSendCallback = (bytes: Uint8Array, ip: string, port: number) => void

/* ------------------------------- AooSource ------------------------------ */

let core: MainModule | null = null

function mod(): MainModule {
	if (!core) throw new Error("aoo: not initialized — call initialize() first")
	return core
}

function version(): string { return `AOO version: ${mod().versionString()}` }

function strerror(code: number): string { return mod().strerror(code) }

function check(code:number, op:string, obj:AooSink|AooSource|null = null): void { 
	if(code !== 0) {
		let origin = "aoo"
		if(obj) {
			origin = obj.constructor.name
		}
		throw new Error(`${origin}: ${op} failed: ${strerror(code)}`)
	}
}

export async function initialize(): Promise<void> {
	if (core) return
	core = await createCore()
	check(core.initialize(), "initialize")
	console.info(version())
}

export function terminate(): void { 
	core?.terminate() 
	core = null
}

export const DataType: AooDataTypes = {
	get raw()  { return mod().kAooDataRaw },
	get text() { return mod().kAooDataText },
	get osc()  { return mod().kAooDataOSC },
	get midi() { return mod().kAooDataMIDI },
	get json() { return mod().kAooDataJSON },
}

export class AooSource {
	readonly #raw: Source

	constructor(id:number) {
		this.#raw = new (mod().AooSource)(id)
	}

	setup(channels:number, sampleRate:number,blockSize:number):void {
		check(this.#raw.setup(channels, sampleRate, blockSize), "setup", this)
	}

	setFormat():void { check(this.#raw.setFormat(), "setFormat", this )}

	addSink(ip:string, port:number, id:number):void {
		check(this.#raw.addSink(ip, port, id), "addSink", this)
	}

	startStream():void { check(this.#raw.startStream(), "startStream", this) }
	
	process(interleaved: Float32Array): number {
		return this.#raw.process(interleaved)
	}

	handleMessage(bytes:Uint8Array, ip:string, port:number):number {
		return this.#raw.handleMessage(bytes, ip, port)
	}

	send(cb:AooSendCallback): number {
		return this.#raw.send(cb)
	}

	setEventHandler(cb:AooSourceEventHandler): void {
		this.#raw.setEventHandler(cb)
	}

	addStreamMessage(type:number, data:Uint8Array, sampleOffset = 0, channel = 0):number {
		return this.#raw.addStreamMessage(type, data, sampleOffset, channel)
	}

	delete():void {
		this.#raw.delete()
	}
	[Symbol.dispose]():void { this.#raw.delete() }
}

export class AooSink {
	readonly #raw: Sink

	constructor(id: number) {
		this.#raw = new (mod().AooSink)(id)
	}

	setup(channels: number, sampleRate: number, blockSize: number): void {
		check(this.#raw.setup(channels, sampleRate, blockSize), "setup", this)
	}

	setLatency(seconds: number): void { check(this.#raw.setLatency(seconds), "setLatency", this) }
	
	inviteSource(ip: string, port: number, id: number): void {
		check(this.#raw.inviteSource(ip, port, id), "inviteSource", this)
	}
	
	handleMessage(bytes: Uint8Array, ip: string, port: number): number {
		return this.#raw.handleMessage(bytes, ip, port)
	}

	send(cb: AooSendCallback): number { return this.#raw.send(cb) }

	/** Returns a COPY of the interleaved output block — safe to keep past this call. */
	process(): Float32Array {
		return (this.#raw.process() as Float32Array).slice()
	}

	setEventHandler(cb: AooSinkEventHandler): void {
		this.#raw.setEventHandler(cb)
	}

	setStreamMessageHandler(cb: AooMessageHandler): void {
		// copy the transient heap view out before handing the message to the user
		this.#raw.setStreamMessageHandler((msg: AooStreamMessage) => {
			cb({
				sampleOffset: msg.sampleOffset,
				channel: msg.channel,
				type: msg.type,
				data: new Uint8Array(msg.data),
				source: msg.source,
			})
		})
	}

	delete(): void { this.#raw.delete() }
	[Symbol.dispose](): void { this.#raw.delete() }
}