# Running Gridcoin unattended (background service + stake-only autounlock)

This guide walks through running the Gridcoin **multiprocess** core as a background service that **stakes
unattended**, on **Linux** (systemd) and **Windows** (Task Scheduler), including the optional **stake-only
autounlock** for an encrypted wallet.

It is a task-oriented walkthrough. For the underlying model and the full mechanism, see
[`multiprocess.md`](multiprocess.md); for the Windows script reference, see
[`../contrib/windows/README.md`](../contrib/windows/README.md).

## Is this for you?

Use this if you want the wallet to **run headless and keep staking** across reboots without leaving the GUI
open. In multiprocess mode the **core** (`gridcoinresearchd`) runs as a background service and owns the wallet
and the blockchain; the **GUI** (`gridcoinresearch`) is a separate program you open on demand and it *attaches*
to the running core. See [`multiprocess.md`](multiprocess.md) for the split model.

Staking on an **encrypted** wallet additionally needs the wallet unlocked after every start. The optional
autounlock does that **for staking only** — never for spending — with the passphrase held in the OS credential
store, never in the wallet, on a command line, or in a log. It is opt-in and off by default.

---

## Linux (systemd)

The packaged daemon ships a hardened systemd unit, so this is nearly turnkey.

### 1. Run the core as a service

```bash
sudo systemctl enable --now gridcoinresearchd.service
```

That starts the core now and on every boot. Attach the GUI when you want it:

```bash
gridcoinresearch -multiprocess
```

### 2. (Optional) Enable stake-only autounlock

Encrypt the wallet passphrase, bound to this host (and its TPM, if present), then enable the companion unit:

```bash
systemd-ask-password 'Wallet passphrase:' \
    | sudo systemd-creds encrypt --name=wallet-passphrase - /etc/gridcoin/wallet-passphrase.cred
sudo chown gridcoin:gridcoin /etc/gridcoin/wallet-passphrase.cred
sudo chmod 0400 /etc/gridcoin/wallet-passphrase.cred

sudo systemctl enable --now gridcoinresearchd-autounlock.service
```

> **Never put the passphrase in the command itself** (`printf '%s' 'MY-PASSPHRASE' | …`). Even though a shell
> builtin keeps it out of `ps`, the whole command line is written to `~/.bash_history`, and to sudo's I/O log if
> `log_input` is enabled. `systemd-ask-password` prompts for it, so it is never part of any command. Without
> `systemd-ask-password`, use a shell prompt instead:
> `read -rs -p 'Wallet passphrase: ' pw && printf '%s' "$pw" | sudo systemd-creds encrypt …; unset pw`.

The autounlock unit runs whenever the core starts (boot, restart, upgrade), unlocks **stake-only**, and exits.
A rejected passphrase or an unverifiable RPC listener leaves it `failed` (visible) rather than looping. Full
detail — including the listener-ownership gate and the security model — is in
[`multiprocess.md` → *Unattended stake-only autounlock (Linux)*](multiprocess.md).

The daemon package *Recommends* `python3` (the helper is stdlib-only Python 3); on a minimal install run
`sudo apt install python3` first.

---

## Windows (Task Scheduler)

On Windows the same roles run as **scheduled tasks**, driven by PowerShell scripts the installer places in
`…\GridcoinResearch\windows\`. Everything below runs in an **elevated** PowerShell (Run as administrator) — a
boot-triggered task needs it, and so does DPAPI.

Run-as **your own (the wallet) user, never SYSTEM**: the core's IPC socket is owner-only, so a SYSTEM core would
lock your own GUI out.

### 1. Install and prepare the session

Run the installer. It lays out `gridcoinresearch.exe` (GUI), `daemon\gridcoinresearchd.exe`, and the helper
scripts in `windows\`. Then, in an elevated PowerShell:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
cd "C:\Program Files\GridcoinResearch\windows"
```

> Set the policy in the session like this and call the scripts directly. Do **not** wrap a call in
> `powershell.exe -ExecutionPolicy Bypass .\script -Arg "C:\Program Files\..."` — the outer shell re-parses and
> splits the spaced path.

### 2. Run the core as a boot task

```powershell
.\Install-GridcoinCoreTask.ps1
#   enter your Windows LOGIN password at the prompt (so the task can start at boot)
Start-ScheduledTask -TaskPath '\Gridcoin\' -TaskName 'Core-Start'
```

This registers **Core-Start** (boot) and **Core-Stop** (graceful), then starts the core headless. Attach the
GUI with `.\Start-GridcoinGui.ps1` (or `.\Start-GridcoinGui.ps1 -CreateShortcut` for a Start-Menu entry).

> **First-start note:** the *first* multiprocess-capable start does a one-time `wallet.dat` rewrite and may
> exit. If `Get-Process gridcoinresearchd` shows nothing right after, run the `Start-ScheduledTask … Core-Start`
> line once more.

**To stop the core, start the Core-Stop task** — `Start-ScheduledTask -TaskPath '\Gridcoin\' -TaskName
'Core-Stop'`. Never `Stop-ScheduledTask` the Core-Start task: that is a hard kill with no database flush.

