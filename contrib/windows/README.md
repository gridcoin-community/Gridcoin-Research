# Gridcoin multiprocess on Windows — core task scripts

PowerShell scripts to run the split **multiprocess** Gridcoin core as a background
Scheduled Task on Windows, stop it gracefully, upgrade a locked exe, and launch the
GUI. See [`doc/multiprocess.md`](../../doc/multiprocess.md) for the split-mode model,
and [`doc/running-unattended.md`](../../doc/running-unattended.md) for the step-by-step
walkthrough.

The Windows installer **bundles these scripts** into `…\GridcoinResearch\windows\`; run
them from there in an **elevated** PowerShell. (In a source checkout they live here in
`contrib/windows/`.)

| Script | Purpose |
|---|---|
| `Install-GridcoinCoreTask.ps1` | Register the **Core-Start** (boot) and **Core-Stop** (graceful) tasks, run-as the wallet user. |
| `Uninstall-GridcoinCoreTask.ps1` | Remove them. |
| `Update-GridcoinCore.ps1` | Graceful upgrade engine for the locked-exe problem. |
| `Set-GridcoinShutdownFlush.ps1` | **Opt-in**: configure/remove a Group Policy shutdown script for a reliable DB flush on OS shutdown. |
| `Start-GridcoinGui.ps1` | Launch the `-multiprocess` GUI (or drop a Start-Menu shortcut with `-CreateShortcut`). |
| `Set-GridcoinAutounlock.ps1` | **Opt-in**: enable/remove stake-only autounlock (DPAPI secret + resident task). |
| `Invoke-GridcoinAutounlock.ps1` | The resident autounlock helper the task runs (not called directly). |

Autounlock is **opt-in** and independent of the core tasks — see [Autounlock](#autounlock-opt-in-stake-only-dpapi) below.

## Quick start

```powershell
# In an ELEVATED PowerShell. A fresh Windows install blocks .ps1 by default, so set the
# policy for this session and call the scripts directly -- do NOT wrap a call as
# `powershell.exe -ExecutionPolicy Bypass .\script -Arg "C:\Program Files\..."`, because
# the outer shell re-parses and splits the spaced path.
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
cd "C:\Program Files\GridcoinResearch\windows"

# Run-as the account that owns the wallet; it will prompt for that account's Windows password.
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
(`ipc.cookie`) are created **owner-only** in the datadir (NTFS ACL), so on Windows
only that user can open them. (The Linux `SO_PEERCRED`/peer-UID check has no Windows
equivalent — AF_UNIX on Windows exposes no peer credentials — so the ACL is the
enforcement here.) A SYSTEM-owned core would lock the wallet user's own GUI out.
This also means **only that Windows user can attach a GUI** — a *different* user
running `gridcoinresearch.exe -multiprocess` gets *"could not connect to the
daemon"*. **That is the access control working, not a bug.**

**Graceful stop = start the Core-Stop task.** To stop the core, *start* the
`Core-Stop` task (`Start-ScheduledTask … Core-Stop`) — it runs `gridcoinresearchd
… stop`, an RPC stop that flushes the databases. **Never `Stop-ScheduledTask` the
`Core-Start` task**: Task Scheduler's stop is a hard `TerminateProcess` with no
flush, which risks database corruption. Shutdown, upgrade, and manual stop all go
through the one `Core-Stop` task.

## Firewall (inbound P2P)

