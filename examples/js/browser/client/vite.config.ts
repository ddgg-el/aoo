import { defineConfig } from "vite"

const crossOriginIsolation = {
	"Cross-Origin-Opener-Policy": "same-origin",
	"Cross-Origin-Embedder-Policy": "require-corp"
}

export default defineConfig({
	build: { target: "esnext" },
	server: { headers: crossOriginIsolation },
	preview: { headers: crossOriginIsolation } 
})