import dgram from "node:dgram"

const s = dgram.createSocket("udp4")
s.on("message", (m, r) => console.log(`got ${m.length}B from ${r.address}:${r.port}: ${m}`))
s.bind(12345, () => console.log("Listening on udp 12345"))