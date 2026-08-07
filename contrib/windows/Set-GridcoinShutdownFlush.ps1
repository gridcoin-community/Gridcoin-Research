#Requires -Version 5.1
<#
.SYNOPSIS
    Configure a Group Policy Computer->Shutdown script that gracefully stops the Gridcoin
    multiprocess core (flushing BDB/LevelDB) before Windows shuts down. -Remove undoes it.

.DESCRIPTION
    The Core-Stop task carries NO OS-shutdown trigger, on purpose: a scheduled task cannot
    make Windows wait, so an event-triggered stop is best-effort and gets cut short mid-flush.
    A Group Policy Computer->Shutdown script is the reliable primitive -- Windows runs it
    SYNCHRONOUSLY and WAITS for it (bounded by "Maximum wait time for Group Policy scripts",
    default 600s).

    This writes a small .cmd (run as SYSTEM at shutdown) that issues the graceful RPC stop
    and waits for the core to exit, then wires it into the LOCAL Group Policy machine
    shutdown scripts (scripts.ini + gpt.ini) and runs gpupdate so the change takes effect.
    The stop runs as SYSTEM (which has ACL access to the datadir and can reach the loopback
    RPC), so it does not depend on the wallet user's session still being available late in
    shutdown.

    DO NOT also put an OS-shutdown trigger on Core-Stop -- the two would double-stop and
    race. Install-GridcoinCoreTask.ps1 deliberately leaves Core-Stop on-demand.

    DOMAINS: local and domain-GPO shutdown scripts STACK -- all applicable scripts run,
    additively, no override, bounded collectively by the max-wait-time. BUT if a domain GPO
    enables "Turn off Local Group Policy Objects processing", this LOCAL script is IGNORED;
    in that case deploy the same .cmd via a DOMAIN GPO instead (see contrib/windows/README.md).

    VALIDATE: after running this, reboot once and confirm a clean flush in the core's
    debug.log -- a full "Shutdown: ... Final flush of wallet database ..." sequence logged
    immediately before the restart (rather than a bare restart with no shutdown lines).

.NOTES
    Requires an ELEVATED PowerShell (writes under %SystemRoot%\System32\GroupPolicy and HKLM).
