# The Gridcoin.Qml module

This document records the names, units and threading rules of `Gridcoin.Qml`, the C++ side
of a QML front end (`src/qt/quick`). It describes what the module provides now and which
names later changes will add.

## 1. Building

- Configure with `-DENABLE_QML=ON`. It needs `-DENABLE_GUI=ON -DUSE_QT6=ON` and Qt 6.4 or
  later with the Qml and Quick components. The option is off by default.
- The module is `Gridcoin.Qml`: the backing library is `gridcoinqml` and the plugin target is
  `gridcoinqmlplugin`.
- The tests are `test_gridcoin-qml`, built into `<build>/qmltest/`. It is registered with
  CTest as `gridcoin_qml_tests`.
- No executable uses the module yet. Without the option the module is not built; the default
  build still changes in the core-message routing, the detach step and the detach guard's reap
  of the global thread pool.
- On Debian and Ubuntu the module needs `qt6-declarative-dev` to build and
  `qml6-module-qtquick` at run time. `qml6-module-qtqml-models` is needed by QML that
  imports `QtQml` (the module's tests import only `QtQuick`).

## 2. Names available now

Units rule: an amount is exposed as a `qint64` count of halfords plus a formatted string,
never as a floating-point GRC value. An enumeration is a `Q_ENUM` of the module, never an
integer literal. (No amount or enumeration is exposed yet; this is the rule for the names
that arrive later.)

### `Shell` (singleton)

| Name | Kind | Meaning |
|---|---|---|
| `marker` | property, constant | Identifies the provider that owns the instance; used by the identity tests. |
| `shown` | property | True once the composition root asked the root to show its main window (`showMain`). |
| `startMinimized` | property | True when the main window was asked to show minimized (`-min`). |
| `quitRequested` | property | True once a quit was requested; the root's close handler accepts the closes the quit sends. |
| `ipcActive` | property | True in a split (`-multiprocess`) build, where the IPC facts below are set. |
| `ipcGitCommitMismatch` | property | True when the GUI and the node were built from different commits. |
| `ipcGuiVersion` | property | The GUI process's own version. |
| `ipcNodeVersion` | property | The connected daemon's version. |
| `ipcNodeBuiltAt` | property | The daemon's build timestamp. |
| `ipcSchema` | property | The negotiated IPC schema, "major.minor". |
| `ipcProtocol` | property | The negotiated IPC protocol version. |
| `ipcSocketPath` | property | The socket the GUI connected on. |
| `ipcNodeIdentity` | property | The node's raw identity token; empty means unavailable. |
| `ipcNetwork` | property | The node's chain network ("main", "test", ...). |
| `requestQuit()` | invokable | Begins an application quit through the front end. |
| `requestClose()` | invokable | For the main window's close handler. Returns true when the window may close and the application quits; false when it was hidden to the tray instead (`hideToTrayRequested`). A hidden window with no tray could not be restored, so there is no hide without a tray. |
| `shownChanged`, `startMinimizedChanged`, `quitRequestedChanged`, `ipcInfoChanged` | signals | The matching property changed. |
| `coreMessage(caption, message, modal)` | signal | A core message, delivered queued: the thread that raised it never waits for QML. |
| `updateAvailable(caption, version, updateVersion, message)` | signal | The core found a newer version. |
| `buildMismatchWarning(guiCommit, nodeCommit)` | signal | The mixed-build banner. |
| `showMainRequested(minimized)` | signal | The root should show its main window. |
| `hideMainRequested()` | signal | The root should hide its main window (the first step of the detach). |
| `hideToTrayRequested()` | signal | Emitted by `requestClose()` when it hides to the tray. |

### `Splash` (singleton)

| Name | Kind | Meaning |
|---|---|---|
| `marker` | property, constant | As for `Shell`. |
| `loaded`, `total` | properties | The progress in the core's start-up message, or 0 and 0 when the message has none. |
| `message` | property | The whole text of the latest start-up message. |
| `active` | property | The splash was created and not yet destroyed. |
| `finished` | property | The core is ready and the splash was told to close. |
| `progressChanged`, `messageChanged`, `activeChanged`, `finishedChanged` | signals | The matching property changed. |

The splash reads progress from the core's two formats, `<loaded>/<highest> Blocks Loaded
(<n>%)` and `<depth>/<check depth> Blocks Verified`, both from `CTxDB::LoadBlockIndex`.

### The core bridge

`QmlCoreBridge` is the target of the core's string-invoked messages. Its slots only emit
signals and return, so the core thread is never held by QML.

| Slot | Effect |
|---|---|
| `error(caption, message, modal)` | `Shell.coreMessage`. Every message is delivered queued; a modal one is also written to the GUI log and stderr when it is posted. |
| `update(caption, version, updateVersion, message)` | `Shell.updateAvailable`. |
| `handleURI(uri)` | Stores the URI as `pendingUri` and emits `pendingUriChanged`. |

`pendingUri` keeps the URI until the send screen's adapter claims it with `takePendingUri()`.

## 3. Mapping from the QML front end's context properties

The QML front end on the `qml` branch of `ZiggySchulz/Gridcoin-Research` (at `3ca8d949f`)
binds eight context properties: `_initModel`, `_clientModel`, `_walletModel`,
`_researcherModel`, `_votingModel`, `_sendCoinsController`, `_nativeDialog` and
`_mrcModel`. This table maps their 48 members to `Gridcoin.Qml`. "Planned" names the
adapter source file that will provide the member in a later change; the QML name is fixed
there.

| Context property | Member | Gridcoin.Qml name | Notes |
|---|---|---|---|
| `_initModel` | `loaded` | `Splash.loaded` | |
| `_initModel` | `total` | `Splash.total` | |
| `_initModel` | `message` | `Splash.message` | |
| `_initModel` | `startMinimized` | `Shell.startMinimized` | |
| `_initModel` | `showSplashScreen()` | `Splash.active` | |
| `_initModel` | `initializationDone`, `doneLoading()` | `Splash.finished` and `Shell.showMainRequested(minimized)` | |
| `_initModel` | `hideSplashScreen()` | `Splash.finished` | |
| `_clientModel` | `numBlocks` | planned `clientstatus` | |
| `_clientModel` | `numBlocksPeers` | planned `clientstatus` | |
| `_clientModel` | `difficulty` | planned `clientstatus` | |
| `_clientModel` | `networkWeight` | planned `clientstatus` | |
| `_clientModel` | `coinWeight` | planned `clientstatus` | |
| `_clientModel` | `inSync` | planned `clientstatus` | Computed by the module's own sync rule. |
| `_clientModel` | `isTestNet` | planned `clientstatus` | |
| `_clientModel` | `statusBarWarnings` | planned `clientstatus` | |
| `_clientModel` | `minerWarnings` | planned `clientstatus` | |
| `_clientModel` | `fullVersion` | planned `aboutinfo` | |
| `_walletModel` | `balance` | planned `walletsummary` | Halfords plus a formatted string, not a double. |
| `_walletModel` | `stake` | planned `walletsummary` | Halfords plus a formatted string, not a double. |
| `_walletModel` | `unconfirmedBalance` | planned `walletsummary` | Halfords plus a formatted string, not a double. |
| `_walletModel` | `immatureBalance` | planned `walletsummary` | Halfords plus a formatted string, not a double. |
| `_walletModel` | `numTransactions` | planned `walletsummary` | |
| `_walletModel` | `encryptionStatus` | planned wallet-security adapter | An enum. |
| `_walletModel` | `transactionTableModel` | planned history model | |
| `_walletModel` | `addressTableModel` | planned address-book models | |
| `_walletModel` | `receiveAddressTableModel` | planned address-book models | |
| `_walletModel` | `unlockWallet(QString)` | planned wallet-operation gate: `answerUnlock(passphrase)` | |
| `_walletModel` | `cancelUnlock()` | planned wallet-operation gate: `cancelUnlock()` | |
| `_walletModel` | `requireUnlock()` | planned wallet-operation gate: `unlockRequested(mode)` | |
| `_walletModel` | `askFeeResult(bool)` | planned send adapter: `confirmFee(bool)` | |
| `_walletModel` | `askFeeDialog(int64_t)` | planned send adapter: `feeConfirmationRequested(fee)` | |
| `_researcherModel` | `magnitude` | planned `researchersummary` | |
| `_researcherModel` | `accrual` | planned `researchersummary` | |
| `_researcherModel` | `cpid` | planned `researchersummary` | |
| `_researcherModel` | `status` | planned `researchersummary` | |
| `_researcherModel` | `researcherMode` | planned `researchersummary` | An enum, not 0, 1 or 2. |
| `_votingModel` | `currentPollTitle` | planned `votingsummary` | |
| `_sendCoinsController` | `recipients` | planned send adapter | Typed recipients, amounts in halfords. |
| `_sendCoinsController` | `addRecipient()` | planned send adapter | |
| `_sendCoinsController` | `removeRecipient(int)` | planned send adapter | |
| `_sendCoinsController` | `updateRecipient(int, map)` | planned send adapter | |
| `_sendCoinsController` | `clearRecipients()` | planned send adapter | |
| `_sendCoinsController` | `sendCoins()` | planned send adapter | The result arrives through `finished(id, result)`. |
| `_sendCoinsController` | `coinsSentOrFailed(QString)` | planned send adapter | The result arrives through `finished(id, result)`. |
| `_nativeDialog` | `information()` | none | QML shows its own dialogs and answers through the request, signal and answer shape; the module never shows a blocking dialog. |
| `_nativeDialog` | `warning()` | none | As above. |
| `_nativeDialog` | `question()` | none | As above. |

`_mrcModel`: no member is used by the QML front end; the module's MRC adapter is planned.

## 4. Synchronous reads the module inherits

These are not made asynchronous by the module:

- `ClientModel::updateTimer` polls the node interface on the GUI thread on a timer.
- `WalletModel::drainEventQueue` drains wallet events on the GUI thread and calls into the
  wallet interface.

Under `-multiprocess` each is an IPC round trip on the GUI thread.

## 5. Request, signal, answer

Every prompt follows one shape. QML calls a `Q_INVOKABLE` request that returns at once with
a request id. The module emits a signal saying what it needs. QML answers through a second
`Q_INVOKABLE`. The module then emits `finished(id, result)`, or a failure signal that keeps
the request open. There is no nested event loop and no blocking connection anywhere in the
module; `test/lint/lint-qml-module.sh` checks it.

## 6. Lanes

`QmlCallRunner`'s serial lane (one thread, in order) runs every call not named here. The
pooled lane (the global `QThreadPool`) runs `checkForLatestUpdate`, `runDiagnostics`,
`executeRpcConsoleCommand` and the scraper snapshot.

Both lanes deliver on the GUI thread, queued, under a generation. `drain()` cancels queued
calls on both lanes and waits only for the in-flight serial call. After `close()` or
`drain()` every post is refused. A call still running when the runner closes, and that then
throws, is logged and not rethrown on the GUI thread. A pooled closure may capture
`interfaces::Node` but never an adapter or model pointer.

A pooled call starts at once unless the global pool is saturated; one still queued when the
runner closes or moves to a new generation does not run. A call posted once
`FrontEndDetachGuard` is armed (from `attachModels()` onward) is reaped before `node` is
destroyed, on every exit path. A call posted earlier (at `construct()` or during the
`isCoreReady()` wait) is not reaped before `node` is destroyed on a throw between
`makeNode()` and the guard. It was posted before `node` existed, so it cannot hold it.

## 7. Core messages

A core message raised in the GUI process while the main window exists reaches
`Shell.coreMessage` queued, from whatever thread raised it, and that thread never waits for
the QML front end. Under `-multiprocess`, messages raised in the daemon stay in the
daemon's log and stderr. A message raised while no main window exists (before it is
constructed, or once teardown has cleared it after the event loop) is not shown, and is
written to the GUI log and stderr, modal or not, as under Widgets. One raised after the
event loop has returned but before that teardown is not shown either, and only a modal one
is logged.

Each modal message is also written to the GUI log and to stderr when it is posted, so a
message whose dialog closes before it is read (for example one followed at once by a
shutdown) is still recorded. Until a later change adds the root's message dialog, and for
an init error that ends start-up until a later change adds its dialog, the log and stderr
are the only record. The Widgets front end still holds the raising thread until its dialog
closes.
