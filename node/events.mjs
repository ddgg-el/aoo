/** @import { AooSource, AooClient, AooSink } from "./index.mjs" */
import { EventEmitter } from "node:events";

export const NATIVE = Symbol("aooNative")
/**
 * 
 * @param {AooClient | AooSink | AooSource} native AOO Classes
 * @param { {pollMethod: string, eventName: string}[]} extraQueues 
 * @returns
 */
export function pollable(native, extraQueues = [], extraMethods = {}) {
	const emitter = new EventEmitter()
	emitter.setMaxListeners(0)
	
	/** @type ReturnType<typeof setInterval> | null */
	let timer = null
	let active = false

	const drainQueue = () => {
		for (const ev of native.pollEvents()) {
			emitter.emit(ev.type, ev)
		}
		for (const queue of extraQueues) {
			for (const item of native[queue.pollMethod]()) {
				emitter.emit(queue.eventName, item)
			}
		}
	}
	const startPolling = () => { 
		if(!timer) {
			timer = setInterval(drainQueue, 5)
		}
	}
	const stopPolling = () => {
		if(timer) {
			clearInterval(timer)
			timer = null
		}
	}

	const overrides = {
		[NATIVE]: native,
		on:   (eventName, listener) => {
			emitter.on(eventName, listener)
			startPolling()
			return wrapper
		},
		once: (eventName, listener) => {
			emitter.once(eventName, listener)
			startPolling()
			return wrapper
		},
		off: (eventName, listener) => {
			emitter.off(eventName, listener)
			return wrapper
		},
		removeAllListeners: (eventName) => {
			emitter.removeAllListeners(eventName)
			return wrapper
		},
		emit:  (...args) => emitter.emit(...args),
		
		...extraMethods
	}

	const wrapper = new Proxy(native, {
		get(target, property) {
			if(Object.hasOwn(overrides, property)) return overrides[property]
			
			if(property === "start") {
				return (...args) => {
					const r = target.start(...args);
					startPolling()
					return r
				}
			}

			if(property === "stop" || property === "delete") {
				return function(...args) {
					stopPolling()
					return target[property](...args)
				}
			}
			const value = target[property]
			if(typeof value !== "function") {
				return value
			}
			return function(...args) {
				const nativeArgs = args.map(arg => (arg && arg[NATIVE]) || arg)
				return value.apply(target, nativeArgs)
			}
		}
	})
	return wrapper
}