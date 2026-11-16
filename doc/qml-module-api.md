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
- With the option the build also produces `gridcoinresearch-qml` in `<build>/bin/`: the GUI
  with the QML front end (section 8). Without the option the module is not built; the default
  build still changes in the core-message routing, the detach step and the detach guard's reap
  of the global thread pool.
- On Debian 12 or later and Ubuntu 24.04 or later (Ubuntu 22.04's Qt 6.2 is below the 6.4
  floor) the module needs `qt6-declarative-dev` to build. At run time the module
  needs `qml6-module-qtquick`; the carried tree also needs `qml6-module-qtquick-controls`,
  `-layouts`, `-templates`, `-window`, `-shapes`, `qml6-module-qt-labs-platform`,
  `qml6-module-qtqml-models`, `qml6-module-qtqml-workerscript`,
  `qml6-module-qt5compat-graphicaleffects` and `libqt6shadertools6`. Five carried files import
  `QtQml`, among them the root `WindowManager.qml` that `gridcoinresearch-qml` loads, the theme
  and the clock, so the executable and `test_gridcoin-qml` (which loads them) need
  `qml6-module-qtqml-models`. Three carried files import `Qt5Compat.GraphicalEffects`
  (section 8), whose types do not load without Qt Shader Tools.
- `install_dependencies.sh` with `ENABLE_QML=true` installs the Qt5Compat GraphicalEffects
  module only on Debian, Ubuntu and Linux Mint. On every other system a QML build installs
  it, with Qt Shader Tools, by hand.
- `gridcoinresearch-qml` compiles its QML root at start, before it opens its log or connects
  to a daemon, and exits with a message naming what failed when the root does not compile;
  when the Qt5Compat effects module is the cause, the message says so.

## 2. Names available now

Units rule: an amount is exposed as a `qint64` count of halfords plus a formatted string,
never as a floating-point GRC value. An enumeration is a `Q_ENUM` of the module, never an
integer literal.

The status singletons below are owned by the provider, as `Shell` and `Splash` are, and are
attached to the models by `QmlFrontEnd::attachModels` and detached by the matching detach
hooks. Unattached, every invokable is safe and does nothing, or returns 0 or false. The
"Lane" of a name is where the read behind it runs (section 6): serial, pooled, or none (a
model signal's argument or the model's cached state, read on the GUI thread).

`ClientStatus` (singleton): the block height, sync, connections, staking, warnings and
scraper state.

| Name | Kind | Meaning |
|---|---|---|
| `marker` | property, constant | As for `Shell`. |
| `numBlocks`, `numBlocksPeers` | properties, int | The height and the peers' height. Lane: none (`numBlocksChanged`), seeded by the serial read at attach. |
| `blocksText` | property, string | The height, plus " of <peers> (<p>%)" while not in sync and behind the peers, as `OverviewPage::setHeight` shows it. |
| `inSync` | property, bool | `BitcoinGUI::setNumBlocks`'s rule: connected, a tip under 90 minutes old, and the peers' height reached. Lane: serial (the status read). |
| `syncState` | property, enum `SyncState` | `NoConnections`, `CatchingUp` or `UpToDate`, by the same rule. Lane: serial. |
| `syncProgress` | property, int | 0, 25, 50, 75 or 90: the largest not above the height's percentage of the peers' height. |
| `syncToolTip` | property, string | `setNumBlocks`'s tooltip (rich text). Lane: serial. |
| `numConnections`, `connectionLevel`, `connectionToolTip` | properties | The connection count, its icon level (0 to 4, as `setNumConnections`) and its tooltip. Lane: none (`numConnectionsChanged`) and serial (the status read). |
| `difficulty`, `networkWeight`, `coinWeight` | properties, double | Not amounts. Lane: none (`difficultyChanged`, `minerStatusChanged`), seeded by the serial read at attach. |
| `coinWeightText` | property, string | The coin weight with no decimals, "#.##" while masked. |
| `stakingState` | property, enum `StakingState` | `Staking`, `NotStaking` or `UnableToStake`: `updateStakingIcon`'s three arms. Lane: none (`minerStatusChanged`), seeded by the serial read at attach. |
| `stakingToolTip` | property, string | `updateStakingIcon`'s tooltip (rich text, the miner warnings escaped), formatted again at each status read and at each change of the mask. While values are masked the coin weight and the estimated staking frequency in it read "#.##". |
| `statusBarWarnings`, `minerWarnings` | properties, string | `ClientModel::getStatusBarWarnings` and `getMinerWarnings`, at every status read whatever the sync state; shown as plain text. Lane: serial. |
| `isTestNet` | property, bool | Lane: serial (the read at attach). |
| `scraperState` | property, enum `ScraperState` | `Waiting`, `Inactive`, `Downloading`, `Converged` or `NoConvergence`: `updateScraperIcon`'s arms. Lane: pooled (the scraper snapshot). |
| `scraperToolTip` | property, string | `updateScraperIcon`'s tooltip (rich text, names escaped). The read at attach starts it with the waiting tooltip when the node has had no scraper event, as `BitcoinGUI::setClientModel` does, or when its last event is an out-of-sync one. Empty until that read is delivered, and after it while the node's last event has no arm of `updateScraperIcon`, until an event that has one. Lane: pooled. |
| `blocksChanged`, `syncChanged`, `connectionsChanged`, `weightsChanged`, `stakingChanged`, `warningsChanged`, `networkChanged`, `scraperChanged` | signals | The matching group of properties changed. |

