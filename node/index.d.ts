/* ----------------------------- shared types ----------------------------- */

export type AooEndpoint = {
  ip:string
  port:number
  id:number
}

/* ----------------------------- events ----------------------------- */

type AooEventName =
  | "error" | "sinkPing" | "sourcePing" | "invite" | "uninvite"
  | "sinkAdd" | "sinkRemove" | "sourceAdd" | "sourceRemove"
  | "frameResend"
  | "streamStart" | "streamStop" | "streamState" | "streamTime" | "streamLatency" | "formatChange"
  | "inviteDecline" | "inviteTimeout" | "uninviteTimeout"
  | "bufferOverrun" | "bufferUnderrun" | "blockDrop" | "blockResend" | "blockXRun" | "frameResend"
  | "disconnect" | "notification" | "groupEject"
  | "peerPing" | "peerState" | "peerHandshake" | "peerTimeout"
  | "peerJoin" | "peerLeave" | "peerMessage" | "peerUpdate"
  | "groupUpdate" | "userUpdate" | "clientError"
  | "clientLogin" | "clientLogout"| "groupAdd" | "groupRemove" | "groupJoin" | "groupLeave"

type EventOf<E extends { type: string }, K extends string> = E extends unknown ? (K extends E["type"] ? E : never) : never
interface AooEmitter<E extends { type:string }, Extra = {}> {
  on ( event: "event", listener: (ev: E) => void ) : this
  once ( event: "event", listener: (ev: E) => void ) : this
  off ( event: "event", listener: (ev: E) => void ) : this 
  on <K extends E["type"] | keyof Extra>(event: K, listener: K extends keyof Extra ? Extra[K] : (ev: EventOf<E, K>) => void): this
  once<K extends E["type"] | keyof Extra>(event: K, listener: K extends keyof Extra ? Extra[K] : (ev: EventOf<E, K>) => void): this
  off <K extends E["type"] | keyof Extra>(event: K, listener: K extends keyof Extra ? Extra[K] : (ev: EventOf<E, K>) => void): this
  removeAllListeners(event?: string): this
  emit(event: string, ...args: any[]): boolean
}

type PacketEvents = { packet: (p: { bytes: Uint8Array; ip: string; port: number}) => void }
type StreamMsgEvents = { streamMessage: (m: AooStreamMessage) => void }

export type AooServerEvent = 
  | { type: "clientLogin"; id: number; error: number; version?: string }
  | { type: "clientLogout"; id: number; error: number; errorMessage?: string }
  | { type: "groupAdd"; id: number; name: string }
  | { type: "groupRemove"; id: number; name?: string }
  | { type: "groupJoin"; groupId: number; userId: number; clientId: number; group?: string; user?: string }
  | { type: "groupLeave"; groupId: number; userId: number; group?: string; user?: string }
  | { type: Exclude<AooEventName, "clientLogin" | "clientLogout" | "groupAdd" | "groupRemove" | "groupJoin" | "groupLeave"> }

export type AooClientEvent =
  | { type: "peerJoin" | "peerLeave" | "peerTimeout"; group: string; user: string; endpoint: AooEndpoint }
  | { type: "peerMessage"; group: number; user: string; userId:number; msgType: number; data: Uint8Array }
  | { type: "peerPing"; user: string; rtt:number}
  | { type: "notification"; msgType: number; data: Uint8Array }
  | { type: "disconnect"; error: number; message: string}
  | { type: Exclude<AooEventName, "peerJoin" | "peerLeave" | "peerTimeout" | "peerMessage" | "disconnect" | "notification" | "peerPing" | "disconnect"> } // numeric AOO event types not yet marshalled

export type AooSourceEvent =
  | { type: "sinkPing"; endpoint: AooEndpoint; rtt: number; packetLoss: number }
  | { type: "sinkAdd" | "sinkRemove"; endpoint: AooEndpoint }
  | { type: "invite" | "uninvite"; endpoint: AooEndpoint; token: number }
  | { type: "frameResend"; endpoint: AooEndpoint; count:number}
  | { type: number } // FIXME: string + Exclude

export type AooSinkEvent =
  | { type: "sourcePing"; endpoint: AooEndpoint; rtt: number }
  | { type: "sourceAdd" | "sourceRemove"; endpoint: AooEndpoint }
  | { type: "streamStart" | "streamStop"; endpoint: AooEndpoint }
  | { type: "streamState"; endpoint: AooEndpoint; state: "inactive" | "active" | "buffering"; sampleOffset: number }
  | { type: "formatChange"; endpoint: AooEndpoint; codec: string; channels: number; sampleRate: number; blockSize: number }
  | { type: "streamLatency"; endpoint: AooEndpoint; sourceLatency: number; sinkLatency: number; bufferLatency: number }
  | { type: "bufferUnderrun" | "bufferOverrun"; endpoint: AooEndpoint }
  | { type: "blockDrop" | "blockResend" | "blockXRun"; endpoint: AooEndpoint; count: number }
  | { type: "streamTime"; endpoint: AooEndpoint; sourceTime: number; sinkTime: number; sampleOffset: number }
  | { type: number } // FIXME: string + Exclude

