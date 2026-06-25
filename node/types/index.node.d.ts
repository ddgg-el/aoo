import { AooSinkBase } from "./core.js";
export declare function aoo_initialize(): Promise<void>;
export declare class AooSink extends AooSinkBase {
    process(): Float32Array;
}
export { AooSourceBase as AooSource, AooDataType, AooMsgType, messageType, AooResampleMethod, aoo_terminate, aoo_version, aoo_strerror } from "./core.js";
export type { AooFormat, AooEndpoint, AooDataTypes, AooSourceEvent, AooSinkEvent, AooStreamMessage, AooSendCallback, AooSourceEventHandler, AooSinkEventHandler, AooStreamMessageHandler } from "./core.js";