`WalletSummary` (singleton): the balances, the display unit, the mask state and the wallet's
lock state.

| Name | Kind | Meaning |
|---|---|---|
| `marker` | property, constant | As for `Shell`. |
| `balance`, `stake`, `unconfirmedBalance`, `immatureBalance`, `total` | properties, qint64 halfords | The four balances and their sum (the total includes the immature balance). Lane: none (`balanceChanged`), seeded by a serial read at attach. |
| `balanceText`, `stakeText`, `unconfirmedBalanceText`, `immatureBalanceText`, `totalText` | properties, string | Each amount in the display unit, masked while `maskValues` holds, as `OverviewPage::setBalance` formats it. |
| `balanceRoundedText` | property, string | The overview header's rounded balance. |
| `immatureVisible` | property, bool | The immature balance is not zero. |
| `displayUnit` | property, enum `Unit` | `GRC`, `MilliGRC` or `MicroGRC`, from the options model. |
| `displayUnitName` | property, string | The unit's name. |
| `maskValues` | property, bool | The options model's mask state. |
| `setMaskValues(masked)` | invokable | Sets the options model's mask state. |
| `encryptionStatus` | property, enum `EncryptionStatus` | `Unencrypted`, `Locked`, `Unlocked` or `UnlockedForStakingOnly`. Lane: none (`encryptionStatusChanged`), seeded by a serial read at attach. |
| `encryptionToolTip` | property, string | `BitcoinGUI::setEncryptionStatus`'s tooltip (rich text). |
| `encryptionStatusKnown` | property, bool | False until the lock state has been read or signalled. |
| `copyAmount(field)` | invokable, enum `AmountField` | Copies `Balance`, `Stake`, `Unconfirmed`, `Immature` or `Total` in the display unit through `Clipboard`; false while masked or unattached. |
| `balancesChanged`, `displayUnitChanged`, `maskValuesChanged`, `encryptionStatusChanged`, `encryptionStatusKnownChanged` | signals | The matching properties changed. |

`ResearcherSummary` (singleton): the researcher and beacon state. Lane: none for every name:
the values come from `ResearcherModel`'s cached snapshot, and the mask state and unit from
the options model.

| Name | Kind | Meaning |
|---|---|---|
| `marker` | property, constant | As for `Shell`. |
| `mode` | property, enum `Mode` | `StakingOnly`, `Pool` or `Solo`. |
| `modeText` | property, string | "CPID", "Pool" or "Staking Only", as `OverviewPage::updateResearcherStatus` labels the modes. |
| `cpid`, `magnitudeText` | properties, string | Masked with `ResearcherModel`'s mask strings; the magnitude reads "..." while out of sync. |
| `showMagnitude` | property, bool | Solo mode. |
| `statusText` | property, string | The researcher status. |
| `accrualText` | property, string | The pending accrual in the display unit, masked; "..." while out of sync. |
| `accrualNearLimit` | property, bool | The accrual is near its limit. |
| `beaconVisible`, `beaconActive`, `beaconToolTip` | properties | The beacon icon: hidden for a non-cruncher and in pool mode; its tooltip (rich text, values escaped) is refreshed every 60 s for the beacon age. |
| `summaryChanged` | signal | Any property changed. |

`RecentTransactions` (singleton): the overview's recent transaction list, a list model over
the `VIEW_OVERVIEW` cursor.

| Name | Kind | Meaning |
|---|---|---|
| `marker` | property, constant | As for `Shell`. |
| `iconSource` | role, url string | The transaction type's icon. |
| `amount` | role, qint64 halfords | The net amount. |
| `amountText` | role, string | The net amount in the display unit with the unit's name. |
| `negative` | role, bool | The amount is below zero. |
| `address` | role, string | The record's address, not the address-book label. |
| `date` | role, date | The transaction time. |
| `confirmed` | role, bool | The record counts for the balance. |
| `masked` | property, bool | The options model masks values; the list is hidden. |
| `setVisibleRows(n)` | invokable | The list shows `n` rows: `OverviewPage::updateTransactions`' stairstep sets the served-row limit. Lane: serial (`setViewLimit`). |
| `maskedChanged` | signal | `masked` changed. |

