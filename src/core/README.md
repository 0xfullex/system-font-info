# src/core

The platform-independent model and the JS boundary. No file here calls an OS font API; no backend includes `napi.h`.

## Files

| File | Role |
| --- | --- |
| `value.h` | `Value`: a JSON-like tree (null, bool, number, string, array, ordered object) plus `Shared` nodes. |
| `model.h` | `Family`, `Face` and their parts: the one result shape every backend fills. |
| `platform.h` | The interface each backend implements. |
| `to_js.h`, `to_js.cpp` | Converts a `std::vector<Family>` and `Value` trees to JS values. |
| `addon.cpp` | `NODE_API_MODULE` entry: `getFonts`, `startWatching`, `stopWatching` and the `__testing` hooks. |

## Invariants

- **Shared objects stay shared.** Within one conversion, every `Value::Shared` pointer and every `SharedAxes` pointer becomes exactly one JS object, reused by every face that references it (the memo is keyed by pointer). The JS layer relies on this to process a shared `opentype` or `axes` object once.
- **Keys and nesting follow `../../ts/types.ts`.** Optional fields (`file`, `file.index`, `axes`, `coordinates`, `namedInstance`, `opentype`, `coverage`) are present only when set. The native face lacks `fullName`, `styleName` and the axes' `name`; the JS layer derives them.
- **Nothing runs on the JS thread that can block.** `enumerateFonts` runs in an `AsyncWorker`. Conversion happens on the JS thread, family by family inside a handle scope, with persistent references for the shared-object memo.
- **One watcher per environment.** State lives in the env's instance data. Notifications are coalesced: while a call is queued and undelivered, further native notifications are dropped (an atomic flag cleared on the JS thread right before the callback runs). The thread-safe function is unref'd, so watching never keeps the process alive. `stopWatching` destroys the watcher (joining its thread) before releasing the thread-safe function, and an environment cleanup hook does the same at teardown. Calling `startWatching` while watching throws.
- **Errors.** Backends throw `std::exception`; `getFonts` rejects with an `Error` carrying the message. Argument errors throw `TypeError` / `RangeError` synchronously.
- `__testing.readFontFile` runs the same table reader a backend uses, over a file, and is for tests only.