export type AooSourceEventHandler = (ev: AooSourceEvent) => void
export type AooSinkEventHandler = (ev: AooSinkEvent) => void

/* --------------------------- stream format ---------------------------- */

export type AooFormat = 
  | { codec: "pcm" }
  | { codec: "opus"; 
    application?:'audio' | 'lowdelay' | "voip"; 
    blockSize?: number; 
    bitrate?: number; 
    complexity?: number } 

/* --------------------------- stream messages ---------------------------- */

export interface AooStreamMessage {
  sampleOffset: number
  channel: number
  type: number
  data: Uint8Array
  source: AooEndpoint
}

export type AooSendCallback = (bytes: Uint8Array, ip: string, port: number) => void

/* --------------------------- resample methods ---------------------------- */

export interface AooResampleMethods { 
  hold: number; 
  linear: number; 
  cubic: number 
}

export const AooResampleMethod: AooResampleMethods = {
	get hold() {return getCore().kAooResampleHold},
	get linear() {return getCore().kAooResampleLinear},
	get cubic() {return getCore().kAooResampleCubic}
}

/* --------------------------- data types ---------------------------- */

export interface AooDataTypes { 
  raw: number; 
  text: number; 
  osc: number; 
  midi: number; 
  json: number 
}

export const AooDataType: AooDataTypes = {
	get raw()  { return getCore().kAooDataRaw },
	get text() { return getCore().kAooDataText },
	get osc()  { return getCore().kAooDataOSC },
	get midi() { return getCore().kAooDataMIDI },
	get json() { return getCore().kAooDataJSON },
}

/* ---------------------------  message types ---------------------------- */

export interface AooMsgType { 
  source: number; 
  sink: number 
}

export const AooMsgType: AooMsgTypes = {
	get source() { return getCore().kAooMsgTypeSource},
	get sink() { return getCore().kAooMsgTypeSink}
}

/* --------------------------  OPUS application --------------------------- */

export interface AooOpusApplications { 
  audio: number; 
  lowdelay: number; 
  voip: number 
}

export const AooOpusApplication: AooOpusApplications = {
	get audio() { return getCore().OPUS_APPLICATION_AUDIO},
	get lowdelay() { return getCore().OPUS_APPLICATION_LOWDELAY},
	get voip() { return getCore().OPUS_APPLICATION_VOIP},
}

export interface AooOpusSignalTypes {
	music:number
	voice:number
	auto:number
}
export const AooOpusSignalType: AooOpusSignalTypes = {
	get music() { return getCore().OPUS_SIGNAL_MUSIC},
	get voice() { return getCore().OPUS_SIGNAL_VOICE},
	get auto() { return getCore().OPUS_AUTO}
}

export const AooOpusSignalType: { music: number; voice: number; auto: number }


export function aoo_strerror(code: number): string
export function aoo_terminate(): void
export function aoo_messageType(bytes: Uint8Array): number

/* --------------------------- AooStreamEndpoint -------------------------- */

interface AooStreamEndpoint<E> {
  setup(numChannels: number, sampleRate: number, blockSize: number): void
  send(cb: (bytes: Uint8Array, ip: string, port: number) => void): void
  handleMessage(bytes: Uint8Array, ip: string, port: number): void
  pollEvents(): E[]
  eventsAvailable(): boolean
  reset(): void
  setId(id: number): void
  setBufferSize(seconds: number): void
  setPacketSize(bytes: number): void
  setPingInterval(seconds: number): void
  setResampleMethod(method: number): void
  setDynamicResampling(enabled: boolean): void
  setBinaryFormat(enabled: boolean): void
  setDllBandwidth(q: number): void
  getRealSampleRate(): number
  delete(): void
  [Symbol.dispose](): void
}

/* --------------------------- AooSource -------------------------- */

export interface AooSource extends 
AooStreamEndpoint<AooSourceEvent>,
AooEmitter<AooSourceEvent> {}
export class AooSource {
  constructor(id:number)
  // setup(numChannels:number, sampleRate:number, blockSize:number)
  // send(cb:(bytes:Uint8Array, ip:string, port:number) => void): void
  // handleMessage(bytes: Uint8Array, ip: string, port: number): void
  // pollEvents(): AooSourceEvent[]
  // eventsAvailable(): boolean
  // reset(): void
  // setId(id: number): void
  // setBufferSize(seconds: number): void
  // setPacketSize(bytes: number): void
  // setPingInterval(seconds: number): void
  // setResampleMethod(method: number): void
  // setDynamicResampling(enabled: boolean): void
  // setBinaryFormat(enabled: boolean): void
  // setDllBandwidth(q: number): void
  // getRealSampleRate(): number

