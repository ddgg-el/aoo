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
	| { type: "bufferUnderrun" | "bufferOverrun", endpoint: AooEndpoint }
	| { type: "blockDrop" | "blockResend" | "blockXRun", endpoint: AooEndpoint, count: number }
	| { type: "streamTime", endpoint: AooEndpoint, sourceTime: number, sinkTime: number, sampleOffset: number }

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

function check(code:number, op:string, obj:AooStreamEndpoint<any>|null = null): void { 
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

export interface AooResampleMethods {
	hold:number
	linear:number
	cubic:number
}

export const AooResampleMethod: AooResampleMethods = {
	get hold() {return getCore().kAooResampleHold},
	get linear() {return getCore().kAooResampleLinear},
	get cubic() {return getCore().kAooResampleCubic}
}

export const AooDataType: AooDataTypes = {
	get raw()  { return getCore().kAooDataRaw },
	get text() { return getCore().kAooDataText },
	get osc()  { return getCore().kAooDataOSC },
	get midi() { return getCore().kAooDataMIDI },
	get json() { return getCore().kAooDataJSON },
}

export interface AooMsgTypes { 
	source:number, 
	sink:number }

export const AooMsgType: AooMsgTypes = {
	get source() { return getCore().kAooMsgTypeSource},
	get sink() { return getCore().kAooMsgTypeSink}
}

export function messageType(bytes: Uint8Array): number {
	return getCore().messageType(bytes);
}

/* --------------------------- AooStreamEndpoint -------------------------- */
/* Shared base for AooSource and AooSink (the audio-streaming objects).
   Generic over the event type E so setEventHandler stays typed.   */

export abstract class AooStreamEndpoint<E> {
	protected readonly raw:any

	constructor(raw:any) {
		this.raw = raw
	}

	setup(channels:number, sampleRate:number, blockSize:number): void {
		check(this.raw.setup(channels, sampleRate, blockSize), "setup", this)
	}

	send(cb:AooSendCallback): number {
		return this.raw.send(cb)
	}

	handleMessage(bytes:Uint8Array, ip:string, port:number): number {
		return this.raw.handleMessage(bytes, ip, port)
	}

	reset(): void { 
		check(this.raw.reset(), "reset", this) 
	}

	setId(id: number): void { 
		check(this.raw.setId(id), "setId", this) 
	}

	setPacketSize(bytes: number): void { 
		check(this.raw.setPacketSize(bytes), "setPacketSize", this) 
	}

	setPingInterval(seconds: number): void { 
		check(this.raw.setPingInterval(seconds), "setPingInterval", this) 
	}

	setDllBandwidth(q: number): void { 
		check(this.raw.setDllBandwidth(q), "setDllBandwidth", this) 
	}

	setResampleMethod(method: number): void { 
		check(this.raw.setResampleMethod(method), "setResampleMethod", this) 
	}

	setBinaryFormat(enabled: boolean): void { 
		check(this.raw.setBinaryFormat(enabled), "setBinaryFormat", this) 
	}

	setEventHandler(cb:(ev:E) => void): void {
		this.raw.setEventHandler(cb)
	}

	pollEvents(): number { return this.raw.pollEvents() }

	setDynamicResampling(enabled: boolean): void {
		check(this.raw.setDynamicResampling(enabled), "setDynamicResampling", this)
	}

	setBufferSize(seconds: number): void {
		check(this.raw.setBufferSize(seconds), "setBufferSize", this)
	}

	getRealSampleRate(): number {
		return this.raw.getRealSampleRate()
	}

	eventsAvailable(): boolean {
		return this.raw.eventsAvailable()
	}

	delete():void {
		this.raw.delete()
	}
	[Symbol.dispose]():void { this.raw.delete() }
}

/* --------------------------- AooSourceBase -------------------------- */
/* Superclass that implements methods that can be used in both the Browser and in Node */

export class AooSourceBase extends AooStreamEndpoint<AooSourceEvent> {
	constructor(id:number) {
		super(new (getCore().AooSource)(id))
	}

	setFormat():void { 
		check(this.raw.setFormat(), "setFormat", this )
	}

	setRedundancy(n: number): void { 
		check(this.raw.setRedundancy(n), "setRedundancy", this) 
	}
	
	setResendBufferSize(seconds: number): void { 
		check(this.raw.setResendBufferSize(seconds), "setResendBufferSize", this) 
	}

	setStreamTimeSendInterval(seconds: number): void { 
		check(this.raw.setStreamTimeSendInterval(seconds), "setStreamTimeSendInterval", this) 
	}

	addSink(endpoint:AooEndpoint): void {
		check(this.raw.addSink(endpoint.ip, endpoint.port, endpoint.id), "addSink", this)
	}

	startStream():void { 
		check(this.raw.startStream(), "startStream", this) 
	}

	stopStream(sampleOffset = 0): void {
		check(this.raw.stopStream(sampleOffset), "stopStream", this)
	}
	
	process(interleaved: Float32Array): number {
		return this.raw.process(interleaved)
	}
	
	addStreamMessage(type:number, data:Uint8Array, sampleOffset = 0, channel = 0): number {
		return this.raw.addStreamMessage(type, data, sampleOffset, channel)
	}

	removeSink(endpoint:AooEndpoint): void { 
		check(this.raw.removeSink(endpoint.ip, endpoint.port, endpoint.id), "removeSink", this) 
	}

	activate(endpoint:AooEndpoint, active: boolean): void { 
		check(this.raw.activate(endpoint.ip, endpoint.port, endpoint.id, active), "activate", this) 
	}
	
	setSinkChannelOffset(endpoint:AooEndpoint, offset: number): void { 
		check(this.raw.setSinkChannelOffset(endpoint.ip, endpoint.port, endpoint.id, offset), "setSinkChannelOffset", this) 
	}

	handleInvite(endpoint:AooEndpoint, token:number, accept: boolean): void {
		check(this.raw.handleInvite(endpoint.ip, endpoint.port, endpoint.id, token, accept), "handleInvite", this)
	}

	handleUninvite(endpoint:AooEndpoint, token: number, accept: boolean): void {
		check(this.raw.handleUninvite(endpoint.ip, endpoint.port, endpoint.id, token, accept), "handleUninvite", this)
	}

	removeAllSinks(): void {
		check(this.raw.removeAllSinks(), "removeAllSinks", this)
	}
}

/* --------------------------- AooSinkBase -------------------------- */
/* Superclass that implements methods that can be used in both the Browser and in Node */

export class AooSinkBase extends AooStreamEndpoint<AooSinkEvent> {
	constructor(id: number) {
		super(new (getCore().AooSink)(id))
	}

	setLatency(seconds: number): void { 
		check(this.raw.setLatency(seconds), "setLatency", this) 
	}

	setResendData(enabled: boolean): void { 
		check(this.raw.setResendData(enabled), "setResendData", this) 
	}

	setResendInterval(seconds: number): void { 
		check(this.raw.setResendInterval(seconds), "setResendInterval", this) 
	}

	setResendLimit(n: number): void { 
		check(this.raw.setResendLimit(n), "setResendLimit", this) 
	}
	
	inviteSource(endpoint:AooEndpoint): void {
		check(this.raw.inviteSource(endpoint.ip, endpoint.port, endpoint.id), "inviteSource", this)
	}

	uninviteSource(endpoint:AooEndpoint): void { 
		check(this.raw.uninviteSource(endpoint.ip, endpoint.port, endpoint.id), "uninviteSource", this) 
	}

	resetSource(endpoint:AooEndpoint): void { 
		check(this.raw.resetSource(endpoint.ip, endpoint.port, endpoint.id), "resetSource", this) 
	}

	getBufferFillRatio(endpoint:AooEndpoint): number { 
		return this.raw.getBufferFillRatio(endpoint.ip, endpoint.port, endpoint.id) 
	}

	uninviteAll(): void {
		check(this.raw.uninviteAll(), "uninviteAll", this)
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

	pollStreamMessages(): number {
		return this.raw.pollStreamMessages()
	}
}