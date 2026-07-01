// @ts-check
import { AooClient } from "aoo-native"

let shuttingDown = false
let backoff = 1000
const PASSWORD = "_"   // match your Max/Pd group
const SERVER_ADDR = {ip: "127.0.0.1", port: 7078}
const GROUP = "test-group"
const USER = "node-" + process.pid

const client = new AooClient()
client.start(0)

async function connect() {
  try {
	await client.join(SERVER_ADDR.ip, SERVER_ADDR.port, GROUP, USER, PASSWORD)
    console.log("joined", GROUP)
    backoff = 1000                         // reset on success
  } catch (e) {
	// @ts-ignore
    console.error("join failed:", e.message)
    scheduleReconnect()
  }
}

function scheduleReconnect() {
  if (shuttingDown) return
  console.log(`reconnecting in ${backoff} ms…`)
  setTimeout(connect, backoff)
  backoff = Math.min(backoff * 2, 15000)   // exponential, capped
}

client.on("disconnect", ev => {
  console.warn(`disconnected: ${ev.message} (code ${ev.error})`)
  scheduleReconnect()
})

await connect()   // initial

process.on("SIGINT", async () => {
  shuttingDown = true                       // stop the reconnect loop
//   try { pa.quit() } catch {}
  try { await client.close() } catch {}
  process.exit(0)
})