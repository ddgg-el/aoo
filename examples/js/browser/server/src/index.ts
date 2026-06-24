import dgram, { type RemoteInfo } from "node:dgram"
import { WebSocket, WebSocketServer } from "ws"

const WS_PORT = 8081
const UDP_PORT = 9001

const udp = dgram.createSocket("udp4")
const wss = new WebSocketServer({port: WS_PORT})
const clients = new Set<WebSocket>

let udpPeer:{ address: string, port:number} | null = null


udp.on("message", (data, rinfo) => {
	udpPeer = { address: rinfo.address, port: rinfo.port }
	for (const ws of clients) {
		if (ws.readyState === WebSocket.OPEN) {
			ws.send(data, { binary: true })
		}
	}
})

udp.bind(UDP_PORT, () => console.log(`relay udp/${UDP_PORT} <-> ws/${WS_PORT}`))

wss.on("connection", (ws) => {
	clients.add(ws)
	console.log(`client connected (${clients.size} total)`)
	ws.on("message", (data:Buffer) => {
		for (const peer of clients) {
			if(peer !== ws && peer.readyState === WebSocket.OPEN) {
				peer.send(data, {binary: true})
			}
		}
		if(udpPeer) {
			udp.send(data, udpPeer.port, udpPeer.address)
		}
		// udp.send(data as Buffer, sourceAddr.port, sourceAddr.address)
	})

	ws.on("close", () => {
		clients.delete(ws)
		console.log(`client disconnected (${clients.size} total)`)
	})
})

console.log(`WS relay on ws://localhost:${WS_PORT}`)

