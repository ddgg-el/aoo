// @ts-check
import { createRequire } from "module"
import { fileURLToPath } from "url"
import { dirname, join } from "path"
import { existsSync } from "fs"
import { pollable, NATIVE } from "./events.mjs"

const require = createRequire(import.meta.url)
const here = dirname(fileURLToPath(import.meta.url))

function loadAddon() {
  for (const variant of ["Release", "Debug"]) {
    const p = join(here, "build", variant, "aoo_native.node")
    if (existsSync(p)) return require(p)
  }
  throw new Error("aoo-native: no compiled addon found — run `npm run build`")
}

export class AooClient {
  constructor() {
    /** @type AooClient */
    const client = pollable(
      new aoo.AooClient(), 
      [{ pollMethod: "pollPackets", eventName: "packet"}],
      {
        async close() {
          try{ await client.leaveGroup() } catch {}
          try{ await client.disconnect() } catch {}
          client.stop()
        }
      }
    )
    return client  
  }
}

export class AooServer {
  constructor() {
    return pollable(new aoo.AooServer())
  }
}

export class AooSource {
  /** @param {number} id  */
  constructor(id) {
    return pollable(new aoo.AooSource(id))
  }
}

export class AooSink {
  /** @param {number} id  */
  constructor(id) {
    return pollable(new aoo.AooSink(id), 
    [{ pollMethod: "pollStreamMessages", eventName: "streamMessage" }],
    )
  }
}


const aoo = loadAddon()

export const aoo_strerror = aoo.aoo_strerror
export const aoo_terminate = aoo.aoo_terminate
export const messageType = aoo.messageType
export const AooResampleMethod = aoo.AooResampleMethod
export const AooMsgType = aoo.AooMsgType
export const AooDataType = aoo.AooDataType
export const AooOpusApplication = aoo.AooOpusApplication
export const AooOpusSignalType = aoo.AooOpusSignalType

export const aoo_version = aoo.aoo_version
export default aoo