These seven roles are the only ones: the model reads no address-book label. Its other calls
on the transaction source run on the GUI thread (section 4).

`VotingSummary` (singleton): the overview's current poll title.

| Name | Kind | Meaning |
|---|---|---|
| `marker` | property, constant | As for `Shell`. |
| `currentPollTitle` | property, string | `VotingModel::getCurrentPollTitle`, read at attach, on each new poll and every 60 s; shown as plain text. Lane: serial. |
| `currentPollTitleChanged` | signal | `currentPollTitle` changed. |

`AboutInfo` (singleton): the About window's version, copyright and version-information
check.

| Name | Kind | Meaning |
|---|---|---|
| `marker` | property, constant | As for `Shell`. |
| `fullVersion` | property, string | The node's client version. Lane: serial (the read at attach). |
| `copyrightText` | property, constant | `AboutDialog`'s copyright line. |
| `updateCheckAvailable` | property, bool | Mainnet, and neither this process's `-disableupdatecheck` nor the node's `disableupdatecheck` setting is set. |
| `updateCheckDisabledReason` | property, string | Empty when available; else `AboutDialog`'s configuration or mainnet text. |
| `updateCheckRunning`, `pendingRequestId` | properties, bool and quint64 | A check is running, and its request id (0 when idle). |
| `checkForUpdate()` | invokable | Starts a check and returns its request id, or 0 when unattached, unavailable or already running. Lane: pooled. |
| `refreshUpdateCheck()` | invokable | Re-reads this process's flag at once and the node's setting. Lane: serial. |
| `updateCheckFinished(id, version, details, upgradeType)` | signal | A check finished; `upgradeType` is an `UpgradeType` value (`Unknown`, `Leisure`, `Mandatory`, `Unsupported`). An empty result reads as the configuration reason when the node's setting is set, else as no response. |
| `fullVersionChanged`, `updateCheckAvailableChanged`, `updateCheckRunningChanged` | signals | The matching properties changed. |

`Clipboard` (singleton): the system clipboard for QML. Lane: none; it has no attach.

| Name | Kind | Meaning |
|---|---|---|
| `marker` | property, constant | As for `Shell`. |
| `setText(text)` | invokable | Writes `text` without its thin spaces (U+2009), the digit-group separators of a formatted amount. |
| `text()` | invokable | The clipboard's text. |

`WalletOps` (singleton): the wallet-operation gate. One wallet operation at a time across the module, through the
prompt latch the gate takes; its `Security` owner is for the wallet's other passphrase prompts, which take it in a
later change. Lane: serial, for every read and wallet call below.

| Name | Kind | Meaning |
|---|---|---|
| `marker` | property, constant | As for `Shell`. |
| `busy` | property, bool | A request of the gate holds the shared prompt latch. |
| `Mode` | enum | `Unlock` (the wallet is locked) or `Elevate` (it is unlocked for staking only), from one lock-state read when the request begins. |
| `Failure` | enum | `WrongPassphrase`, `WrongPassphraseNul` (the passphrase holds a zero byte) or `ExpiredDuringPrompt` (an elevation found the wallet locked again). |
| `Result` | enum | `Completed` or `Cancelled`. |
| `unlockRequested(id, mode, title, text)` | signal | The request `id` needs the passphrase. The title and text are `AskPassphraseDialog`'s. |
| `answerUnlock(id, passphrase)` | invokable | Answers the request with a passphrase. Returns false, and makes no call, unless `id` is waiting for that answer. The passphrase becomes a `SecureString` at once and is kept nowhere. |
| `cancelUnlock(id)` | invokable | Cancels the request, whether it is waiting or its answer is running. Returns false unless `id` is open. |
| `unlockFailed(id, reason, text)` | signal | The answer failed; the request waits for another answer. |
| `unlockAccepted(id)` | signal | The wallet is unlocked, or widened, for the request's operation. |
| `finished(id, result)` | signal | The request ended. |

`SendCoins` (singleton): the send page. An amount is halfords plus a text in the display unit; the confirmation, fee
and result texts are those of the Widgets send dialog.

