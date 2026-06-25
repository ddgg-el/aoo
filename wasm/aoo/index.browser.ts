import createCore from "./dist/aoo.web.mjs"
import {
	initWith,
	isInitialized,
	getCore,
	AooSinkBase,
	AooSourceBase,
	AooFormat
} from "./core.js"

export async function aoo_initialize(): Promise<void> {
	if(isInitialized()) return
	initWith(await createCore())
}

export function createInputNode(ctx: AudioContext, channels: number): Promise<AudioWorkletNode> {
	return new Promise(resolve => {
		getCore().createInputNode(channels, ctx, (node:AudioWorkletNode) => resolve(node))
	})
}

export class AooSource extends AooSourceBase {
	createInputNode(ctx: AudioContext, channels: number, format?:AooFormat): Promise<AudioWorkletNode> {
		return new Promise(resolve => {
			getCore().createInputNode(this.raw, channels, ctx, (node: AudioWorkletNode) => resolve(node))
			this.setFormat(format)
			this.setBufferSize(0.05);
		})
	}
}

export class AooSink extends AooSinkBase {
	createOutputNode(ctx: AudioContext, channels: number): Promise<AudioWorkletNode> {
		return new Promise((resolve) => {
			getCore().createOutputNode(this.raw, channels, ctx, (node: AudioWorkletNode) => resolve(node))
		})
	}
	
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
	AooDataType, 
	AooMsgType,
	messageType,
	AooResampleMethod,
	aoo_terminate, 
	aoo_version, 
	aoo_strerror
} from "./core.js"

export type { 
	AooFormat,
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