`Install-GridcoinCoreTask.ps1` adds an **inbound firewall rule for the daemon**
(`gridcoinresearchd.exe`). The headless core never triggers the interactive firewall
prompt the GUI gets on first run, and in multiprocess mode the **core**, not the GUI,
does the P2P — so without a rule Windows silently blocks inbound and the node is
outbound-only (it still syncs and stakes, just can't accept incoming peers). The rule is
program-based on the **Domain** and **Private** profiles (Public is excluded; add it with
`-FirewallProfile Domain,Private,Public`). Because it is program-based (any TCP to the
daemon), if you also set `rpcallowip` — which makes the RPC listener bind to all
interfaces instead of localhost — the RPC port becomes reachable through this rule too;
RPC stays credential-protected, but be aware. Pass `-SkipFirewallRule` to manage the
firewall yourself; `Uninstall-GridcoinCoreTask.ps1` removes the rule.

## Clean database flush on OS shutdown

`Core-Stop` carries **no** OS-shutdown trigger. A scheduled task can't make Windows wait,
so an event-triggered (EventID-13) stop is best-effort and gets cut short mid-flush —
confirmed on real hardware, where reboots produced no logged graceful shutdown (the core
was effectively hard-killed, and only recovered clean thanks to LevelDB's crash-resilience).
BDB (the wallet) is less forgiving of a mid-write kill, so for a **guaranteed** flush use a
Group Policy **Computer → Shutdown** script, which Windows runs **synchronously and waits
for** (bounded by *Maximum wait time for Group Policy scripts*, default 600 s).

Configure it (opt-in, elevated):

```powershell
.\Set-GridcoinShutdownFlush.ps1 -DataDir "$env:APPDATA\GridcoinResearch"   # -Remove to undo
```

It writes a small `.cmd` that runs the graceful RPC stop **as SYSTEM** (so it doesn't depend
on the wallet user's session still being loaded late in shutdown) and waits for the core to
exit, then wires it into the local machine shutdown scripts and runs `gpupdate`. **Validate
by rebooting once** and confirming a full `Shutdown: … Final flush of wallet database …`
sequence in `debug.log` just before the restart. (We deliberately don't *also* put a shutdown
trigger on `Core-Stop` — the two would double-stop and race.)

**Domains.** Local and domain-GPO shutdown scripts **stack** — all run at shutdown,
additively, bounded collectively by the max-wait-time. But if a domain GPO enables **"Turn
off Local Group Policy Objects processing,"** this *local* script is ignored; in that case
deploy the same `.cmd` (graceful RPC stop + wait-for-exit) via a **domain GPO** instead.

**Manual alternative** (if you'd rather not run the helper): `gpedit.msc` → Computer
Configuration → Windows Settings → Scripts → **Shutdown**, and add a `.cmd` that runs
`"…\daemon\gridcoinresearchd.exe" -datadir="…" stop` followed by a loop on
`tasklist | find /i "gridcoinresearchd.exe"` until it's gone.

## Crash recovery

`Core-Start` runs the core through a generated launcher (`core-autostart.cmd`) that
retries **once, after a 2-minute wait**, if the daemon exits non-zero — the analogue of
the Linux unit's `Restart=on-failure`. A *graceful* stop exits 0 and is not retried;
only a non-zero exit is. Each attempt's stderr is captured to
`%TEMP%\gridcoin-core-start.err.log` (deliberately NOT inside the data directory: cmd
evaluates the redirect before running the daemon, so writing there would force the
launcher to pre-create the data directory, and the daemon only applies its owner-only
DACL to a directory it creates itself).

The retry deliberately lives in the launcher rather than in the task: Task Scheduler's
own restart-on-failure does **not** fire on a program's non-zero exit (confirmed
on-device), so the `-RestartCount` it used to be registered with never actually helped.

## Autounlock (opt-in, stake-only, DPAPI)

Unattended **staking** on an encrypted wallet needs the wallet unlocked after every boot
or restart. This is opt-in and deliberately **external** to the wallet (the in-wallet
auto-unlock was removed years ago as insecure): the passphrase lives in the OS credential
store, and the wallet is unlocked over RPC **for staking only** — never for spending. It is
the Windows analogue of the Linux `gridcoinresearchd-autounlock.service` (systemd
`LoadCredentialEncrypted`); the security contract mirrors
[`contrib/wallettools/gridcoin_autounlock.py`](../wallettools/gridcoin_autounlock.py).

Enable it **as the wallet user, from an elevated console** (Run as administrator):

```powershell
.\Set-GridcoinAutounlock.ps1 -DataDir "$env:APPDATA\GridcoinResearch"
# Prompts: the WALLET passphrase (twice), then the account's Windows LOGIN password.
```

Run it **at the machine's own console, not over SSH/PSRemoting**: DPAPI CurrentUser encryption
needs an interactive or batch logon and fails over a network logon (no master key). Elevation is
needed because the resident task carries a boot trigger.

What it does:

- **Secret at rest = DPAPI (CurrentUser).** The passphrase is encrypted with
  `CryptProtectData` and written to `<datadir>\autounlock\passphrase.cred` — decryptable
  only by **that Windows account on that machine**, and additionally locked owner-only by
  NTFS ACL. It is never stored in the wallet, on a command line, or in a log. Because DPAPI
  CurrentUser is per-account, the task **must run as the same user that ran setup** — the
  script refuses a mismatched `-TaskUser`.
- **Registers `\Gridcoin\Autounlock`**, a resident task (run-as the wallet user) that runs
  `Invoke-GridcoinAutounlock.ps1`: it waits for the core, unlocks **stake-only**
  (`walletpassphrase … true`), and **self-heals** — it re-unlocks after each core restart.
- **Native listener-ownership gate.** Before any credential goes on the wire it verifies
  that our own SID owns every LISTEN socket on the RPC port (`Get-NetTCPConnection` →
  `Win32_Process` owner SID). The RPC port is unbound for minutes during core startup and an
  unprivileged loopback port can be squatted by a local account first; a **foreign or
  unverifiable owner makes it refuse** and log — that is the gate working, not a bug.

A running log (redacted — no secrets) is at `<datadir>\autounlock\autounlock.log`. To
remove everything:

```powershell
.\Set-GridcoinAutounlock.ps1 -Remove   # unregister the task and delete the DPAPI blob
```

Note: a `-17 already unlocked` in the log means the wallet was unlocked by another path
(e.g. a GUI unlock) and the **staking-only restriction was not re-asserted** — it may be
spendable until the next restart.

## Status

Validated by PowerShell parse-check + inspection; full runtime validation on Windows is
pending on the test box. Core tasks: install → reboot → autostart → upgrade → GUI attach →
shutdown-flush → different-user connection-refused. Autounlock: enable → **DPAPI decrypt
inside the batch-logon task** (the primary unknown) → cold stake-only unlock → foreign-listener
refused → self-heal on restart → remove. Known items to confirm there: the shutdown-flush
mechanism (above); that stored-password task registration requires an **elevated** shell and
the "Log on as a batch job" right for the run-as account; and Microsoft/AzureAD-account
principal forms.
