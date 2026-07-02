const { existsSync } = require("fs")
const { join } = require("path")
const { EventEmitter } = require("events")

const NATIVE = Symbol("aooNative")

function loadAddon() {
  for (const variant of ["Release", "Debug"]) {
    const p = join(__dirname, "build", variant, "aoo_native.node")
    if (existsSync(p)) return require(p)
  }
  throw new Error("aoo-native: no compiled addon found — run `npm run build`")
}
const native = loadAddon()

function pollable(nativeObj, extraQueues = [], extraMethods = {}) {
  const emitter = new EventEmitter()
  emitter.setMaxListeners(0)
  let timer = null
  const drainQueue = () => {
    for (const ev of nativeObj.pollEvents()) {
		emitter.emit(ev.type, ev)
		emitter.emit("event", ev)
	}
    for (const q of extraQueues)
      for (const item of nativeObj[q.pollMethod]()) emitter.emit(q.eventName, item)
  }
  const startPolling = () => { if (!timer) timer = setInterval(drainQueue, 5) }
  const stopPolling  = () => { if (timer) { clearInterval(timer); timer = null } }

  const overrides = {
    [NATIVE]: nativeObj,
    on:   (e, f) => (emitter.on(e, f),   startPolling(), wrapper),
    once: (e, f) => (emitter.once(e, f), startPolling(), wrapper),
    off:  (e, f) => (emitter.off(e, f),  wrapper),
    removeAllListeners: (e) => (emitter.removeAllListeners(e), wrapper),
    emit: (...a) => emitter.emit(...a),
    ...extraMethods,
	[Symbol.dispose]() {
		stopPolling()
		const teardown = nativeObj.delete ?? nativeObj.stop
		if(teardown) teardown.call(nativeObj)
	}
  }
  const wrapper = new Proxy(nativeObj, {
    get(target, property) {
      if (Object.hasOwn(overrides, property)) return overrides[property]
      if (property === "start") return (...a) => { const r = target.start(...a); startPolling(); return r }
      if (property === "stop" || property === "delete") return (...a) => { stopPolling(); return target[property](...a) }
      const v = target[property]
      if (typeof v !== "function") return v
      return (...a) => v.apply(target, a.map(x => (x && x[NATIVE]) || x))
    },
  })
  return wrapper
}

class AooClient {
  constructor() {
    const client = pollable(new native.AooClient(),
      [{ pollMethod: "pollPackets", eventName: "packet" }],
      { async close() {
          try { await client.leaveGroup() } catch {}
          try { await client.disconnect() } catch {}
          client.stop()
        } })
    return client
  }
}
class AooServer { constructor() { return pollable(new native.AooServer()) } }
class AooSource { constructor(id) { return pollable(new native.AooSource(id)) } }
class AooSink   { constructor(id) { return pollable(new native.AooSink(id),
  [{ pollMethod: "pollStreamMessages", eventName: "streamMessage" }]) } }

module.exports = {
  AooClient, AooServer, AooSource, AooSink, NATIVE,
  aoo_strerror: native.aoo_strerror,
  aoo_terminate: native.aoo_terminate,
  aoo_messageType: native.aoo_messageType,
  AooResampleMethod: native.AooResampleMethod,
  AooMsgType: native.AooMsgType,
  AooDataType: native.AooDataType,
  AooOpusApplication: native.AooOpusApplication,
  AooOpusSignalType: native.AooOpusSignalType,
  aoo_version: native.aoo_version,
}