| Name | Kind | Meaning |
|---|---|---|
| `marker` | property, constant | As for `Shell`. |
| `recipients` | property, list model | The rows, always at least one. Roles: `recipient`, `label`, `amount` (halfords), `amountText`, `message`, `subtractFee`, `addressValid`, `amountValid`. |
| `busy` | property, bool | True from a non-zero `send()` until its `finished` (or, when a disconnect ended the send, until detach). Meanwhile `send()` returns 0, every row edit is refused, and a URI waits in the core bridge. |
| `messageAllowed` | property, bool | Whether a transaction message can be sent at the next block (`-messagecontractdisableheight`); true until the first block height arrives. Only the first row's message is sent. Lane: none (`numBlocksChanged`). |
| `unitName` | property, string | The display unit's name. |
| `Outcome` | enum | `Sent`, `Aborted`, `Failed`, or `Busy` (the gate refused to begin: the prompt latch is held, or the runner, closed at quit, refused the read). |
| `addRecipient()`, `removeRecipient(row)`, `clearRecipients()` | invokables | Row edits; false when refused (a send in progress, unattached, or the last row). |
| `updateRecipient(row, map)` | invokable | Sets any of `recipient`, `label`, `amount` (text in the display unit), `message` and `subtractFee`; false when refused. |
| `send()` | invokable | Validates every row and marks the invalid ones; returns 0 if any is invalid or a send is in progress, else copies the rows and returns the request id. |
| `sendConfirmationRequested(id, title, text)` | signal | The send confirmation; the text is rich text, with each label escaped. |
| `confirmSend(id, yes)` | invokable | Answers the confirmation. Yes asks `WalletOps` for the wallet, which may ask for the passphrase. |
| `feeConfirmationRequested(id, fee, title, text)` | signal | The wallet needs a fee (halfords) above the one accepted so far. The wallet stays unlocked while the question is open. |
| `confirmFee(id, yes)` | invokable | Answers the fee question; yes sends again with that fee accepted. At most ten fees are asked per send. |
| `finished(id, outcome, title, message, txid)` | signal | The request ended. The message is empty for `Sent` and `Aborted`; `txid` is set for `Sent`. |
| `acceptUri(url)` | invokable | Hands a dropped URL to the core bridge, the path a received URI takes. |
| `claimPendingUri()` | invokable | Takes the bridge's pending URI unless a send is in progress: a valid one fills the page and emits `uriAccepted()`, an invalid one emits `uriRejected(title, text)`. A finished send claims a URI that waited for it. |
| `outOfSyncTitle()`, `outOfSyncText()` | invokables | The Widgets warning shown before the send page while the wallet is out of sync. |

### `Shell` (singleton)

| Name | Kind | Meaning |
|---|---|---|
| `marker` | property, constant | Identifies the provider that owns the instance; used by the identity tests. |
| `shown` | property | True once the composition root asked the root to show its main window (`showMain`). |
| `startMinimized` | property | True when the main window was asked to show minimized (`-min`). |
| `quitRequested` | property | True once a quit was requested; from then on `requestClose()` returns true, so the main window's close handler accepts every close. |
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
| `requestClose()` | invokable | For the main window's close handler. Returns true when the window may close and the application quits; false when it was hidden to the tray instead (`hideToTrayRequested`). A hidden window with no tray could not be restored, so there is no hide without a tray. A later change may answer false and ask QML to confirm first, for confirm-on-close, quitting once QML answers. |
| `shownChanged`, `startMinimizedChanged`, `quitRequestedChanged`, `ipcInfoChanged` | signals | The matching property changed. |
| `coreMessage(caption, message, modal)` | signal | A core message, delivered queued: the thread that raised it never waits for QML. |
| `updateAvailable(caption, version, updateVersion, message)` | signal | The core found a newer version. |
| `buildMismatchWarning(guiCommit, nodeCommit)` | signal | The mixed-build banner. |
| `showMainRequested(minimized)` | signal | The root should show its main window. |
| `hideMainRequested()` | signal | The first step of the detach: the root hides every window it shows, not only the main window. Qt's quit closes none of them, and the GUI thread pumps no events for the rest of the teardown. `QmlFrontEnd::hideMain()` then hides any top-level window still visible. |
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

### `Fonts` (singleton)

