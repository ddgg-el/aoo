// @ts-check
const { existsSync } = require("fs")
const { join } =  require("path")

function loadAddon() {
  for (const variant of ["Release", "Debug"]) {
    const p = join(__dirname, "build", variant, "aoo_native.node")
    if (existsSync(p)) return require(p)
  }
  throw new Error("aoo-native: no compiled addon found — run `npm run build`")
}


module.exports = loadAddon()