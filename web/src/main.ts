import createModule from "../js/aoo.mjs"

const aoo = await createModule()

const log = [`AOO version -> ${aoo.versionString()}`]
log.push("\nInitializing AOO...")
log.push(`AOO -> ${aoo.initialize() === 0 ? "Initialized" : "Error"}`)

// aoo.setLogHandler((level, msg) => { /* show it */ })

const recv = new aoo.AooReceive(1)
log.push("\nInitializing AooReceive...")
log.push(`AooReceive -> ${recv.setup(2,48000, 256) === 0 ? "Initialized" : "Error Initializing AooReceive"}`)

recv.inviteSource("127.0.0.1", 9000, 1);
let n = 0

const drain = () => recv.send((bytes:Uint8Array, ip:string, port:number) => {
	n++
	log.push(`  send #${n}: ${bytes.length} B -> ${ip}:${port}  [${Array.from(bytes).map((ascii) => String.fromCharCode(ascii)).join("")}]`)
})

// Beat the clock forward past INVITE_INTERVAL (0.1 s), draining each tick.
for (let i = 0; i <= 10; i++) {
  recv.process(i * 0.05)   // elapsed: 0, 0.05, 0.10, ...
  drain()
}
log.push(`drained ${n} packet(s)`)
// console.log("drained", n, "packet(s)");

recv.delete()


log.push("\nTerminating AOO")
log.push(`AOO -> ${aoo.terminate() == undefined ? "Terminated" : "Could not terminate AOO" }`)
document.querySelector("#out")!.textContent = log.join("\n");