| Name | Kind | Meaning |
|---|---|---|
| `uiFamily` | property, constant | The UI font family: the platform's general system font family on macOS and iOS, "Inter" elsewhere. |

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
`_mrcModel`. This table maps 47 of their members to `Gridcoin.Qml` (no carried file binds
`_walletModel.numTransactions`, so it has no row). "Planned" names the
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
| `_clientModel` | `numBlocks` | `ClientStatus.numBlocks` | |
| `_clientModel` | `numBlocksPeers` | `ClientStatus.numBlocksPeers` | |
| `_clientModel` | `difficulty` | `ClientStatus.difficulty` | |
| `_clientModel` | `networkWeight` | `ClientStatus.networkWeight` | |
| `_clientModel` | `coinWeight` | `ClientStatus.coinWeight` | `ClientStatus.coinWeightText` is the masked text. |
| `_clientModel` | `inSync` | `ClientStatus.inSync` | Computed by the module's own sync rule. |
| `_clientModel` | `isTestNet` | `ClientStatus.isTestNet` | |
| `_clientModel` | `statusBarWarnings` | `ClientStatus.statusBarWarnings` | |
| `_clientModel` | `minerWarnings` | `ClientStatus.minerWarnings` | |
| `_clientModel` | `fullVersion` | `AboutInfo.fullVersion` | |
| `_walletModel` | `balance` | `WalletSummary.balance`, `balanceText` | Halfords plus a formatted string, not a double. |
| `_walletModel` | `stake` | `WalletSummary.stake`, `stakeText` | Halfords plus a formatted string, not a double. |
| `_walletModel` | `unconfirmedBalance` | `WalletSummary.unconfirmedBalance`, `unconfirmedBalanceText` | Halfords plus a formatted string, not a double. |
| `_walletModel` | `immatureBalance` | `WalletSummary.immatureBalance`, `immatureBalanceText` | Halfords plus a formatted string, not a double. |
| `_walletModel` | `encryptionStatus` | `WalletSummary.encryptionStatus` | An enum. |
| `_walletModel` | `transactionTableModel` | `RecentTransactions`, for the overview | The overview list is `OverviewTxModel`'s cursor, not the history table; the history model is planned. |
| `_walletModel` | `addressTableModel` | planned address-book models | |
| `_walletModel` | `receiveAddressTableModel` | planned address-book models | |
| `_walletModel` | `unlockWallet(QString)` | `WalletOps.answerUnlock(id, passphrase)` | |
| `_walletModel` | `cancelUnlock()` | `WalletOps.cancelUnlock(id)` | |
| `_walletModel` | `requireUnlock()` | `WalletOps.unlockRequested(id, mode, title, text)` | |
| `_walletModel` | `askFeeResult(bool)` | `SendCoins.confirmFee(id, yes)` | |
| `_walletModel` | `askFeeDialog(int64_t)` | `SendCoins.feeConfirmationRequested(id, fee, title, text)` | The fee in halfords. |
| `_researcherModel` | `magnitude` | `ResearcherSummary.magnitudeText` | |
| `_researcherModel` | `accrual` | `ResearcherSummary.accrualText` | |
| `_researcherModel` | `cpid` | `ResearcherSummary.cpid` | |
| `_researcherModel` | `status` | `ResearcherSummary.statusText` | |
| `_researcherModel` | `researcherMode` | `ResearcherSummary.mode` | An enum, not 0, 1 or 2. |
| `_votingModel` | `currentPollTitle` | `VotingSummary.currentPollTitle` | |
| `_sendCoinsController` | `recipients` | `SendCoins.recipients` | Typed recipients, amounts in halfords. |
| `_sendCoinsController` | `addRecipient()` | `SendCoins.addRecipient()` | |
| `_sendCoinsController` | `removeRecipient(int)` | `SendCoins.removeRecipient(row)` | |
| `_sendCoinsController` | `updateRecipient(int, map)` | `SendCoins.updateRecipient(row, map)` | |
| `_sendCoinsController` | `clearRecipients()` | `SendCoins.clearRecipients()` | |
| `_sendCoinsController` | `sendCoins()` | `SendCoins.send()` | The confirmation, the passphrase and the fee are asked through signals; the result arrives through `SendCoins.finished`. |
| `_sendCoinsController` | `coinsSentOrFailed(QString)` | `SendCoins.finished(id, outcome, title, message, txid)` | |
| `_nativeDialog` | `information()` | none | QML shows its own dialogs and answers through the request, signal and answer shape; once the front end exists the module never shows a blocking dialog (section 5). |
| `_nativeDialog` | `warning()` | none | As above. |
| `_nativeDialog` | `question()` | none | As above. |

`_mrcModel`: no member is used by the QML front end; the module's MRC adapter is planned.

## 4. Synchronous reads the module inherits

These are not made asynchronous by the module. Inherited from the models:

- `ClientModel::updateTimer` polls the node interface on the GUI thread on a timer.
- `WalletModel::drainEventQueue` drains wallet events on the GUI thread and calls into the
  wallet interface.
- `ClientModel::updateNumBlocks` (`getBlockDifficulty`, `tryGetNumBlocksOfPeers`,
  `tryGetNetworkWeight`), `updateAlert` (`getAlertStatusBarMessage`) and
  `updateMinerStatus` (`tryGetEstimatedTimeToStake`).
- `WalletModel::updateStatus` (`getLockState`), its constructor and
  `reloadTransactionView` (`txSource().prime(...)`).
