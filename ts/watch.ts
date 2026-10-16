import { loadNative } from './native'

// One entry per watch() call, so subscribing the same function twice gives two
// independent subscriptions.
interface Subscription {
  onChange: () => void
}

const subscriptions = new Set<Subscription>()

function dispatch(): void {
  for (const subscription of [...subscriptions]) {
    // Skip a subscriber that was unsubscribed by an earlier one in this round.
    if (!subscriptions.has(subscription)) continue
    try {
      subscription.onChange()
    } catch (error) {
      // The other subscribers still run. The error is not swallowed: it is
      // rethrown from a microtask, so it surfaces as an uncaught exception the
      // way an error in any event callback does.
      queueMicrotask(() => {
        throw error
      })
    }
  }
}

/**
 * Calls `onChange`, with no arguments, whenever the set of installed fonts may have changed.
 * It carries no detail about what changed, and may fire when nothing observable did; call
 * getFonts() again to see the new state. Watching never keeps the process alive.
 * Returns a function that stops this subscription; calling it again does nothing.
 */
export function watch(onChange: () => void): () => void {
  if (typeof onChange !== 'function') throw new TypeError('watch: onChange must be a function')
  const native = loadNative()
  const subscription: Subscription = { onChange }
  if (subscriptions.size === 0) native.startWatching(dispatch)
  subscriptions.add(subscription)

  return () => {
    if (!subscriptions.delete(subscription)) return
    if (subscriptions.size === 0) native.stopWatching()
  }
}
