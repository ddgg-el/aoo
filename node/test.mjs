// @ts-check
import aoo from "aoo-native"

console.log("AOO native version:", aoo.aoo_version())

const client = new aoo.AooClient()
console.log("client up on port", client.start(0))

client.connect("localhost", 7078)
setTimeout(() => client.joinGroup("test-group", "user-"+process.pid), 500)

const poll = setInterval(() => {
	for (const ev of client.pollEvents()) {
		console.log("event: ", ev)
	}
},200)

setTimeout(() => {
	clearInterval(poll);
	client.stop()
	console.log("stopped cleanly")
}, 15000)