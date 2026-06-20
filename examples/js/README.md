# AOO JavaScript / WebAssembly examples

Two end‑to‑end examples built on the WebAssembly bindings for AOO
(Audio Over OSC). Each pairs a wasm AOO **source** (generates a test tone)
with a wasm AOO **sink** (receives and plays the stream) — so **no native AOO
software (e.g. Pure Data) is required**; both ends run on the wasm `aoo`
package.

- **`node/`** — source and sink as two Node.js processes talking over a real
  UDP socket; audio plays through PortAudio.
- **`browser/`** — the sink runs in the browser (Web Audio). Because browsers
  cannot open UDP sockets, a small Node relay bridges the source's UDP to the
  browser over WebSocket.

All examples consume the local `aoo` package at `wasm/aoo` (referenced via
`file:` in each `package.json`). Build it first.

---

## Building the `aoo` package

Compiled with the [Emscripten SDK](https://emscripten.org/): networking **off**
(`AOO_NET=OFF` — the host owns the transport), the **Opus** codec on, and
single‑threaded. Output is a single‑file ES module plus TypeScript types.

```bash
# 1. Make sure the Opus submodule is present (needed for the codec)
git submodule update --init --recursive

# 2. Activate the Emscripten SDK so emcmake/emcc are on your PATH
source /path/to/emsdk/emsdk_env.sh

# 3. Configure + build (from the repository root)
emcmake cmake -S . -B build-wasm
cmake --build build-wasm --target aoo_wasm
```

This produces:

```
wasm/aoo/dist/aoo.mjs     # single-file ESM (wasm embedded)
wasm/aoo/dist/aoo.d.mts   # TypeScript declarations
```

After this, `npm install` inside any example resolves the `aoo` dependency to
that package.

---

## Example 1 — Node (`node/`)

Two processes over a real UDP socket: `source.mjs` generates a tone and streams
it to `sink.mjs`, which plays it through PortAudio (via
[`naudiodon2`](https://www.npmjs.com/package/naudiodon2)). Each end is clocked
by a PortAudio stream — the source by an input stream, the sink by an output
stream.

```bash
cd node
npm install

# terminal 1 — the receiver (binds UDP 9001, plays audio)
node sink.mjs

# terminal 2 — the source (generates a tone, sends to 127.0.0.1:9001)
node source.mjs
```

You should hear the tone once the sink's ~50 ms latency buffer fills.

> **Note:** `package.json` is meant to expose `run:sink` / `run:source`, but the
> key is misspelled `"script"` — it must be `"scripts"`. Fix that and you can use
> `npm run run:sink` / `npm run run:source` instead of `node …`.

- UDP port **9001** · 2 channels · 48000 Hz.

---

## Example 2 — Browser (`browser/`)

The sink runs in the browser; the source is the same Node `source.mjs`, bridged
in by a relay (browsers can't speak UDP):

```
source.mjs ──UDP 9001──► server (relay) ──WebSocket 8081──► client (browser) ──► Web Audio
```

- **`browser/server/`** — a Node relay bridging UDP (port **9001**) and the
  browser's WebSocket (port **8081**).
- **`browser/client/`** — a Vite + TypeScript app. The wasm sink writes decoded
  PCM into a `SharedArrayBuffer` ring buffer played by an `AudioWorklet`. (The
  `SharedArrayBuffer` requires cross‑origin isolation — Vite's dev/preview
  server sends the required COOP/COEP headers; see `vite.config.ts`.)

Run three things, in three terminals:

```bash
# 1) the relay
cd browser/server
npm install
npm run dev        # tsx --watch src/index.ts

# 2) the web client
cd browser/client
npm install
npm run dev        # Vite dev server; open the printed URL

# 3) the source (reuses the Node example's source)
cd node
node source.mjs
```

Open the page, click **Start audio** (a user gesture is required to start the
`AudioContext`), and you should hear the tone.

To run the client's production build instead of the dev server:

```bash
cd browser/client
npm run build      # tsc --noEmit && vite build  -> dist/
npm run preview    # serves dist/ WITH the COOP/COEP headers (required for SAB)
```

> Serve the built client with `npm run preview` (or any server that sends the
> COOP/COEP headers). A plain static server without them disables
> `SharedArrayBuffer` and the audio ring buffer fails.

- Source → relay UDP: **9001** · relay ↔ browser WebSocket: **8081**.

---

## Notes

- **Run only one receiver on UDP 9001 at a time** — either the Node `sink.mjs`
  (Example 1) or the relay (Example 2), not both.
- **Going cross‑machine:** the source sends to `127.0.0.1` by default. To stream
  to another host, change the destination in `source.mjs` and use a literal IPv4
  address (`localhost` may resolve to IPv6 `::1`, which the IPv4 sockets here do
  not receive).
