export type AooFormat = 
  | { codec: "pcm" }
  | { codec: "opus"; application?:'audio' | 'lowdelay' | "voip"; blockSize?: number; bitrate?: number; complexity?: number } 


type AooEventName =
  | "error" | "sinkPing" | "sourcePing" | "invite" | "uninvite"
  | "sinkAdd" | "sinkRemove" | "sourceAdd" | "sourceRemove"
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

export type AooSendCallback = (bytes: Uint8Array, ip: string, port: number) => void

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

export interface AooSource extends AooEmitter<AooSourceEvent> {}
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

export interface AooSink   extends AooEmitter<AooSinkEvent, StreamMsgEvents> {}
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

}


export function aoo_version(): string
declare const aoo: {
  AooClient: typeof AooClient
  AooSource: typeof AooSource
  aoo_version: typeof aoo_version
}

export default aoo