export type AooFormat = 
  | { codec: "pcm" }
  | { codec: "opus"; application?:'audio' | 'lowdelay' | "voip"; blockSize?: number; bitrate?: number; complexity?: number } 
export type AooClientEvent =
  // | { type: "peerJoin" | "peerLeave"; group: string; user: string; ip: string; port: number; userId: number }
  | { type: "peerJoin" | "peerLeave"; group: string; user: string; endpoint: AooEndpoint }
  | { type: "disconnect" }
  | { type: number } // numeric AOO event types not yet marshalled

export type AooSourceEvent =
  | { type: "sinkPing"; endpoint: AooEndpoint; rtt: number; packetLoss: number }
  | { type: "sinkAdd" | "sinkRemove"; endpoint: AooEndpoint }
  | { type: "invite" | "uninvite"; endpoint: AooEndpoint; token: number }
  | { type: number }

export type AooSinkEvent =
  | { type: "sourcePing"; endpoint: AooEndpoint; rtt: number }
  | { type: "sourceAdd" | "sourceRemove"; endpoint: AooEndpoint }
  | { type: "streamStart" | "streamStop"; endpoint: AooEndpoint }
  | { type: "streamState"; endpoint: AooEndpoint; state: "inactive" | "active" | "buffering" }
  | { type: "formatChange"; endpoint: AooEndpoint; codec: string; channels: number; sampleRate: number; blockSize: number }
  | { type: number }

export type AooEndpoint = {
  ip:string
  port:number
  id:number
}

export interface AooStreamMessage {
  sampleOffset: number
  channel: number
  type: number
  data: Uint8Array
  source: AooEndpoint
}

export const AooResampleMethod: { hold: number; linear: number; cubic: number }
export const AooMsgType: { source: number; sink: number }
export const AooDataType: { raw: number; text: number; osc: number; midi: number; json: number }
export const AooOpusApplication: { audio: number; lowdelay: number; voip: number }
export const AooOpusSignalType: { music: number; voice: number; auto: number }


export function aoo_strerror(code: number): string
export function aoo_terminate(): void
export function messageType(bytes: Uint8Array): number

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
}

export class AooSource implements AooStreamEndpoint<AooSourceEvent> {
  constructor(id:number)
  setup(numChannels:number, sampleRate:number, blockSize:number)
  send(cb:(bytes:Uint8Array, ip:string, port:number) => void): void
  handleMessage(bytes: Uint8Array, ip: string, port: number): void
  pollEvents(): AooSourceEvent[]
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

  setRedundancy(n: number): void
  setResendBufferSize(s: number): void
  setStreamTimeSendInterval(s: number): void
  removeAllSinks(): void
  activate(endpoint:AooEndpoint, active:boolean): void
  setSinkChannelOffset(endpoint:AooEndpoint, offset:number): void

  setFormat(format?: AooFormat): void
  setOpusBitrate(bitrate: number): void
  setOpusComplexity(complexity: number): void
  setOpusSignalType(signal: "music" | "voice" | "auto"):void

  addSink(endpoint:AooEndpoint): void
  startStream(): void
  stopStream(): void
  process(samples:Float32Array): void
  removeSink(endpoint:AooEndpoint): void
  handleInvite(endpoint:AooEndpoint, token:number, accept:boolean)
  handleUninvite(endpoint:AooEndpoint, token:number, accept:boolean)

  addStreamMessage(msg: { type: number; data: Uint8Array; sampleOffset?: number; channel?: number }): void
  delete(): void

}

export class AooSink implements AooStreamEndpoint<AooSinkEvent> {
  constructor(id:number)
  setup(numChannels:number, sampleRate:number, blockSize:number)
  send(cb:(bytes:Uint8Array, ip:string, port:number) => void): void
  handleMessage(bytes: Uint8Array, ip: string, port: number): void
  pollEvents(): AooSourceEvent[]
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

  setResendData(b: boolean): void
  setResendInterval(s: number): void
  setResendLimit(n:number): void
  uninviteSource(ep:AooEndpoint): void
  uninviteAll(): void
  resetSource(ep:AooEndpoint): void
  getBufferFillRatio(ep:AooEndpoint): number

  setLatency(seconds: number): void
  process(): Float32Array
  inviteSource(endpoint:AooEndpoint): void

  pollStreamMessages(): AooStreamMessage[]
  delete(): void
}

export class AooClient {
  constructor()
  /** Create the UDP socket + start network threads. Returns the bound port. */
  start(port: number, external?:boolean): number
  stop(): void
  addSource(source:AooSource): void
  addSink(sink:AooSink): void
  notify(): void
  connect(host: string, port: number): void
  joinGroup(group: string, user: string): void
  join(server: string, port: number, group: string, user: string): void
  /** Drain pending events; call on a timer. */
  pollEvents(): AooClientEvent[]
  /** Send a raw UDP packet out the client's socket to a numeric ip:port. */
  sendPacket(bytes: Uint8Array, ip: string, port: number): void
  pollPackets(): { bytes: Uint8Array; ip: string; port: number }[]
  userId(): number

  removeSource(source: AooSource): void
  removeSink(sink: AooSink): void
}


export function aoo_version(): string
declare const aoo: {
  AooClient: typeof AooClient
  AooSource: typeof AooSource
  aoo_version: typeof aoo_version
}

export default aoo