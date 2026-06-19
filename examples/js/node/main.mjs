import createModule from "aoo";
import dgram from "node:dgram";
import portAudio from "naudiodon2"

const SINK_ID = 1
const PORT = 9001
const CHANNELS = 2
const SR = 48000
const BLOCK = 256

const M = await createModule();
M.initialize();

// TODO: M.setLogHandler?.((lvl, msg) => console.log("[aoo]", msg));
// console.log(M)

const sink = new M.AooSink(SINK_ID);
sink.setup(CHANNELS, SR, BLOCK);
sink.setLatency(0.05);

// TODO: verify IPV6 protocol 
// const sock = dgram.createSocket({ type: "udp6", ipv6Only: false });
const sock = dgram.createSocket("udp4");

sock.on("message", (msg, rinfo) => {
  sink.handleMessage(new Uint8Array(msg), rinfo.address, rinfo.port);
});

sock.bind(PORT, () => console.log(`AOO sink on udp/${PORT}, id ${SINK_ID}`));

const pa = new portAudio.AudioIO({
  outOptions: {
    channelCount: CHANNELS,
    sampleFormat: portAudio.SampleFormatFloat32,
    sampleRate: SR,
    deviceId: -1,
    closeOnError: false
  }
})

pa.start()

function renderAudioBlock() {
  const audio = sink.processNow()
  sink.send((bytes, ip, port) => {
    sock.send(Buffer.from(bytes), port, ip);   // copy! view is transient
  });

  const buf = Buffer.from(audio.buffer.slice(audio.byteOffset, audio.byteOffset + audio.byteLength))
  if(pa.write(buf)) {
    setImmediate(renderAudioBlock)
  } else {
    pa.once("drain", renderAudioBlock)
  }
}

renderAudioBlock()

process.on("SIGINT", () => {
  pa.quit(); sock.close(); sink.delete()
  M.terminate(); process.exit(0)
})
