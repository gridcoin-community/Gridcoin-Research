# Gridcoin multiprocess on Windows — core task scripts

PowerShell scripts to run the split **multiprocess** Gridcoin core as a background
Scheduled Task on Windows, stop it gracefully, upgrade a locked exe, and launch the
GUI. See [`doc/multiprocess.md`](../../doc/multiprocess.md) for the split-mode model.

| Script | Purpose |
|---|---|
| `Install-GridcoinCoreTask.ps1` | Register the **Core-Start** (boot) and **Core-Stop** (graceful) tasks, run-as the wallet user. |
| `Uninstall-GridcoinCoreTask.ps1` | Remove them. |
| `Update-GridcoinCore.ps1` | Graceful upgrade engine for the locked-exe problem. |
| `Start-GridcoinGui.ps1` | Launch the `-multiprocess` GUI (or drop a Start-Menu shortcut with `-CreateShortcut`). |

Opt-in **autounlock** (DPAPI, stake-only) is separate — `Set-GridcoinAutounlock.ps1` (future).

## Quick start

```powershell
# From the install directory (…\GridcoinResearch\windows\). Run-as the account that
# owns the wallet; it will prompt for that account's Windows password.
.\Install-GridcoinCoreTask.ps1 -DataDir "$env:APPDATA\GridcoinResearch"

Start-ScheduledTask -TaskPath '\Gridcoin\' -TaskName 'Core-Start'   # start the core now
.\Start-GridcoinGui.ps1                                             # attach the GUI
```

The core then autostarts on every boot. To upgrade:

```powershell
.\Update-GridcoinCore.ps1 -NewBinariesDir C:\path\to\new\bin   # graceful stop -> replace -> restart
```

## Two things to know

**Run-as the wallet user, never SYSTEM.** The IPC socket (`node.sock`) and cookie
(`ipc.cookie`) are created **owner-only** in the datadir, and a per-connection
peer-UID check enforces same-user. A SYSTEM-owned core would lock the wallet user's
own GUI out. This also means **only that Windows user can attach a GUI** — a
*different* user running `gridcoinresearch.exe -multiprocess` gets *"could not
connect to the daemon"*. **That is the access control working, not a bug.**

**Graceful stop = start the Core-Stop task.** To stop the core, *start* the
`Core-Stop` task (`Start-ScheduledTask … Core-Stop`) — it runs `gridcoinresearchd
… stop`, an RPC stop that flushes the databases. **Never `Stop-ScheduledTask` the
`Core-Start` task**: Task Scheduler's stop is a hard `TerminateProcess` with no
flush, which risks database corruption. Shutdown, upgrade, and manual stop all go
through the one `Core-Stop` task.

## Graceful stop on OS shutdown

The `Core-Stop` task carries a best-effort OS-shutdown trigger (Kernel-General
EventID 13), but **a scheduled task cannot reliably flush before shutdown**: Windows
does not wait for an event-triggered task to finish, and the services it depends on
are being torn down in that window, so the RPC `stop` will often be cut short
mid-flush. For a **reliable** clean shutdown, configure a Group Policy **Computer →
Shutdown** script — Windows runs shutdown scripts synchronously and waits for them
(bounded by *Maximum wait time for Group Policy scripts*, default 600 s):

1. `gpedit.msc` → Computer Configuration → Windows Settings → Scripts → **Shutdown**.
2. Add a script that starts the stop task and waits for the core to exit — e.g. a
   `.cmd` running `schtasks /Run /TN "\Gridcoin\Core-Stop"` followed by a short loop
   on `tasklist | findstr gridcoinresearchd` until it is gone.

Confirming the exact wait-for-flush behavior on real hardware is the top item on the
validation list.

## Crash recovery

`Core-Start` is registered with restart-on-failure (3 tries, 1 minute apart) — the
analogue of the Linux unit's `Restart=on-failure`. A *graceful* stop exits 0 and is
not restarted; only a crash (non-zero exit) triggers a restart.

## Status

New scripts, validated by PowerShell parse-check + inspection; full runtime
validation on Windows (install → reboot → autostart → upgrade → GUI attach →
shutdown-flush → different-user connection-refused) is pending on the test box.
Known items to confirm there: the shutdown-flush mechanism (above); that
stored-password task registration requires an **elevated** shell and the "Log on as
a batch job" right for the run-as account; and Microsoft/AzureAD-account principal
forms.
