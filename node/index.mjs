// @ts-check
import { createRequire } from "module"
import { fileURLToPath } from "url"
import { dirname, join } from "path"
import { existsSync } from "fs"

const require = createRequire(import.meta.url)
const here = dirname(fileURLToPath(import.meta.url))

function loadAddon() {
  for (const variant of ["Release", "Debug"]) {
    const p = join(here, "build", variant, "aoo_native.node")
    if (existsSync(p)) return require(p)
  }
  throw new Error("aoo-native: no compiled addon found — run `npm run build`")
}

const aoo = loadAddon()
export const AooClient = aoo.AooClient
export const AooSource = aoo.AooSource
export const AooSink = aoo.AooSink
export const aoo_version = aoo.aoo_version
export default aoo