  addSink(endpoint:AooEndpoint): void
  removeSink(endpoint:AooEndpoint): void
  removeAllSinks(): void
  activate(endpoint:AooEndpoint, active:boolean): void
  setSinkChannelOffset(endpoint:AooEndpoint, offset:number): void
  startStream(): void
  stopStream(offset?:number): void
  process(samples:Float32Array): void
  handleInvite(endpoint:AooEndpoint, token:number, accept:boolean)
  handleUninvite(endpoint:AooEndpoint, token:number, accept:boolean)
  addStreamMessage(msg: { type: number; data: Uint8Array; sampleOffset?: number; channel?: number }): void

  setRedundancy(n: number): void
  setResendBufferSize(s: number): void
  setStreamTimeSendInterval(s: number): void

  setFormat(format?: AooFormat): void
  setOpusBitrate(bitrate: number): void
  setOpusComplexity(complexity: number): void
  setOpusSignalType(signal: "music" | "voice" | "auto"):void

}

/* --------------------------- AooSink -------------------------- */

export interface AooSink   extends AooEmitter<AooSinkEvent, StreamMsgEvents> {}
export class AooSink implements AooStreamEndpoint<AooSinkEvent> {
  constructor(id:number)
  // setup(numChannels:number, sampleRate:number, blockSize:number)
  // send(cb:(bytes:Uint8Array, ip:string, port:number) => void): void
  // handleMessage(bytes: Uint8Array, ip: string, port: number): void
  // pollEvents(): AooSourceEvent[]
  // eventsAvailable(): boolean
  // reset(): void
  // setId(id: number): void
  // setBufferSize(seconds: number): void
  // setPacketSize(bytes: number): void
  // setPingInterval(seconds: number): void
  // setResampleMethod(method: number): void
  // setDynamicResampling(enabled: boolean): void
  // setBinaryFormat(enabled: boolean): void
  // setDllBandwidth(q: number): void
  // getRealSampleRate(): number

  setLatency(seconds: number): void
  setResendData(b: boolean): void
  setResendInterval(s: number): void
  setResendLimit(n:number): void

  inviteSource(endpoint:AooEndpoint): void
  uninviteSource(ep:AooEndpoint): void
  resetSource(ep:AooEndpoint): void
  getBufferFillRatio(ep:AooEndpoint): number
  uninviteAll(): void
  /* set streamMessage handler */
  pollStreamMessages(): AooStreamMessage[]

  process(): Float32Array

  // delete(): void
  // [Symbol.dispose](): void
}

export interface AooClient extends AooEmitter<AooClientEvent, PacketEvents> {}
export class AooClient {
  constructor()
  /** Create the UDP socket + start network threads. Returns the bound port. */
  start(port: number, external?:boolean): number
  stop(): void
  addSource(source:AooSource): void
  addSink(sink:AooSink): void
  notify(): void
  connect(host: string, port: number): Promise<void>
  joinGroup(group: string, user: string, groupPassword?:string): Promise<{ userId: number; groupId: number }>
  leaveGroup(): Promise<void>
  join(server: string, port: number, group: string, user: string, groupPassword?:string): Promise<{userId: number; groupId: number}>
  sendMessage(user: number|string, msg: { type:number; data: Uint8Array}, reliable?:boolean): void
  /** Drain pending events; call on a timer. */
  pollEvents(): AooClientEvent[]
  /** Send a raw UDP packet out the client's socket to a numeric ip:port. */
  sendPacket(bytes: Uint8Array, ip: string, port: number): void
  pollPackets(): { bytes: Uint8Array; ip: string; port: number }[]
  userId(): number
  groupId(): number
  connected(): boolean
  removeSource(source: AooSource): void
  removeSink(sink: AooSink): void
  close(): Promise<void>
  disconnect(): Promise<void>
  [Symbol.dispose](): void
}

export interface AooServer extends AooEmitter<AooServerEvent> {}
export class AooServer {
  constructor()
  start(port:number): number
  stop(): void
  pollEvents(): AooServerEvent[]
  findGroup(name: string): number                       // groupId, or -1
  addGroup(name: string, password?: string): number     // groupId
  removeGroup(groupId: number): void
  findUserInGroup(groupId: number, userName: string): number  // userId, or -1
  removeUserFromGroup(groupId: number, userId: number): void
  notifyClient(client: number, msg: { type: number; data: Uint8Array }): void
  notifyGroup(group: number, user:number, msg: { type:number, data: Uint8Array}): void
  [Symbol.dispose](): void
}


export function aoo_version(): string
declare const _default: {
  AooClient: typeof AooClient
  AooServer: typeof AooServer
  AooSource: typeof AooSource
  AooSink: typeof AooSink
  aoo_strerror: typeof aoo_strerror
  aoo_terminate: typeof aoo_terminate
  aoo_messageType: typeof aoo_messageType
  AooResampleMethod: typeof AooResampleMethod
  AooMsgType: typeof AooMsgType
  AooDataType: typeof AooDataType
  AooOpusApplication: typeof AooOpusApplication
  AooOpusSignalType: typeof AooOpusSignalType
  aoo_version: typeof aoo_version
}

export default _default