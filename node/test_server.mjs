import { AooDataType, AooServer } from "aoo-native"
const server = new AooServer()
console.log(`AOO server on ${server.start(7078)}`)

const lobby = server.addGroup("lobby", "none")
console.log("group 'lobby' id:", lobby, "findGroup:", server.findGroup("lobby"))

setInterval(() => {
	for (const ev of server.pollEvents()) {
		console.log("server: ", ev)
		switch (ev.type) {
			case "groupJoin":
				server.notifyClient(ev.clientId, { type: AooDataType.text, data: Buffer.from(`welcome ${ev.user}`)})
				console.log(`${ev.user} joined ${ev.group} (userId ${ev.userId});`)	
				if(ev.user) console.log(`findUserInGroup: ${server.findUserInGroup(ev.groupId, ev.user)}`)
				break;
		
			default:
				break;
		}
	}
}, 200)