#>
[CmdletBinding()]
param(
    [string]$DataDir = (Join-Path $env:APPDATA 'GridcoinResearch'),

    # Path to gridcoinresearchd.exe; resolved across the installer (daemon\) and build-tree
    # layouts if not given (same logic as Install-GridcoinCoreTask.ps1).
    [string]$CorePath,

    # Cap on how long the shutdown script waits for the core to exit (seconds). Keep it well
    # under "Maximum wait time for Group Policy scripts" (default 600s).
    [int]$WaitSeconds = 300,

    [switch]$Remove
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$id = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
if (-not $id.IsInRole([Security.Principal.WindowsBuiltinRole]::Administrator)) {
    throw "Run this from an ELEVATED PowerShell (it writes to %SystemRoot%\System32\GroupPolicy and HKLM)."
}

$gpMachine   = Join-Path $env:SystemRoot 'System32\GroupPolicy\Machine'
$shutdownDir = Join-Path $gpMachine 'Scripts\Shutdown'
$scriptsIni  = Join-Path $gpMachine 'Scripts\scripts.ini'
$gptIni      = Join-Path $env:SystemRoot 'System32\GroupPolicy\gpt.ini'
$cmdPath     = Join-Path $shutdownDir 'GridcoinShutdownFlush.cmd'
# Group Policy "Scripts" client-side extension GUID + the scripts snap-in GUID.
$scriptsCse  = '[{42B5FAAE-6536-11D2-AE5A-0000F87571E3}{40B6664F-4972-11D1-A7CA-0000F87571E3}]'

# --- scripts.ini helpers (UTF-16LE, the format gpedit writes) -------------------------
function Read-IniLines([string]$path) {
    # The leading comma stops PowerShell from unrolling an empty/single-element array on
    # return (which would surface as $null / a bare string at the call site).
    if (Test-Path -LiteralPath $path) { return ,@(Get-Content -LiteralPath $path -Encoding Unicode) }
    return ,@()
}
function Write-IniLines([string]$path, [string[]]$lines) {
    $dir = Split-Path -Parent $path
    if (-not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    Set-Content -LiteralPath $path -Value $lines -Encoding Unicode
    (Get-Item -LiteralPath $path -Force).Attributes = 'Hidden'
}
# Return the [Shutdown] section's NCmdLine entries as an ordered list of paths.
function Get-ShutdownCmds([string[]]$lines) {
    $inSec = $false; $cmds = @{}
    foreach ($l in $lines) {
        if ($l -match '^\s*\[(.+)\]\s*$') { $inSec = ($Matches[1] -ieq 'Shutdown'); continue }
        if ($inSec -and $l -match '^\s*(\d+)CmdLine\s*=\s*(.*)$') { $cmds[[int]$Matches[1]] = $Matches[2].Trim() }
    }
    return $cmds
}

function Bump-GptIni {
    # Ensure gpt.ini exists, carries the Scripts CSE in gPCMachineExtensionNames, and bump
    # Version so Group Policy treats the local GPO as changed on the next (even non-forced)
    # gpupdate. Version packs computerVersion in the HIGH 16 bits and userVersion in the low
    # 16; shutdown scripts are Computer Configuration, so bump the machine word (+65536).
    $lines = Read-IniLines $gptIni
    if ($lines.Count -eq 0) { $lines = @('[General]', 'Version=0') }
    $hasVer = $false; $hasCse = $false
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match '^\s*Version\s*=\s*(\d+)\s*$') {
            $lines[$i] = 'Version=' + ([int]$Matches[1] + 65536); $hasVer = $true
        }
        if ($lines[$i] -match '^\s*gPCMachineExtensionNames\s*=') {
            if ($lines[$i] -notmatch '42B5FAAE-6536-11D2-AE5A-0000F87571E3') {
                $lines[$i] = $lines[$i].TrimEnd() + $scriptsCse
            }
            $hasCse = $true
        }
    }
    if (-not $hasVer) { $lines += 'Version=65536' }
    if (-not $hasCse) { $lines += ('gPCMachineExtensionNames=' + $scriptsCse) }
    Write-IniLines $gptIni $lines
}

# --- -Remove ---------------------------------------------------------------------------
if ($Remove) {
    $lines = Read-IniLines $scriptsIni
    if ($lines.Count -gt 0) {
        # Drop only OUR entry (the paired NCmdLine/NParameters referencing our .cmd).
        $drop = @{}
        $cmds = Get-ShutdownCmds $lines
        foreach ($k in $cmds.Keys) { if ($cmds[$k] -ieq $cmdPath) { $drop[$k] = $true } }
        if ($drop.Count -gt 0) {
            $out = New-Object System.Collections.Generic.List[string]
            $inSec = $false
            foreach ($l in $lines) {
                if ($l -match '^\s*\[(.+)\]\s*$') { $inSec = ($Matches[1] -ieq 'Shutdown'); $out.Add($l); continue }
                if ($inSec -and $l -match '^\s*(\d+)(CmdLine|Parameters)\s*=' -and $drop.ContainsKey([int]$Matches[1])) { continue }
                $out.Add($l)
            }
            Write-IniLines $scriptsIni $out.ToArray()
            Bump-GptIni
            Write-Host "Removed the Gridcoin shutdown script from local Group Policy."
        } else {
            Write-Host "No Gridcoin entry in the local shutdown scripts (ok)."
        }
    } else {
        Write-Host "No local shutdown scripts.ini present (ok)."
    }
    if (Test-Path -LiteralPath $cmdPath) { Remove-Item -LiteralPath $cmdPath -Force; Write-Host "Deleted $cmdPath" }
    & gpupdate.exe /target:computer /force | Out-Null
    Write-Host "Done. Reboot to confirm the shutdown script no longer runs."
    return
}

