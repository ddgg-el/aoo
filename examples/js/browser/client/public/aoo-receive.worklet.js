class AooReceive extends AudioWorkletProcessor {
	constructor(opts) {
		super()
		const { dataSab, ctrlSab, channels, capacity } = opts.processorOptions
		this.data = new Float32Array(dataSab) // interleaved ring
		this.ctrl = new Int32Array(ctrlSab) // [readIdx, writeIdx]
		this.channels = channels
		this.capacity = capacity
	}

	process(_in, outputs) {
		const out = outputs[0]
		const frames = out[0].length
		let read = Atomics.load(this.ctrl, 0)
		const write = Atomics.load(this.ctrl, 1)
		let available = (write - read + this.capacity) % this.capacity

		for (let i = 0; i < frames; i++) {
			if(available >= this.channels) {
				for (let c = 0; c < this.channels; c++) {
					out[c][i] = this.data[read]
					read = (read + 1) % this.capacity
				}
				available -= this.channels
			} else {
				for (let c = 0; c < this.channels; c++) {
					out[c][i] = 0;
					
				}
			}
		}
		Atomics.store(this.ctrl, 0, read)
		return true
	}
}

registerProcessor("aoo-receive", AooReceive)