- `ResearcherModel::refresh` and `onResearcherChanged` (the researcher snapshot).
- `MRCModel::refresh`, on every `numBlocksChanged`.
- `VotingModel::handleNewPoll` (`inInitialBlockDownload`).

Added by `RecentTransactions` (through `OverviewTxModel`):

- `registerView` and `getRows` at attach.
- `getRows` on a drained `VIEW_OVERVIEW` Reset newer than the seed, which is new to the QML
  build.
- `unregisterView` at `detachWallet`.

`setViewLimit` runs on the serial lane. Under `-multiprocess` each call above is an IPC
round trip on the GUI thread.

## 5. Request, signal, answer

Every prompt follows one shape. QML calls a `Q_INVOKABLE` request that returns at once with
a request id. The module emits a signal saying what it needs. QML answers through a second
`Q_INVOKABLE`. The module then emits `finished(id, result)`, or a failure signal that keeps
the request open. There is no nested event loop and no blocking connection anywhere in the
module but one: `gridcoinresearch-qml`'s start-up error box in `qml_main.cpp`, shown from
the factory before `GuiMain` opens its log, connects to a daemon or starts the core, as
`GuiMain`'s own start-up error boxes are. Besides the synchronous calls section 4 lists, the
module has two waits on the GUI thread by design, both at teardown in
`GuiFrontEnd::detachModels`: `QmlCallRunner::drain()`, where teardown waits for an in-flight
serial call, and `WalletOpGate::detach()` (the C++ side of `WalletOps`), which
`QmlFrontEnd::detachWallet` calls after that drain to restore a wallet still unlocked for a
wallet operation. That restore runs on the GUI thread (an IPC round trip under
`-multiprocess`). Narrowing the wallet back to staking only takes the wallet's lock
(`CWallet::RestrictToStakingOnly`), so it waits there for any other holder of that lock to
finish: a send in flight, the staker, or a rescan, which holds the lock for the whole scan.
The module's pooled calls are not waited for in the module: the composition root reaps the
global `QThreadPool` on the GUI thread after `detachModels` (`FrontEndDetachGuard`), on
every exit path. `test/lint/lint-qml-module.sh` is a tripwire for the common blocking
constructs in the rest, not a proof.

The wallet-operation gate is the worked example. `SendCoins.confirmSend(id, true)` asks `WalletOps` for the wallet.
The gate decides the mode from one lock-state read on the serial lane; a locked wallet, or one unlocked for staking
only, emits
`WalletOps.unlockRequested(id, mode, title, text)`, and the unlock dialog answers with
`WalletOps.answerUnlock(id, passphrase)` or `WalletOps.cancelUnlock(id)`. A wrong passphrase emits `unlockFailed`, and
the request waits for another answer. With the wallet unlocked, each send attempt runs on the serial lane; a fee above
the accepted one emits `SendCoins.feeConfirmationRequested`, which `SendCoins.confirmFee` answers while the wallet stays
unlocked; the result is `SendCoins.finished`. The gate takes the prompt latch, which the wallet's other
passphrase prompts take too in a later change, so one wallet operation runs at a time across the module: a send asked
for while the latch is held, or whose read the runner refuses after its close at quit, finishes at once with `Busy`.

## 6. Lanes

`QmlCallRunner`'s serial lane (one thread, in order) runs every call not named here. The
pooled lane (the global `QThreadPool`) runs calls whose results may arrive in any order.
Planned, not in the module yet: `runDiagnostics`. `AboutInfo.checkForUpdate()` runs
`checkForLatestUpdate`, and `ClientStatus` reads the scraper snapshot, on the pooled lane.
The RPC console's commands will run in the order they were
typed, on an ordered lane, as the Widgets console runs them: a `walletpassphrase` must
finish before the command typed after it starts. The pooled lane keeps no order, so the
console never uses it.

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

Serial closures. A serial closure captures `interfaces::` references taken at post time (`Node&` from `ClientModel::node()`, `Wallet&` from `WalletModel::wallet()`, `WalletTxSource&` from `WalletModel::txSource()`, `StakingStatus&` from `ClientModel::stakingStatus()`), never an adapter pointer; it also captures the result holder its `deliver` reads. A closure that calls an audited model method (one whose only effects are calls on the model's own interface references, loads of its `std::atomic` `m_cached_*` fields, an atomic store of a node read into such a field while it still holds -1, and computation on the values it reads, including `QString` formatting and the model's static `tr`; it touches no other model state) also captures that model by reference, taken at post time from the model the adapter received at `attach()`, and calls no method on it directly that the list below does not name; the methods an entry calls itself are part of that entry's audit. A pooled closure captures `interfaces::` references and the result holder its `deliver` reads (a `std::shared_ptr` to a plain value struct), never an adapter or model pointer. The `deliver` function runs on the GUI thread and may use the adapter, which the provider owns past the runner.

