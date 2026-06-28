import { WebSocket, WebSocketServer } from "ws"
import { AooClient } from "aoo-native"

const WS_PORT = 8081
const wss = new WebSocketServer({port: WS_PORT})
const clients = new Set<WebSocket>

let sinkAddr: {ip:string, port:number}| null = null


function frame(ip: string, port: number, payload: Uint8Array): Buffer {
	const ipb = Buffer.from(ip, "utf8")
	const head = Buffer.alloc(3)
	head.writeUInt16BE(port, 0); head.writeUInt8(ipb.length, 2)
	return Buffer.concat([head, ipb, payload])
}
function unframe(buf: Buffer) {
	const port = buf.readUInt16BE(0), ipLen = buf.readUInt8(2)
	return { ip: buf.toString("utf8", 3, 3 + ipLen), port, payload: buf.subarray(3 + ipLen) }
}


wss.on("connection", (ws) => {
	clients.add(ws)
	const aooClient = new AooClient()
	aooClient.start(0)
	console.log(`client connected (${clients.size} total)`)

	const poll = setInterval(() => {
		for (const ev of aooClient.pollEvents()) {
			ws.send(JSON.stringify({ kind: "event", ...ev}))
		}

		for (const packet of aooClient.pollPackets()) {
			ws.send(frame(packet.ip, packet.port, packet.bytes), { binary: true})
		}
		// const packets = aooClient.pollPackets()
		// for (const packet of packets) {
		// 	for (const ws of clients) {
		// 		if(ws.readyState === WebSocket.OPEN) {
		// 			ws.send(packet.bytes, {binary: true})
		// 		}
		// 	}
		// }
	}, 5)
	
	ws.on("message", (data:Buffer, isBinary:boolean) => {
		if(!isBinary) {
			const msg = JSON.parse(data.toString())
			if(msg.type === "join") {
				aooClient.join(msg.ip, msg.port, msg.group, msg.username)
				// aooClient.connect(msg.ip, msg.port)
				// aooClient.joinGroup(msg.group, msg.username)
			}
			return
		}
		// for (const peer of clients) {
		// 	if(peer !== ws && peer.readyState === WebSocket.OPEN) {
		// 		peer.send(data, {binary: true})
		// 	}
		// }
		const { ip, port, payload } = unframe(data)
		// if(sinkAddr) {
			aooClient.sendPacket(payload, ip, port)
		// }
	})

	ws.on("close", () => {
		clearInterval(poll)
		aooClient.stop()
		clients.delete(ws)
		console.log(`client disconnected (${clients.size} total)`)
	})
})

console.log(`WS relay on ws://localhost:${WS_PORT}`)