For a reliable flush on OS shutdown, also configure a Group Policy Computer → Shutdown script (a scheduled task
alone cannot block shutdown) — see [`../contrib/windows/README.md` → *Graceful stop on OS
shutdown*](../contrib/windows/README.md).

### 3. (Optional) Enable stake-only autounlock

Run this **at the machine's own console** (elevated) — DPAPI encryption needs an interactive logon and fails
over a remote (SSH/PSRemoting) session:

```powershell
.\Set-GridcoinAutounlock.ps1 -DataDir "$env:APPDATA\GridcoinResearch"
#   prompts: the WALLET passphrase (twice), then your Windows LOGIN password
Start-ScheduledTask -TaskPath '\Gridcoin\' -TaskName 'Autounlock'
```

> **Install location matters.** Both `Install-GridcoinCoreTask.ps1` and `Set-GridcoinAutounlock.ps1` refuse to
> register a task if the script (or launcher) they will execute — or any directory above it — can be written,
> deleted, or taken over by a local principal other than an administrator or the wallet user. Otherwise another
> local user could swap the script and have your own task run it *as you*, handing over the passphrase. Keep the
> install under `Program Files` (or another admin-owned location), not in a shared/temp folder.

This encrypts the passphrase with **DPAPI (CurrentUser)** to an owner-only
`…\GridcoinResearch\autounlock\passphrase.cred` and registers a resident task that unlocks **stake-only** and
re-unlocks after each core restart. It refuses to send anything if a *foreign* process owns the RPC port (the
listener-ownership gate). Detail: [`../contrib/windows/README.md` →
*Autounlock*](../contrib/windows/README.md).

---

## Upgrading

- **Linux:** upgrade the package, then `sudo systemctl restart gridcoinresearchd.service`. Autounlock re-runs on
  the fresh instance.
- **Windows:** with the core running, from the `windows\` folder:
  ```powershell
  .\Update-GridcoinCore.ps1 -NewBinariesDir C:\path\to\new\bin
  ```
  It gracefully stops the core, waits for the exe lock to release, copies the new binaries, and restarts. If the
  stop times out it aborts and leaves the old binary in place (safe by default).

## Verifying it works

- The core is up: `gridcoinresearchd getinfo` (Linux) / `& "…\daemon\gridcoinresearchd.exe" -datadir=… getinfo`
  (Windows) returns.
- Autounlock is working and **stake-only**: `getwalletinfo` shows the wallet unlocked, and a spend-path command
  is refused —
  ```
  dumpprivkey <one of your addresses>
  → error code -13: "Wallet is unlocked for staking only."
  ```
- Logs: Linux → `journalctl -u gridcoinresearchd-autounlock.service`; Windows →
  `…\GridcoinResearch\autounlock\autounlock.log` (redacted — never contains the passphrase).

## Turning it off

- **Linux:** `sudo systemctl disable --now gridcoinresearchd-autounlock.service` (autounlock) and/or
  `gridcoinresearchd.service` (the core).
- **Windows:** `.\Set-GridcoinAutounlock.ps1 -Remove` (removes the task + deletes the DPAPI blob), and
  `.\Uninstall-GridcoinCoreTask.ps1` (removes Core-Start/Core-Stop).

> Two things `-Remove` does **not** do. It does **not relock a running wallet** — the wallet stays unlocked
> (stake-only) until the core is restarted or you issue `walletlock`; stop the core with
> `Start-ScheduledTask -TaskPath '\Gridcoin\' -TaskName 'Core-Stop'` if you want it locked now. And deleting the
> blob is an ordinary file delete, **not a secure erase**: the DPAPI-encrypted bytes may survive in free space,
> backups, or Volume Shadow Copies. If you believe the passphrase itself is compromised, change the wallet
> passphrase — do not rely on the delete.

## Troubleshooting

- **GUI says "could not connect to the daemon."** The core isn't running, wasn't started with `-multiprocess`,
  uses a different `-datadir`, or (Windows) is running as a different user. See
  [`multiprocess.md` → *Troubleshooting*](multiprocess.md).
- **Windows: "Access is denied" registering a task.** Use an **elevated** PowerShell; a boot-triggered task
  needs it, and the account needs the "Log on as a batch job" right.
- **Autounlock refuses with "the config omits rpcport and does not select a chain unambiguously."** Set
  `rpcport=` in the config file (or pass `--rpcport` / `-RpcPort`). The helper only assumes a default port when
  the config clearly selects one chain — guessing the mainnet port on, say, a testnet node could send that
  wallet's passphrase to a mainnet wallet listening on the same host. Note the helper reads only the config
  file: if the chain is selected on the core's *command line* (`-testnet`), set `rpcport=` in the config as well.
- **Autounlock log shows "REFUSING TO SEND CREDENTIALS."** A process owned by another user is listening on the
  RPC port — investigate before relying on autounlock; this is the gate working, not a bug.
- **Autounlock log shows "-17 already unlocked."** The wallet was unlocked by another path (e.g. a GUI unlock)
  and the staking-only restriction was not re-asserted — it may be spendable until the next restart.
