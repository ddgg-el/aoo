/**
 * @template {{ type: string }} E
 * @template {E['type']} T
 * @param {E[]} events 
 * @param {T} type 
 * @returns {(E & { type: T }) | undefined}
 */
export function findEvent(events, type) {
	return events.find(e => e.type === type)
}