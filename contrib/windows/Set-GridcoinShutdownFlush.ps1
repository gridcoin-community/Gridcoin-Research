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

# --- run-time State the shutdown engine reads -----------------------------------------
# gpedit.msc writes this directly; a foreground gpupdate does NOT translate the local GPO's
# scripts.ini into it, so we write it ourselves, matching gpedit's exact layout for a local
# computer .cmd shutdown script (observed on Windows 11):
#   State\...\Shutdown\<gpo>       GPO-ID=LocalGPO SOM-ID=Local FileSysPath=<...\Machine>
#                                  DisplayName/GPOName='Local Group Policy' PSScriptOrder=<count>
#   State\...\Shutdown\<gpo>\<n>   Script=<full .cmd path> Parameters='' ExecTime=0 (QWORD)
$stateBase = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Group Policy\State\Machine\Scripts\Shutdown'

function Read-RegString([string]$path, [string]$name) {
    $i = Get-ItemProperty -LiteralPath $path -Name $name -EA SilentlyContinue
    if ($i) { return [string]$i.$name }
    return $null
}
# Index of the GPO subkey whose GPO-ID = LocalGPO, or (with -CreateIfMissing) the next free
# index so we compose alongside any domain-GPO shutdown scripts instead of colliding.
function Get-LocalGpoStateIndex([switch]$CreateIfMissing) {
    $found = $null; $max = -1
    if (Test-Path -LiteralPath $stateBase) {
        foreach ($g in Get-ChildItem -LiteralPath $stateBase -EA SilentlyContinue) {
            if ($g.PSChildName -notmatch '^\d+$') { continue }
            $n = [int]$g.PSChildName; if ($n -gt $max) { $max = $n }
            if ((Read-RegString $g.PSPath 'GPO-ID') -eq 'LocalGPO') { $found = $n }
        }
    }
    if ($null -eq $found -and $CreateIfMissing) { return $max + 1 }
    return $found
}
function Set-ShutdownScriptState([string]$cmdFullPath) {
    $gKey = Join-Path $stateBase (Get-LocalGpoStateIndex -CreateIfMissing)
    # Reuse our script index if our path is already registered, else take the next free one.
    $scr = $null; $maxS = -1
    if (Test-Path -LiteralPath $gKey) {
        foreach ($s in Get-ChildItem -LiteralPath $gKey -EA SilentlyContinue) {
            if ($s.PSChildName -notmatch '^\d+$') { continue }
            $m = [int]$s.PSChildName; if ($m -gt $maxS) { $maxS = $m }
            if ((Read-RegString $s.PSPath 'Script') -ieq $cmdFullPath) { $scr = $m }
        }
    }
    if ($null -eq $scr) { $scr = $maxS + 1 }
    $sKey = Join-Path $gKey $scr
    New-Item -Path $gKey -Force | Out-Null
    New-Item -Path $sKey -Force | Out-Null
    New-ItemProperty -LiteralPath $gKey -Name 'GPO-ID'      -Value 'LocalGPO'           -PropertyType String -Force | Out-Null
    New-ItemProperty -LiteralPath $gKey -Name 'SOM-ID'      -Value 'Local'              -PropertyType String -Force | Out-Null
    New-ItemProperty -LiteralPath $gKey -Name 'FileSysPath' -Value $gpMachine           -PropertyType String -Force | Out-Null
    New-ItemProperty -LiteralPath $gKey -Name 'DisplayName' -Value 'Local Group Policy' -PropertyType String -Force | Out-Null
    New-ItemProperty -LiteralPath $gKey -Name 'GPOName'     -Value 'Local Group Policy' -PropertyType String -Force | Out-Null
    New-ItemProperty -LiteralPath $sKey -Name 'Script'     -Value $cmdFullPath -PropertyType String -Force | Out-Null
    New-ItemProperty -LiteralPath $sKey -Name 'Parameters' -Value ''           -PropertyType String -Force | Out-Null
    New-ItemProperty -LiteralPath $sKey -Name 'ExecTime'   -Value 0            -PropertyType QWord  -Force | Out-Null
    $count = @(Get-ChildItem -LiteralPath $gKey -EA SilentlyContinue | Where-Object { $_.PSChildName -match '^\d+$' }).Count
    New-ItemProperty -LiteralPath $gKey -Name 'PSScriptOrder' -Value $count -PropertyType DWord -Force | Out-Null
}
function Remove-ShutdownScriptState([string]$cmdFullPath) {
    $gi = Get-LocalGpoStateIndex
    if ($null -eq $gi) { return }
    $gKey = Join-Path $stateBase $gi
    if (-not (Test-Path -LiteralPath $gKey)) { return }
    foreach ($s in Get-ChildItem -LiteralPath $gKey -EA SilentlyContinue) {
        if ($s.PSChildName -notmatch '^\d+$') { continue }
        if ((Read-RegString $s.PSPath 'Script') -ieq $cmdFullPath) { Remove-Item -LiteralPath $s.PSPath -Recurse -Force -EA SilentlyContinue }
    }
    $remaining = @(Get-ChildItem -LiteralPath $gKey -EA SilentlyContinue | Where-Object { $_.PSChildName -match '^\d+$' })
    if ($remaining.Count -eq 0) { Remove-Item -LiteralPath $gKey -Recurse -Force -EA SilentlyContinue }  # local GPO has no scripts left
    else { New-ItemProperty -LiteralPath $gKey -Name 'PSScriptOrder' -Value $remaining.Count -PropertyType DWord -Force | Out-Null }
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
    Remove-ShutdownScriptState $cmdPath
    Write-Host "Removed the run-time shutdown-script state. Reboot to confirm it no longer runs."
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
# Fail CLOSED: only privileged principals (SYSTEM, Administrators, TrustedInstaller) may hold
# write/delete/ownership on the binary. ANY other principal with such rights -- a specific
# user, a custom group -- or an ACE whose SID cannot be resolved is treated as a risk. Test
# ONLY the write/delete/own-change bits, NOT the composite Modify/FullControl rights, whose
# bit patterns overlap ReadAndExecute/Synchronize (masking those would wrongly flag the
# read-only Program Files "Users: ReadAndExecute, Synchronize" ACE).
function Get-NonAdminWriteAce([string]$p) {
    try { $acl = Get-Acl -LiteralPath $p } catch { return 'its ACL could not be read' }
    $privileged = @(
        'S-1-5-18',                                                        # SYSTEM
        'S-1-5-32-544',                                                    # Administrators
        'S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464'  # TrustedInstaller
    )
    $wmask = [int]([System.Security.AccessControl.FileSystemRights]'Write, Delete, DeleteSubdirectoriesAndFiles, ChangePermissions, TakeOwnership')
    foreach ($ace in $acl.Access) {
        if ($ace.AccessControlType -ne [System.Security.AccessControl.AccessControlType]::Allow) { continue }
        if (([int]$ace.FileSystemRights -band $wmask) -eq 0) { continue }
        $idr = $ace.IdentityReference
        $sid = $null
        if ($idr -is [System.Security.Principal.SecurityIdentifier]) { $sid = $idr.Value }
        else { try { $sid = $idr.Translate([System.Security.Principal.SecurityIdentifier]).Value } catch { $sid = $null } }
        if ($null -eq $sid) { return "$idr (unresolvable SID) has '$($ace.FileSystemRights)'" }
        if ($privileged -notcontains $sid) { return "$idr has '$($ace.FileSystemRights)'" }
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

# Wire it into the local machine shutdown scripts (add our entry; don't clobber existing ones).
$lines = Read-IniLines $scriptsIni
$cmds = Get-ShutdownCmds $lines
$already = $false
foreach ($k in $cmds.Keys) { if ($cmds[$k] -ieq $cmdPath) { $already = $true } }
if (-not $already) {
    $idx = 0
    if ($cmds.Count -gt 0) { $idx = ((($cmds.Keys) | Measure-Object -Maximum).Maximum) + 1 }
    # Ensure a [Shutdown] section exists; our indexed entry is inserted right after its header
    # (below). Index N is max-existing + 1, so ordering among entries is preserved regardless.
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
Set-ShutdownScriptState $cmdPath

# Confirm the run-time state the shutdown engine reads is now present.
$ok = $false
if (Test-Path -LiteralPath $stateBase) {
    Get-ChildItem -LiteralPath $stateBase -Recurse -EA SilentlyContinue | ForEach-Object {
        if ((Read-RegString $_.PSPath 'Script') -ieq $cmdPath) { $ok = $true }
    }
}
Write-Host ""
if ($ok) {
    Write-Host "Registered the shutdown script (scripts.ini + gpt.ini + run-time state)." -ForegroundColor Green
} else {
    Write-Host "Wrote scripts.ini + gpt.ini, but could not confirm the run-time state key." -ForegroundColor Yellow
}
Write-Host "NOTE: if a DOMAIN GPO enables 'Turn off Local Group Policy Objects processing', this local"
Write-Host "script is ignored at boot -- deploy the .cmd via a domain GPO instead (see README)."
Write-Host ""
Write-Host "VALIDATE: reboot once, then check the core's debug.log for a full graceful shutdown"
Write-Host "sequence ('Shutdown: ... Final flush of wallet database ...') just before the restart."
Write-Host "Remove with:  .\Set-GridcoinShutdownFlush.ps1 -Remove"
