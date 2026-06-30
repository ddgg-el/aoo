import { AooDataType, AooServer } from "aoo-native"
const server = new AooServer()
console.log(`AOO server on ${server.start(7078)}`)

const lobby = server.addGroup("lobby", "none")
console.log("group 'lobby' id:", lobby, "findGroup:", server.findGroup("lobby"))

server.on("groupJoin", ev => {
  server.notifyClient(ev.clientId, { type: AooDataType.text, data: Buffer.from(`welcome ${ev.user}`) })
  console.log(`${ev.user} joined ${ev.group} (userId ${ev.userId})`)
  if (ev.user) console.log(`findUserInGroup: ${server.findUserInGroup(ev.groupId, ev.user)}`)
})
server.on("event", ev => console.log("server:", ev))   // was the catch-all log