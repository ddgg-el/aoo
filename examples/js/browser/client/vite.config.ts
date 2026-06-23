import { resolve } from "node:path"
import { defineConfig } from "vite"

const crossOriginIsolation = {
	"Cross-Origin-Opener-Policy": "same-origin",
	"Cross-Origin-Embedder-Policy": "require-corp"
}

export default defineConfig({
	build: { target: "esnext" },
	server: { 
		headers: crossOriginIsolation,
		fs: {
			allow: [".", "../../../../wasm/aoo"]
		}
	},
	// resolve: {
	// 	alias: {
	// 	'@': resolve(__dirname, 'src'), // Optional alias for cleaner imports
	// 	}
	// },
	preview: { headers: crossOriginIsolation },
	optimizeDeps: { exclude: ["aoo"] },
	assetsInclude: ["**/*.wasm"],
})