// @ts-check
import { AooClient } from "aoo-native";
const client = new AooClient()
client.start(0)
async function connect() {
	try {
		await client.connect("127.0.0.1", 7079)
		console.log("connected")
	} catch (error) {
		console.error("rejected", error.message)
	}
}

connect().catch(r => console.log(r)).finally(() => {
	client.stop()
	process.exit(0)
})