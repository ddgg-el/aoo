import dgram, { type RemoteInfo } from "node:dgram"
import WebSocket, { WebSocketServer } from "ws"

const UDP_PORT = 9001
const WS_PORT = 8081

const udp = dgram.createSocket("udp4")
const wss = new WebSocketServer({port: WS_PORT})

let browser:WebSocket|null = null, sourceAddr:RemoteInfo|null = null

udp.on("message", (msg, rinfo) => {
	sourceAddr = rinfo
	if(browser?.readyState === 1) browser.send(msg)
})

udp.bind(UDP_PORT, () => console.log(`relay udp/${UDP_PORT} <-> ws/${WS_PORT}`))

wss.on("connection", (ws) => {
	browser = ws
	console.log("Browser connected")
	ws.on("message", (data) => {
		if(!sourceAddr) return 
		
		// const buf = Buffer.isBuffer(data) ? data :
		// 	Array.isArray(data) ? Buffer.concat(data) : Buffer.from(data)
		// udp.send(buf, sourceAddr.port, sourceAddr.address)
		udp.send(data as Buffer, sourceAddr.port, sourceAddr.address)
		
	})
})