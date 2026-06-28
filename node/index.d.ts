export type AooClientEvent =
  | { type: "peerJoin" | "peerLeave"; group: string; user: string; ip: string; port: number; userId: number }
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
}

export class AooSource {
  constructor(id:number)
  setup(numChannels:number, sampleRate:number, blockSize:number)
  setFormat(): void
  addSink(endpoint:AooEndpoint): void
  startStream(): void
  stopStream(): void
  process(samples:Float32Array): void
  send(cb:(bytes:Uint8Array, ip:string, port:number) => void): void
  pollEvents(): AooSourceEvent[]
  removeSink(endpoint:AooEndpoint): void
}

export class AooSink {
  constructor(id:number)
  setup(numChannels: number, sampleRate: number, blockSize: number): void
  setLatency(seconds: number): void
  handleMessage(bytes: Uint8Array, ip: string, port: number): void
  process(): Float32Array
  send(cb: (bytes: Uint8Array, ip: string, port: number) => void): void
  pollEvents(): AooSinkEvent[]
}

export function aoo_version(): string
declare const aoo: {
  AooClient: typeof AooClient
  AooSource: typeof AooSource
  aoo_version: typeof aoo_version
}

export default aoo