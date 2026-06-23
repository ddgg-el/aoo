import createCore from "./dist/aoo.node.mjs"
import { initWith, isInitialized, AooSinkBase } from "./core.js"

export async function aoo_initialize(): Promise<void> {
	if(isInitialized()) return
	initWith(await createCore())
}

export class AooSink extends AooSinkBase {
	process(): Float32Array {
		return (this.raw.process() as Float32Array).slice()
	}
}

export { AooSource, 
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