The audited model methods, each with what it calls and stores:

- `VotingModel::getCurrentPollTitle`: calls `VotingManager::currentPollTitle()`; cuts the title to 80 characters and maps '_' to ' '; stores nothing.
- `ClientModel::getNumConnections`: calls `Node::getNodeCount()`; stores nothing.
- `ClientModel::getLastBlockDate`: loads `m_cached_best_block_time`; while it holds -1, calls `Node::getLastBlockTime()`, and when that returns 0 returns the genesis block's time; stores nothing.
- `ClientModel::getStatusBarWarnings`: calls `Node::getWarnings()` and `StakingStatus::isStaking()`; calls the model's own `getDifficulty` (which loads `m_cached_difficulty` and, while it holds -1, calls `Node::getDifficulty()` and stores the result there) and `getMinerWarnings`; formats with the model's static `tr`.
- `ClientModel::getMinerWarnings`: calls `StakingStatus::getErrors()`; stores nothing.
- `ClientModel::getNetWeight`: loads `m_cached_net_weight`; while it holds -1, calls `StakingStatus::getNetworkWeight()` and stores the result divided by 80 there.
- `WalletModel::sendCoins`: the one audited method that changes wallet state, called by `SendCoins`' send attempt. It checks the recipients it is given with the model's own `validateAddress` (`DecodeDestination` and `IsValidDestination`), calls `Wallet::sendCoins()`, and maps the result to a `SendCoinsReturn`; stores nothing. The serial lane is drained before `detachWallet`, so the model outlives the call.

A lazy fill of a cache still at -1 runs on the serial thread too, so once per process, at start-up, it can race with `ClientModel::updateNumBlocks` storing a value on the GUI thread; each store is one atomic store, and the value kept may be older or newer than the one it replaced.

Coalescing. Each adapter has at most one read of a kind in flight; a trigger that arrives meanwhile is remembered, and the read's delivery posts it again once. This holds for the scraper snapshot on the pooled lane, for the status, poll-title and limit reads on the serial lane, and for `AboutInfo.refreshUpdateCheck()`'s node read. A read's in-flight state is cleared only by its delivery, or by a new attach, whose earlier reads never deliver.

The update check. `AboutInfo.checkForUpdate()` runs `checkForLatestUpdate` on the pooled lane. When it returns no version, the same closure re-reads the node's `disableupdatecheck` setting, so a setting changed at run time is reported as the configuration reason, not as a network failure. Each About open calls `AboutInfo.refreshUpdateCheck()`, which re-reads this process's `-disableupdatecheck` at once and the node's setting on the serial lane.

Constraints on later steps. No caller may call `QmlCallRunner::bumpGeneration` between `attachModels` and `detachModels`: a bump drops the deliveries of reads in flight, and an adapter whose delivery was dropped keeps its read in flight for the rest of the session. No step may block the serial thread waiting for a user answer: a prompt happens between serial calls.

The wallet-operation gate. `WalletOps` runs every wallet call of a wallet operation on the serial lane: its lock-state read, the unlock or elevation, the operation's own calls and the restore. No serial call waits for the user: each prompt is emitted from the delivery of a call that has returned, and the answer posts the next call. Across a fee question the wallet stays unlocked and no serial call is in flight. An unlock is held by a lease, which restores the wallet (relocks it, or narrows it back to staking only) exactly once, whichever comes first: on the serial lane after the operation's last call; on the serial lane before an exception thrown by an operation under the lease is classified; on the serial lane when the request is cancelled while its answer runs; or, for a lease still held at quit, synchronously on the GUI thread in `detachWallet`, after the event loop has ended. A release that a moved generation dropped is posted again by the next request, before its own read; one that the runner's close or drain at quit dropped or refused is made in `detachWallet`, since a closed runner refuses every later post. The gate's serial closures capture the `Wallet&` and the request's lease (a `std::shared_ptr` holding the `Wallet&` and the restore flags), never the gate. The rule above on `QmlCallRunner::bumpGeneration` covers the gate, since `WalletOps.busy` implies attached: for the gate, a dropped delivery leaves the shared prompt latch held until detach (so the other passphrase prompts, once they take it, are refused too), and a dropped release is posted again by the next request.

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
shutdown) is still recorded. The root shows core messages as described below. For an init
error that ends start-up, the log and stderr remain the only reliable record until a later
change adds its dialog, because start-up returns before the event loop runs. The Widgets
front end still holds the raising thread until its dialog closes.

