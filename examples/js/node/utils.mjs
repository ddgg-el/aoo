import portAudio from "naudiodon2"

/**
 * 
 * @param {String} deviceName 
 * @returns {Number} device Index
 */
export function chooseAudioDevice(deviceName) {
	const devices = portAudio.getDevices()
	let selected = -1
	for (const device of devices) {
	  if(device.name === deviceName) {
		selected = device.id
	  }
	}
	return selected
}