import createCore from "./dist/aoo.web.mjs"
import {
	initWith,
	isInitialized,
	getCore,
	AooSinkBase
} from "./core.js"

export async function aoo_initialize(): Promise<void> {
	if(isInitialized()) return
	initWith(await createCore())
}

export class AooSink extends AooSinkBase {
	createOutputNode(ctx: AudioContext, channels: number): Promise<AudioWorkletNode> {
		return new Promise((resolve) => {
			getCore().createOutputNode(this.raw, channels, ctx, (node: AudioWorkletNode) => resolve(node))
		})
	}
	// connectToOutput(channels: number): void {
	// 	getCore().startAudioOutput(this.raw, channels)
	// }
	
	playBackTime(): number {
		return this.raw.playBackTime()
	}

	playBackSample(): number {
		return this.raw.playBackSample()
	}
}

// export function resumeAudio(): void {
// 	getCore().resumeAudio()
// }
export {
	AooSource, 
	AooDataType, 
	aoo_terminate, 
	aoo_version, 
	aoo_strerror
} from "./core.js"

export type { 
	AooEndpoint,
	AooDataTypes,
	AooSourceEvent,
	AooSinkEvent,
	AooStreamMessage,
	AooSendCallback,
	AooSourceEventHandler,
	AooSinkEventHandler, 
	AooStreamMessageHandler
} from "./core.js"