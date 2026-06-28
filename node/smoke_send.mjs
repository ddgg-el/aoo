// @ts-check
// import { createRequire } from "module"
// const require = createRequire(import.meta.url)
import aoo from "aoo-native"

const client = new aoo.AooClient()
console.log("client up on port", client.start(0))

// setInterval(()=> {
// 	client.sendPacket(Buffer.from("hallo-aoo"), "127.0.0.1", 12345)
// 	console.log("sent")
// }, 1000)