# --- add ------------------------------------------------------------------------------
if (-not $CorePath) {
    $CorePath = @(
        (Join-Path $PSScriptRoot '..\daemon\gridcoinresearchd.exe'),
        (Join-Path $PSScriptRoot '..\gridcoinresearchd.exe')
    ) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (-not $CorePath) { $CorePath = (Join-Path $PSScriptRoot '..\daemon\gridcoinresearchd.exe') }
}
if (-not (Test-Path -LiteralPath $CorePath)) { throw "gridcoinresearchd.exe not found at '$CorePath'. Pass -CorePath." }
$CorePath = (Resolve-Path -LiteralPath $CorePath).Path
# Both get baked into a batch script: reject the chars that would break/expand there.
if ($DataDir -match '["%]') { throw "-DataDir must not contain a double-quote or percent character (it is baked into a batch script)." }
if ($CorePath -match '%')   { throw "-CorePath must not contain a percent character (it is baked into a batch script)." }

# SECURITY: the shutdown .cmd runs as SYSTEM and invokes $CorePath. If a standard user can
# replace that exe, they gain SYSTEM code execution at shutdown (local privilege escalation).
# Refuse unless the binary is writable only by privileged principals (e.g. under Program Files).
# Test ONLY the write/delete/own-change bits -- NOT the composite Modify/FullControl rights,
# whose bit patterns overlap ReadAndExecute/Synchronize, so masking against them wrongly flags
# read-only ACEs (e.g. the default Program Files "Users: ReadAndExecute, Synchronize" grant).
function Get-NonAdminWriteAce([string]$p) {
    try { $acl = Get-Acl -LiteralPath $p } catch { return 'its ACL could not be read' }
    $bad = @('S-1-5-32-545', 'S-1-5-11', 'S-1-1-0', 'S-1-5-4', 'S-1-5-32-546')  # Users, AuthUsers, Everyone, Interactive, Guests
    $wmask = [int]([System.Security.AccessControl.FileSystemRights]'Write, Delete, DeleteSubdirectoriesAndFiles, ChangePermissions, TakeOwnership')
    foreach ($ace in $acl.Access) {
        if ($ace.AccessControlType -ne [System.Security.AccessControl.AccessControlType]::Allow) { continue }
        if (([int]$ace.FileSystemRights -band $wmask) -eq 0) { continue }
        $sid = try { $ace.IdentityReference.Translate([System.Security.Principal.SecurityIdentifier]).Value } catch { '' }
        if ($bad -contains $sid) { return "$($ace.IdentityReference) has '$($ace.FileSystemRights)'" }
    }
    return $null
}
$offender = Get-NonAdminWriteAce $CorePath
if ($offender) {
    throw "Refusing to wire a SYSTEM shutdown script to '$CorePath': $offender, which would let a non-admin " +
          "replace the exe and gain SYSTEM code execution at shutdown. Install the core under an admin-only " +
          "location (e.g. Program Files) and point -CorePath there."
}

# The shutdown stop needs rpcuser/rpcpassword from the datadir conf. Warn if it's not there
# (e.g. run from a different admin account whose %APPDATA% is not the core's datadir).
if (-not (Test-Path -LiteralPath (Join-Path $DataDir 'gridcoinresearch.conf'))) {
    Write-Host ("WARNING: no gridcoinresearch.conf in '$DataDir'. The shutdown stop needs its rpcuser/rpcpassword; " +
                "if this is not the core's datadir the flush will be a silent no-op. Pass the correct -DataDir.") -ForegroundColor Yellow
}

