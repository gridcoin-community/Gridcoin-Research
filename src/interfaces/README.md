# interfaces

Pure-virtual interfaces that define the boundary between the GUI and the node,
introduced by the multiprocess program (design of record:
`doc/multiprocess_design.md`; RFC discussion #2937).

In the monolithic build the implementations are thin in-process wrappers over
the existing globals and registries (`src/node/interfaces.cpp`,
`src/wallet/interfaces.cpp`, later `src/gridcoin/interfaces.cpp`). In the
Stage 2 multiprocess build the same headers are backed by generated IPC
proxies instead — consumers do not change.

Current set:

- `handler.h` — `Handler`: RAII handle for a registered notification;
  `MakeSignalHandler` adapts a `boost::signals2::connection`.
- `node.h` — `Node`: chain/network state queries and node-side notification
  registration for the GUI's client model.
- `wallet.h` — `Wallet`: wallet queries and wallet notification registration.
- `init.h` — `Init`: per-process bootstrap; hands out the other interfaces.

The surface grows sub-phase by sub-phase (see `doc/multiprocess_design.md` §7);
methods are added when a consumer migrates onto them, not speculatively.

## Rules

1. **Only value types cross the boundary.** No `CBlockIndex*`, `CWalletTx&`,
   registry pointers, or references to global caches in any interface
   signature. If a method wants to return a struct, the struct is a plain
   value type.
2. **Notification callbacks must never take `cs_main`** (nor call interface
   methods that do). Gridcoin's core signals are emitted synchronously, often
   while the emitter holds `cs_main` or another core lock; a callback that
   re-enters core under those locks deadlocks the split build and silently
   serializes the monolithic one. Callbacks enqueue and return.
3. **`src/qt` may include only `src/interfaces/*.h`** (and Qt/GUI-local
   headers) once its migration completes. `test/lint/lint-qt-includes.sh`
   enforces this as a ratchet: the allowlist of existing offenders only
   shrinks. Interface headers themselves may include core headers during
   Phase 1; the Stage 2 schema generation is the final arbiter of
   marshalability.