The root, `WindowManager.qml`, shows each core message in a window of its own, a
`CoreMessageWindow`, created when the message is shown and destroyed once it is dismissed:

- Messages wait in one queue, in arrival order, modal or not. One is shown at a time: the
  root's `currentMessage` holds the message on screen, and a message that arrives while it
  is open is shown after the user closes it. Nothing waits on the queue: it decides only
  what the user sees, and when.
- The next message is shown from a `Qt.callLater` call, never inside the closing window's
  close handler, so its window is created only after the previous window's close has
  finished. `Qt.callLater` runs a close and the arrivals of one turn of the event loop as
  one call, which shows at most one message.
- A modal message's window is application-modal and a non-modal message's window is not;
  each window takes its modality before it is shown.
- Once a quit is requested, the queued messages are cleared and not shown; each is already
  in the GUI log.
- Who logs what: the front end writes each modal message to the GUI log and stderr when it
  is posted, and the root writes each non-modal message to the GUI log when it is delivered,
  so every message the root receives reaches the GUI log once, whether or not a window shows
  it. A non-modal message posted after the event loop has returned, before teardown clears
  the main window, is never delivered and is not logged, as before.
- Under `QApplication`, `QCoreApplication::quit()` and a quit the platform posts close only
  top-level widgets: no quit closes a QML window, so the root's close handler sees only the
  closes of its own windows.

## 8. The carried tree

- Source: `src/qt/qml`, carried from the `qml-reset` branch of `ZiggySchulz/Gridcoin-Research`
  at `e66826761` (his six-commit rework of the `qml` branch at `3ca8d949f`, with the same
  tree), with his commits replayed under his authorship.
- Modules: `Gridcoin.App` (target `gridcoinqmlapp`), `MMPTheme` (target `gridcoinqmltheme`)
  and `CurrentTime` (target `gridcoinqmltime`). The `qml_assets` resource holds the icons at
  `qrc:/icons/...` and the fonts under `:/fonts/`. The tab menu's icons are under
  `qrc:/icons/tabmenu/`, because the Widgets resources already serve a file at `:/icons/menu`.
- The root is `qrc:/qt/qml/Gridcoin/App/WindowManager.qml`, which `gridcoinresearch-qml`
  loads.
- Fonts: `Fonts.uiFamily` is the system font family on macOS and iOS and Inter elsewhere, and
  `LoadQmlFonts` registers the bundled fonts. `MMPTheme` takes its font family from it.
- The root and its shell (`WindowManager.qml`, `SplashScreen.qml`, `MainWindow.qml` and
  `TabMenu.qml`), the overview (`OverviewView.qml` and `RecentTransactionItem.qml`), the
  status footer (`StatusFooter.qml`), the About window (`AboutWindow.qml`), the send page
  (`SendView.qml`), the unlock dialog (`UnlockDialog.qml`) and the new
  `BuildMismatchBanner.qml`, `VersionInfoWindow.qml`, `WalletPromptDialog.qml` and
  `WalletPrompts.qml` bind `Gridcoin.Qml` or no context property at all. The other carried
  screens still bind the context properties of the branch they came from.
  `test/lint/lint-qml-module.sh` holds the list of files that may (`EditAddressDialog.qml`
  and `ReceiveView.qml`); a file is
  removed from it when it binds to `Gridcoin.Qml` names, a listed file that binds none fails
  the lint, and the list is to be empty before the QML front end becomes the default.
- Window state: Qt 6.7 and later defer a QML assignment to `visible` or `visibility` on a
  window whose QObject parent is the root `Item`, which is in no window itself; a declared
  `visible: true` and such an initial property are deferred alike. So the root drives its
  windows only through the `QWindow` methods: `show()`, `showNormal()`, `showMinimized()`,
  `showMaximized()`, `setVisible()` and `hide()`. `test/lint/lint-qml-module.sh` enforces
  this for `WindowManager.qml`; a later change that writes a window's state from another
  file follows the same rule.
- Qt versions: the carried tree loads on Qt 6.4 or later with the Qt5Compat
  GraphicalEffects module (section 1). Without it `gridcoinresearch-qml` stops at start with
  a message naming the module (section 1). A root that compiles but cannot be created ends
  the GUI with a "could not start" message box and EXIT_FAILURE, and the GUI log (debug.log,
  or debug_gui.log under `-multiprocess`) holds the QML errors. Three files import the
  module: `ColorizableImage.qml`
  tints the tab menu's icons with its `ColorOverlay`, and `InfoPopup.qml` and
  `MMPControls/GroupBox.qml` draw their shadows with `DropShadow`. Under Qt Quick's software
  adaptation the tinted icons and the shadows draw nothing.
- The translation scan skips the carried tree for now.
