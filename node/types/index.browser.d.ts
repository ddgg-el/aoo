import { AooSinkBase, AooSourceBase, AooFormat } from "./core.js";
export declare function aoo_initialize(): Promise<void>;
export declare function createInputNode(ctx: AudioContext, channels: number): Promise<AudioWorkletNode>;
export declare class AooSource extends AooSourceBase {
    createInputNode(ctx: AudioContext, channels: number, format?: AooFormat): Promise<AudioWorkletNode>;
}
export declare class AooSink extends AooSinkBase {
    createOutputNode(ctx: AudioContext, channels: number): Promise<AudioWorkletNode>;
    playBackTime(): number;
    playBackSample(): number;
}
export { AooDataType, AooMsgType, messageType, AooResampleMethod, aoo_terminate, aoo_version, aoo_strerror } from "./core.js";
export type { AooFormat, AooEndpoint, AooDataTypes, AooSourceEvent, AooSinkEvent, AooStreamMessage, AooSendCallback, AooSourceEventHandler, AooSinkEventHandler, AooStreamMessageHandler } from "./core.js";
