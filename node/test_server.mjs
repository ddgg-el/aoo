import { AooServer } from "aoo-native"
const server = new AooServer()
console.log(`AOO server on ${server.start(7078)}`)
setInterval(() => {
	for (const ev of server.pollEvents()) {
		console.log("server: ", ev)
	}
}, 200)