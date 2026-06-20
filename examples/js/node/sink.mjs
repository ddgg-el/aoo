// @ts-check
/**
 * @typedef {(bytes: Uint8Array, ip: string, port: number) => void} AooSendCallback
 * * @typedef {{ ip: string, port: number, id: number }} AooEndpoint
 * * @typedef {(
 *   | { type: "sourceAdd" | "sourceRemove", endpoint: AooEndpoint }
 *   | { type: "sourcePing", endpoint: AooEndpoint, rtt: number }
 *   | { type: "streamStart" | "streamStop", endpoint: AooEndpoint }
 *   | { type: "streamState", state: string, sampleOffset: number }
 *   | { type: "streamLatency", sourceLatency: number, sinkLatency: number, bufferLatency: number }
 *   | { type: "formatChange", codec: string, channels: number, sampleRate: number, blockSize: number }
 *   | { type: "invite" | "uninvite", endpoint: AooEndpoint, token: number }
 *   | { type: "frameResend", endpoint: AooEndpoint, count: number }
 *   | { type: "bufferUnderrun" | "bufferOverrun" | "streamTime" | "blockDrop" | "blockResend" | "blockXRun"}
 *   | { type: "frameResend", endpoint: AooEndpoint, count: number }
 * )} AooSourceEvent
 * @typedef {(ev: AooSourceEvent) => void} AooEventHandler
 * @typedef {{channel:number, sampleOffset:number,type:number, source:{ip:string, port:number},data:ArrayBuffer}} AooSourceMessage
 * @typedef {(msg:AooSourceMessage) => void} AooMessageHandler
 */ 

import createModule from "aoo";
import dgram from "node:dgram";
import portAudio from "naudiodon2"
import { chooseAudioDevice } from "./utils.mjs";

const SINK_ID = 1
const PORT = 9001

const CHANNELS = 1
const SR = 48000
const BLOCK = 256
const DEVICE = chooseAudioDevice("MacBook Pro Speakers")

const aoo = await createModule();
aoo.initialize();

// TODO: aoo.setLogHandler?.((lvl, msg) => console.log("[aoo]", msg));
// console.log(aoo)

const sink = new aoo.AooSink(SINK_ID);
sink.setup(CHANNELS, SR, BLOCK);
sink.setLatency(0.05);

sink.setEventHandler( /** @type {AooEventHandler} */ (ev) => {
  switch (ev.type) {
    case "sourceAdd":
      console.log(`sourceAdd     -> ${ev.endpoint.ip}:${ev.endpoint.port} id=${ev.endpoint.id}`)
      break
    case "sourceRemove":
      console.log(`sourceRemove  -> ${ev.endpoint.ip}:${ev.endpoint.port} id=${ev.endpoint.id}`)
      break
    case "streamStart":
      console.log(`streamStart   -> ${ev.endpoint.ip}:${ev.endpoint.port}`)
      break
    case "streamStop":
      console.log(`streamStop    -> ${ev.endpoint.ip}:${ev.endpoint.port}`)
      break
    case "streamState":
      console.log(`streamState   -> ${ev.state} (offset ${ev.sampleOffset})`)
      break
    case "streamLatency":
      console.log(`streamLatency -> source=${(ev.sourceLatency*1000).toFixed(1)}ms sink=${(ev.sinkLatency*1000).toFixed(1)}ms buffer=${(ev.bufferLatency*1000).toFixed(1)}ms`)
      break
    case "formatChange":
      console.log(`formatChange  -> ${ev.codec} ${ev.channels}ch @ ${ev.sampleRate}Hz block=${ev.blockSize}`)
      break
    case "sourcePing":
      console.log(`sourcePing    -> rtt=${(ev.rtt*1000).toFixed(2)}ms`)
      break
    case "bufferUnderrun":
      console.log("bufferUnderrun!")
      break
    case "bufferOverrun":
      console.log("bufferOverrun!")
      break
    case "streamTime":			
    case "blockDrop":
    case "blockResend":
    case "blockXRun":
      break
    default:
      console.log("default event:", ev.type)
  }
})

const dec = new TextDecoder()
sink.setStreamMessageHandler( /** @type {AooMessageHandler} */ (msg) => {
  const bytes = new Uint8Array(msg.data)
  if(msg.type === aoo.kAooDataText) {
    console.log(`streamMsg [text] #${msg.sampleOffset} ch${msg.channel} from ${msg.source.ip}:${msg.source.port}: "${dec.decode(bytes)}"`)
  } else {
    console.log(`streamMsg [type ${msg.type}] ${bytes.length} bytes`)
  }
})

// TODO: verify IPV6 protocol 
// const sock = dgram.createSocket({ type: "udp6", ipv6Only: false });
const sock = dgram.createSocket("udp4");

sock.on("message", (msg, rinfo) => {
  sink.handleMessage(new Uint8Array(msg), rinfo.address, rinfo.port);
});

const pa = portAudio.AudioIO({
  outOptions: {
    channelCount: CHANNELS,
    sampleFormat: portAudio.SampleFormatFloat32,
    sampleRate: SR,
    deviceId: DEVICE,
    closeOnError: false
  }
})

pa.start()

/** @type {AooSendCallback} */
const forward = (bytes, ip, port) => {
  sock.send(Buffer.from(bytes), port, ip);
}

function renderAudioBlock() {
  const audio = sink.process()
  sink.send(forward);

  const buf = Buffer.from(audio.buffer.slice(audio.byteOffset, audio.byteOffset + audio.byteLength))
  if(pa.write(buf)) {
    setImmediate(renderAudioBlock)
  } else {
    pa.once("drain", renderAudioBlock)
  }
}

renderAudioBlock()

process.on("SIGINT", () => {
  pa.quit(); 
  sock.close(); 
  sink.delete()
  aoo.terminate(); 
  process.exit(0)
})

sock.bind(PORT, () => {
  console.log(`AOO sink on udp/${PORT}, id ${SINK_ID}`)
});