# The shutdown .cmd (runs as SYSTEM). Issue the graceful RPC stop, then poll until the core
# exits or the cap is hit. `ping` is the sleep (timeout.exe needs a console and fails here).
$loops = [Math]::Max(1, [int]($WaitSeconds / 2))
$cmd = @"
@echo off
rem Gridcoin graceful shutdown flush (Group Policy Computer->Shutdown script; runs as SYSTEM).
rem Managed by Set-GridcoinShutdownFlush.ps1 -- do not edit by hand.
"$CorePath" -datadir="$DataDir" stop
set /a n=0
:wait
tasklist /fi "imagename eq gridcoinresearchd.exe" | find /i "gridcoinresearchd.exe" >nul || goto done
set /a n+=1
if %n% geq $loops goto done
ping -n 3 127.0.0.1 >nul
goto wait
:done
"@
if (-not (Test-Path -LiteralPath $shutdownDir)) { New-Item -ItemType Directory -Path $shutdownDir -Force | Out-Null }
Set-Content -LiteralPath $cmdPath -Value $cmd -Encoding Ascii
Write-Host "Wrote shutdown script: $cmdPath"

# Wire it into the local machine shutdown scripts (append; don't clobber existing entries).
$lines = Read-IniLines $scriptsIni
$cmds = Get-ShutdownCmds $lines
$already = $false
foreach ($k in $cmds.Keys) { if ($cmds[$k] -ieq $cmdPath) { $already = $true } }
if (-not $already) {
    $idx = 0
    if ($cmds.Count -gt 0) { $idx = ((($cmds.Keys) | Measure-Object -Maximum).Maximum) + 1 }
    # Ensure a [Shutdown] section exists, then append our indexed entry at the end of it.
    $has = $false
    foreach ($l in $lines) { if ($l -match '^\s*\[Shutdown\]\s*$') { $has = $true } }
    $list = New-Object System.Collections.Generic.List[string]
    if ($lines.Count -gt 0) { $list.AddRange([string[]]$lines) }  # $lines is empty on a fresh box
    if (-not $has) { $list.Add('[Shutdown]') }
    # Insert our two keys right after the [Shutdown] header (simplest correct placement).
    $out = New-Object System.Collections.Generic.List[string]
    foreach ($l in $list) {
        $out.Add($l)
        if ($l -match '^\s*\[Shutdown\]\s*$') {
            $out.Add("${idx}CmdLine=$cmdPath")
            $out.Add("${idx}Parameters=")
        }
    }
    Write-IniLines $scriptsIni $out.ToArray()
    Write-Host "Registered as local shutdown script #$idx."
} else {
    Write-Host "Already registered as a local shutdown script (ok)."
}

Bump-GptIni
& gpupdate.exe /target:computer /force | Out-Null

# Verify Group Policy populated the run-time state the shutdown engine reads.
$stateKey = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Group Policy\State\Machine\Scripts\Shutdown'
$ok = $false
if (Test-Path -LiteralPath $stateKey) {
    Get-ChildItem -LiteralPath $stateKey -Recurse -EA SilentlyContinue | ForEach-Object {
        $s = (Get-ItemProperty -LiteralPath $_.PSPath -Name Script -EA SilentlyContinue).Script
        if ($s -and ($s -ieq $cmdPath -or $s -like '*GridcoinShutdownFlush.cmd')) { $ok = $true }
    }
}
Write-Host ""
if ($ok) {
    Write-Host "Group Policy registered the shutdown script (state confirmed)." -ForegroundColor Green
} else {
    Write-Host "Wired into scripts.ini + gpt.ini, but the GP run-time state wasn't confirmed yet." -ForegroundColor Yellow
    Write-Host "It often populates on the next boot; verify by rebooting (below). If a DOMAIN GPO sets" -ForegroundColor Yellow
    Write-Host "'Turn off Local Group Policy Objects processing', deploy the .cmd via a domain GPO instead." -ForegroundColor Yellow
}
Write-Host ""
Write-Host "VALIDATE: reboot once, then check the core's debug.log for a full graceful shutdown"
Write-Host "sequence ('Shutdown: ... Final flush of wallet database ...') just before the restart."
Write-Host "Remove with:  .\Set-GridcoinShutdownFlush.ps1 -Remove"
