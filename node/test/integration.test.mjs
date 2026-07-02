// @ts-check
/** @import {AooClientEvent, AooServerEvent } from "aoo-native" */
import test from "node:test"
import assert from "node:assert/strict"
import { AooServer, AooClient, AooDataType } from "aoo-native"

const PORT = 17017
const GROUP = "itest"

/**
 * 
 * @param { number } ms 
 * @returns 
 */
const delay = (ms) => new Promise((r) => setTimeout(r, ms))

async function waitFor(fn, timeoutMs = 5000) {
	const end = Date.now() + timeoutMs
	while (Date.now() < end) { if (fn()) return true; await delay(50) }
	return false
}

/**
 * 
 * @param { AooClient | AooServer } obj 
 * @returns 
 */
function collect(obj) {
	/** @type { any[]} */
	const events = []
	obj.on("event", (ev) => events.push(ev))
	return events
}

test("server + two clients: discovery, peer message, notification", async (t) => {
	const server = new AooServer(); server.start(PORT)
	/** @type AooServerEvent[] */
	const serverEvents = collect(server); 

	const a = new AooClient(); a.start(0); 
	const b = new AooClient(); b.start(0); 
	/** @type AooClientEvent[]   */
	const aEv = collect(a)
	/** @type AooClientEvent[]   */
	const bEv = collect(b)

	t.after(() => { a.stop(); b.stop(); server.stop() })
	
	await a.join("localhost", PORT, GROUP, "alice")
	await b.join("localhost", PORT, GROUP, "bob")


	// discovery both ways
	assert.ok(await waitFor(() =>
		aEv.some((e) => e.type === "peerJoin" && e.user === "bob") &&
		bEv.some((e) => e.type === "peerJoin" && e.user === "alice")),
		"both peers discover each other")

	// server observed the joins
	assert.ok(serverEvents.some((e) => e.type === "groupJoin" && e.user === "alice"), "server saw alice join")

	// peer message A -> B, addressed by name
	a.sendMessage("bob", { type: AooDataType.text, data: Buffer.from("hi bob") }, true)
	assert.ok(await waitFor(() =>
		bEv.some((e) => e.type === "peerMessage" && Buffer.from(e.data).toString() === "hi bob")),
		"bob receives alice's peer message")

	// server notification -> alice
	const join = serverEvents.find((e) => e.type === "groupJoin" && e.user === "alice")
	server.notifyClient(join?.clientId, { type: AooDataType.text, data: Buffer.from("welcome") })
	assert.ok(await waitFor(() =>
		aEv.some((e) => e.type === "notification" && Buffer.from(e.data).toString() === "welcome")),
		"alice receives the server notification")
})