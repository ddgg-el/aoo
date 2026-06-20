// @ts-check
/**
 * @typedef {(bytes: Uint8Array, ip: string, port: number) => void} AooSendCallback 
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
  const audio = sink.processNow()
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