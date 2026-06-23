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

// TODO: verify with AooError
export type AooStreamState = "inactive" | "active" | "buffering"

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
 	| { type: "streamState", endpoint: AooEndpoint, state: AooStreamState, sampleOffset: number }
 	| { type: "streamLatency", endpoint: AooEndpoint, sourceLatency: number, sinkLatency: number, bufferLatency: number }
 	| { type: "formatChange", endpoint: AooEndpoint, codec: string, channels: number, sampleRate: number, blockSize: number }
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
	streamSample?:number // browser only
	time?:number // browser only
}

export type AooStreamMessageHandler = (msg: AooStreamMessage) => void

export type AooSendCallback = (bytes: Uint8Array, ip: string, port: number) => void

/* ------------------------------- AooSource ------------------------------ */

let core: any | null = null

export function getCore(): any {
	if (!core) throw new Error("aoo: not initialized — call initialize() first")
	return core
}

export function isInitialized(): boolean { return core !== null }

export function initWith(m:any): void {
	core = m
	check(core.initialize(), "initialize")
}

export function aoo_version(): string { return `AOO version: ${getCore().versionString()}` }

export function aoo_strerror(code: number): string { return getCore().strerror(code) }

function check(code:number, op:string, obj:AooSinkBase|AooSource|null = null): void { 
	if(code !== 0) {
		let origin = "aoo"
		if(obj) {
			origin = obj.constructor.name
		}
		throw new Error(`[${origin}]: ${op} failed: ${aoo_strerror(code)}`)
	}
}

export function aoo_terminate(): void { 
	core?.terminate() 
	core = null
}

export const AooDataType: AooDataTypes = {
	get raw()  { return getCore().kAooDataRaw },
	get text() { return getCore().kAooDataText },
	get osc()  { return getCore().kAooDataOSC },
	get midi() { return getCore().kAooDataMIDI },
	get json() { return getCore().kAooDataJSON },
}

export class AooSource {
	protected readonly raw: any

	constructor(id:number) {
		this.raw = new (getCore().AooSource)(id)
	}

	setup(channels:number, sampleRate:number, blockSize:number): void {
		check(this.raw.setup(channels, sampleRate, blockSize), "setup", this)
	}

	setFormat():void { 
		check(this.raw.setFormat(), "setFormat", this )
	}

	addSink(ip:string, port:number, id:number): void {
		check(this.raw.addSink(ip, port, id), "addSink", this)
	}

	startStream():void { 
		check(this.raw.startStream(), "startStream", this) 
	}
	
	process(interleaved: Float32Array): number {
		return this.raw.process(interleaved)
	}

	handleMessage(bytes:Uint8Array, ip:string, port:number): number {
		return this.raw.handleMessage(bytes, ip, port)
	}

	send(cb:AooSendCallback): number {
		return this.raw.send(cb)
	}
	
	addStreamMessage(type:number, data:Uint8Array, sampleOffset = 0, channel = 0): number {
		return this.raw.addStreamMessage(type, data, sampleOffset, channel)
	}

	setEventHandler(cb:AooSourceEventHandler): void {
		this.raw.setEventHandler(cb)
	}

	pollEvents(): number { return this.raw.pollEvents() }


	delete():void {
		this.raw.delete()
	}
	[Symbol.dispose]():void { this.raw.delete() }
}

export class AooSinkBase {
	protected readonly raw: any

	constructor(id: number) {
		this.raw = new (getCore().AooSink)(id)
	}

	setup(channels: number, sampleRate: number, blockSize: number): void {
		check(this.raw.setup(channels, sampleRate, blockSize), "setup", this)
	}

	setLatency(seconds: number): void { 
		check(this.raw.setLatency(seconds), "setLatency", this) 
	}
	
	inviteSource(ip: string, port: number, id: number): void {
		check(this.raw.inviteSource(ip, port, id), "inviteSource", this)
	}
	
	handleMessage(bytes: Uint8Array, ip: string, port: number): number {
		return this.raw.handleMessage(bytes, ip, port)
	}

	send(cb: AooSendCallback): number { 
		return this.raw.send(cb) 
	}

	setEventHandler(cb: AooSinkEventHandler): void {
		this.raw.setEventHandler(cb)
	}

	setStreamMessageHandler(cb: AooStreamMessageHandler): void {
		// copy the transient heap view out before handing the message to the user
		this.raw.setStreamMessageHandler((msg: AooStreamMessage) => {
			cb({
				...msg,
				data: new Uint8Array(msg.data),
			})
		})
	}

	// process(): Float32Array {
	// 	return (this.raw.process() as Float32Array).slice()
	// }


	pollEvents(): number { 
		return this.raw.pollEvents() 
	}

	pollStreamMessages(): number {
		return this.raw.pollStreamMessages()
	}

	delete(): void { this.raw.delete() }
	[Symbol.dispose](): void { this.raw.delete() }
}