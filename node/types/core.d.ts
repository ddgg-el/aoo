export interface AooEndpoint {
    ip: string;
    port: number;
    id: number;
}
export type AooStreamState = "inactive" | "active" | "buffering";
export type AooSourceEvent = {
    type: "sinkAdd" | "sinkRemove";
    endpoint: AooEndpoint;
} | {
    type: "sinkPing";
    endpoint: AooEndpoint;
    rtt: number;
    packetLoss: number;
} | {
    type: "invite" | "uninvite";
    endpoint: AooEndpoint;
    token: number;
} | {
    type: "frameResend";
    endpoint: AooEndpoint;
    count: number;
};
export type AooSinkEvent = {
    type: "sourceAdd" | "sourceRemove";
    endpoint: AooEndpoint;
} | {
    type: "sourcePing";
    endpoint: AooEndpoint;
    rtt: number;
} | {
    type: "streamStart" | "streamStop";
    endpoint: AooEndpoint;
} | {
    type: "streamState";
    endpoint: AooEndpoint;
    state: AooStreamState;
    sampleOffset: number;
} | {
    type: "streamLatency";
    endpoint: AooEndpoint;
    sourceLatency: number;
    sinkLatency: number;
    bufferLatency: number;
} | {
    type: "formatChange";
    endpoint: AooEndpoint;
    codec: string;
    channels: number;
    sampleRate: number;
    blockSize: number;
} | {
    type: "bufferUnderrun" | "bufferOverrun";
    endpoint: AooEndpoint;
} | {
    type: "blockDrop" | "blockResend" | "blockXRun";
    endpoint: AooEndpoint;
    count: number;
} | {
    type: "streamTime";
    endpoint: AooEndpoint;
    sourceTime: number;
    sinkTime: number;
    sampleOffset: number;
};
export type AooSourceEventHandler = (ev: AooSourceEvent) => void;
export type AooSinkEventHandler = (ev: AooSinkEvent) => void;
export type AooFormat = {
    codec: "pcm";
} | {
    codec: "opus";
    application?: "audio" | "lowdelay" | "voip";
    blockSize?: number;
    bitrate?: number;
    complexity?: number;
};
export interface AooStreamMessage {
    sampleOffset: number;
    channel: number;
    type: number;
    data: Uint8Array;
    source: AooEndpoint;
    streamSample?: number;
    time?: number;
}
export type AooStreamMessageHandler = (msg: AooStreamMessage) => void;
export type AooSendCallback = (bytes: Uint8Array, ip: string, port: number) => void;
export declare function getCore(): any;
export declare function isInitialized(): boolean;
export declare function initWith(m: any): void;
export declare function aoo_version(): string;
export declare function aoo_strerror(code: number): string;
export declare function aoo_terminate(): void;
export interface AooResampleMethods {
    hold: number;
    linear: number;
    cubic: number;
}
export declare const AooResampleMethod: AooResampleMethods;
/** AooDataType values, sourced from the C++ constants */
export interface AooDataTypes {
    raw: number;
    text: number;
    osc: number;
    midi: number;
    json: number;
}
export declare const AooDataType: AooDataTypes;
export interface AooMsgTypes {
    source: number;
    sink: number;
}
export declare const AooMsgType: AooMsgTypes;
export declare function messageType(bytes: Uint8Array): number;
export interface AooOpusApplications {
    audio: number;
    lowdelay: number;
    voip: number;
}
export declare const AooOpusApplication: AooOpusApplications;
export interface AooOpusSignalTypes {
    music: number;
    voice: number;
    auto: number;
}
export declare const AooOpusSignalType: AooOpusSignalTypes;
export declare abstract class AooStreamEndpoint<E> {
    protected readonly raw: any;
    constructor(raw: any);
    setup(channels: number, sampleRate: number, blockSize: number): void;
    send(cb: AooSendCallback): number;
    handleMessage(bytes: Uint8Array, ip: string, port: number): number;
    reset(): void;
    setId(id: number): void;
    setPacketSize(bytes: number): void;
    setPingInterval(seconds: number): void;
    setDllBandwidth(q: number): void;
    setResampleMethod(method: number): void;
    setBinaryFormat(enabled: boolean): void;
    setEventHandler(cb: (ev: E) => void): void;
    pollEvents(): number;
    setDynamicResampling(enabled: boolean): void;
    setBufferSize(seconds: number): void;
    getRealSampleRate(): number;
    eventsAvailable(): boolean;
    delete(): void;
    [Symbol.dispose](): void;
}
export declare class AooSourceBase extends AooStreamEndpoint<AooSourceEvent> {
    constructor(id: number);
    setFormat(format?: AooFormat): void;
    setOpusBitrate(bitrate: number): void;
    setOpusComplexity(complexity: number): void;
    setOpusSignalType(signalType: AooOpusSignalTypes): void;
    setRedundancy(n: number): void;
    setResendBufferSize(seconds: number): void;
    setStreamTimeSendInterval(seconds: number): void;
    addSink(endpoint: AooEndpoint): void;
    startStream(): void;
    stopStream(sampleOffset?: number): void;
    process(interleaved: Float32Array): number;
    addStreamMessage(type: number, data: Uint8Array, sampleOffset?: number, channel?: number): number;
    removeSink(endpoint: AooEndpoint): void;
    activate(endpoint: AooEndpoint, active: boolean): void;
    setSinkChannelOffset(endpoint: AooEndpoint, offset: number): void;
    handleInvite(endpoint: AooEndpoint, token: number, accept: boolean): void;
    handleUninvite(endpoint: AooEndpoint, token: number, accept: boolean): void;
    removeAllSinks(): void;
}
export declare class AooSinkBase extends AooStreamEndpoint<AooSinkEvent> {
    constructor(id: number);
    setLatency(seconds: number): void;
    setResendData(enabled: boolean): void;
    setResendInterval(seconds: number): void;
    setResendLimit(n: number): void;
    inviteSource(endpoint: AooEndpoint): void;
    uninviteSource(endpoint: AooEndpoint): void;
    resetSource(endpoint: AooEndpoint): void;
    getBufferFillRatio(endpoint: AooEndpoint): number;
    uninviteAll(): void;
    setStreamMessageHandler(cb: AooStreamMessageHandler): void;
    pollStreamMessages(): number;
}
