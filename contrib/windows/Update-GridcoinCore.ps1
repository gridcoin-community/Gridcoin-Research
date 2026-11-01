#Requires -Version 5.1
<#
.SYNOPSIS
    Safely upgrade a running Gridcoin multiprocess core (the locked-exe problem).

.DESCRIPTION
    A running gridcoinresearchd.exe is write-locked by Windows, and the core runs
    headless. This drives a graceful upgrade:

      1. Detect a running core.
      2. Graceful stop by STARTING the canonical Core-Stop task (never
         Stop-ScheduledTask on Core-Start -- that is a hard kill, no flush).
      3. Wait for the process to exit AND the exe's write-lock to release (a
         connected -multiprocess GUI also quit on core-stop, releasing its lock).
      4. Optionally copy new binaries in (or the caller already did).
      5. Restart via the Core-Start task. Autounlock (Plan 5) self-heals on the
         fresh instance.

    SAFE BY DEFAULT: graceful-only; on stop/lock-release timeout it ABORTS and
    leaves the old binary in place -- it never corrupts the datadir to force an
    upgrade. Idempotent: with no core running it goes straight to replace/restart.

    NOTE: if the install lives under C:\Program Files, replacing binaries needs an
    ELEVATED PowerShell. When -NewBinariesDir is given, this is checked up front
    (before stopping the core) so a permission problem aborts cleanly.
#>
[CmdletBinding()]
param(
    # Default resolved below across both layouts (installer daemon\ subfolder, or a build
    # tree with the exe one level up). An explicit -CorePath is honored as-is.
    [string]$CorePath,

    # Optional: a directory containing the new gridcoinresearch*.exe to copy over
    # the installed ones once the lock is released. Omit if the caller replaces the
    # binaries itself between stop and restart.
    [string]$NewBinariesDir,

    [string]$TaskFolder = 'Gridcoin',
    [int]$StopTimeoutSec = 120,   # BDB flush can be slow
    [int]$PollMs = 1000
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (-not $CorePath) {
    $CorePath = @(
        (Join-Path $PSScriptRoot '..\daemon\gridcoinresearchd.exe'),   # NSIS installer layout
        (Join-Path $PSScriptRoot '..\gridcoinresearchd.exe')           # exe alongside the scripts' parent
    ) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (-not $CorePath) { $CorePath = (Join-Path $PSScriptRoot '..\daemon\gridcoinresearchd.exe') }
}
if (-not (Test-Path -LiteralPath $CorePath)) {
    throw "gridcoinresearchd.exe not found at '$CorePath'. Pass -CorePath explicitly (installer layout: " +
          "...\GridcoinResearch\daemon\gridcoinresearchd.exe, with these scripts in ...\GridcoinResearch\windows\)."
}
$CorePath = (Resolve-Path -LiteralPath $CorePath).Path
$coreDir = Split-Path -Parent $CorePath

function Test-FileWriteLocked {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return $false }
    try {
        $fs = [System.IO.File]::Open($Path, 'Open', 'ReadWrite', 'None')
        $fs.Close()
        return $false
    } catch [System.IO.IOException] {
        # Sharing violation: still held open by the (exiting) core.
        return $true
    } catch [System.UnauthorizedAccessException] {
        # No write permission (e.g. Program Files, non-elevated) -- NOT an in-use
        # lock. Treated as unlocked here; the up-front writability check reports the
        # permission problem clearly before any copy is attempted.
        return $false
    }
}

function Get-RunningCore {
    # Match by image path so we don't touch an unrelated gridcoinresearchd from a
    # different install. Reading .Path can throw on a process we can't inspect
    # (another user / bitness); skip those.
    Get-Process -Name 'gridcoinresearchd' -ErrorAction SilentlyContinue | Where-Object {
        try { $_.Path -and ($_.Path -ieq $CorePath) } catch { $false }
    }
}

# Up-front: if we will copy binaries, confirm the install dir is writable BEFORE
# stopping the core, so a permission problem aborts without leaving it stopped.
if ($NewBinariesDir) {
    if (-not (Test-Path -LiteralPath $NewBinariesDir)) { throw "NewBinariesDir '$NewBinariesDir' not found." }
    # Refuse a no-op "upgrade" that stops+restarts on the old binaries: the new dir
    # must actually contain the core exe.
    if (-not (Test-Path -LiteralPath (Join-Path $NewBinariesDir 'gridcoinresearchd.exe'))) {
        throw "NewBinariesDir '$NewBinariesDir' does not contain gridcoinresearchd.exe; refusing a no-op upgrade."
    }
    $probe = Join-Path $coreDir ('.grc-update-write-probe-' + [guid]::NewGuid().ToString('N'))
    try {
        [System.IO.File]::WriteAllText($probe, '')
        Remove-Item -LiteralPath $probe -Force
    } catch {
        throw "Cannot write to '$coreDir' (needed to replace binaries). Run this from an ELEVATED " +
              "PowerShell -- Program Files installs require it. The core was NOT stopped."
    }
}

# 1-2. Graceful stop.
if (Get-RunningCore) {
    Write-Host "Core running; invoking the graceful Core-Stop task..."
    Start-ScheduledTask -TaskPath "\$TaskFolder\" -TaskName 'Core-Stop'

    # 3. Wait for exit AND write-lock release.
    $deadline = (Get-Date).AddSeconds($StopTimeoutSec)
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds $PollMs
        if (-not (Get-RunningCore) -and -not (Test-FileWriteLocked -Path $CorePath)) { break }
    }
    if ((Get-RunningCore) -or (Test-FileWriteLocked -Path $CorePath)) {
        throw "Core did not stop / '$CorePath' is still write-locked after ${StopTimeoutSec}s. " +
              "Close any process using it and retry. Binaries were NOT replaced."
    }
    Write-Host "Core stopped and the exe lock is released."
} else {
    Write-Host "No core running; proceeding to replace/restart."
}

# 4. Replace binaries (optional convenience).
if ($NewBinariesDir) {
    foreach ($exe in 'gridcoinresearchd.exe', 'gridcoinresearch.exe') {
        $src = Join-Path $NewBinariesDir $exe
        if (Test-Path -LiteralPath $src) {
            Copy-Item -LiteralPath $src -Destination (Join-Path $coreDir $exe) -Force
            Write-Host "Replaced $exe"
        }
    }
}

# 5. Restart.
Start-ScheduledTask -TaskPath "\$TaskFolder\" -TaskName 'Core-Start'
Write-Host "Restarted \$TaskFolder\Core-Start. Autounlock (if enabled) self-heals on the new instance." -ForegroundColor Green
