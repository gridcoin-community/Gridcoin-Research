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

    This writes two small files into the machine shutdown script folder, GridcoinShutdownFlush.cmd
    (one line, which starts Windows PowerShell) and GridcoinShutdownFlush.ps1, then wires the .cmd
    into the LOCAL Group Policy machine shutdown scripts (scripts.ini + gpt.ini) and the run-time
    state. At shutdown the .ps1 runs as SYSTEM. By Task Scheduler's COM interface it reads the
    process ID of the running Core-Start instance (the launcher, which exits only after the core
    has), starts the Core-Stop task, and waits for that process to exit, for at most -WaitSeconds.
    SYSTEM reads no file of the wallet user and runs no Gridcoin program: the core is stopped by
    the task, as the wallet user.

    DO NOT also put an OS-shutdown trigger on Core-Stop -- the two would double-stop and race.

    DOMAINS: local and domain-GPO shutdown scripts STACK -- all applicable scripts run,
    additively, no override, bounded collectively by the max-wait-time. BUT if a domain GPO
    enables "Turn off Local Group Policy Objects processing", this LOCAL script is IGNORED;
    in that case deploy the same two files via a DOMAIN GPO instead (see contrib/windows/README.md).

    VALIDATE: after running this, reboot once and confirm a clean flush in the core's
    debug.log -- a full "Shutdown: ... Final flush of wallet database ..." sequence logged
    immediately before the restart (rather than a bare restart with no shutdown lines).

.NOTES
    Requires an ELEVATED PowerShell (writes under %SystemRoot%\System32\GroupPolicy and HKLM), and
    the \Gridcoin\Core-Stop task (Install-GridcoinCoreTask.ps1).
#>
[CmdletBinding()]
param(
    # Cap on how long the shutdown script waits for the core to exit (seconds). Keep it well
    # under "Maximum wait time for Group Policy scripts" (default 600s).
    [int]$WaitSeconds = 300,

    # The folder of the Core-Start/Core-Stop pair to flush. One script serves one folder.
    [string]$TaskFolder = 'Gridcoin',

    [switch]$Remove
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# An elevated run loads modules only from the system's own module directories.
$env:PSModulePath = "$PSHOME\Modules;$env:ProgramFiles\WindowsPowerShell\Modules"

. (Join-Path $PSScriptRoot 'GridcoinCommon.ps1')

if (-not (Test-GridcoinElevated)) {
    throw "Run this from an ELEVATED PowerShell (it writes to %SystemRoot%\System32\GroupPolicy and HKLM)."
}

$gpMachine   = Join-Path $env:SystemRoot 'System32\GroupPolicy\Machine'
$shutdownDir = Join-Path $gpMachine 'Scripts\Shutdown'
$scriptsIni  = Join-Path $gpMachine 'Scripts\scripts.ini'
$gptIni      = Join-Path $env:SystemRoot 'System32\GroupPolicy\gpt.ini'
$cmdPath     = Join-Path $shutdownDir 'GridcoinShutdownFlush.cmd'
$ps1Path     = Join-Path $shutdownDir 'GridcoinShutdownFlush.ps1'
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
    $removedAny = $false
    $gi = Get-LocalGpoStateIndex
    if ($null -eq $gi) { return $false }
    $gKey = Join-Path $stateBase $gi
    if (-not (Test-Path -LiteralPath $gKey)) { return $false }
    foreach ($s in Get-ChildItem -LiteralPath $gKey -EA SilentlyContinue) {
        if ($s.PSChildName -notmatch '^\d+$') { continue }
        if ((Read-RegString $s.PSPath 'Script') -ieq $cmdFullPath) {
            Remove-Item -LiteralPath $s.PSPath -Recurse -Force -EA SilentlyContinue
            $removedAny = $true
        }
    }
    $remaining = @(Get-ChildItem -LiteralPath $gKey -EA SilentlyContinue | Where-Object { $_.PSChildName -match '^\d+$' })
    if ($remaining.Count -eq 0) { Remove-Item -LiteralPath $gKey -Recurse -Force -EA SilentlyContinue }  # local GPO has no scripts left
    else { New-ItemProperty -LiteralPath $gKey -Name 'PSScriptOrder' -Value $remaining.Count -PropertyType DWord -Force | Out-Null }
    return $removedAny
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
    if (Test-Path -LiteralPath $ps1Path) { Remove-Item -LiteralPath $ps1Path -Force; Write-Host "Deleted $ps1Path" }
    if (Remove-ShutdownScriptState $cmdPath) {
        Write-Host "Removed the run-time shutdown-script state. Reboot to confirm it no longer runs."
    } else {
        Write-Host "No run-time shutdown-script state for Gridcoin (ok)."
    }
    return
}

# --- add ------------------------------------------------------------------------------
if ($TaskFolder -notmatch '^[A-Za-z0-9 _.-]+$') {
    throw '-TaskFolder may contain only letters, digits, space, underscore, dot and hyphen.'
}
if ($WaitSeconds -lt 10 -or $WaitSeconds -gt 580) {
    throw '-WaitSeconds must be between 10 and 580 (Group Policy waits 600 seconds by default).'
}

# SYSTEM runs the script written below at every shutdown, so nobody but SYSTEM, Administrators and
# TrustedInstaller may be able to write the folder it goes in, or any folder above it.
$guardDir = $shutdownDir
while (-not (Test-Path -LiteralPath $guardDir)) { $guardDir = Split-Path -Parent $guardDir }
$offender = Get-GridcoinWriteVector -Path $guardDir -AllowedSids (Get-GridcoinPrivilegedSids) -ChainMasks
if ($offender) {
    throw "Refusing to write a script that SYSTEM runs at shutdown into '$shutdownDir': $offender."
}
foreach ($guardFile in @($cmdPath, $ps1Path)) {
    if (Test-Path -LiteralPath $guardFile) {
        $offender = Get-GridcoinWriteVector -Path $guardFile -AllowedSids (Get-GridcoinPrivilegedSids)
        if ($offender) { throw "Refusing to replace '$guardFile': $offender." }
    }
}

# The shutdown script starts Core-Stop; without it the flush would do nothing.
$stopService = Get-GridcoinTaskService
$stopFolder = $null
$stopTask = $null
try { $stopFolder = $stopService.GetFolder("\$TaskFolder") } catch { $stopFolder = $null }
if ($null -ne $stopFolder) {
    try { $stopTask = $stopFolder.GetTask('Core-Stop') } catch { $stopTask = $null }
}
if ($null -eq $stopTask) {
    throw "The task \$TaskFolder\Core-Stop does not exist. Run Install-GridcoinCoreTask.ps1 first."
}

# The script is the computer's one: it flushes one task folder. Another folder's, whose Core-Stop
# still exists, is not replaced.
if (Test-Path -LiteralPath $ps1Path) {
    $writtenFolder = $null
    foreach ($writtenLine in @(Get-Content -LiteralPath $ps1Path)) {
        if ($writtenLine -match "^\`$flushFolder = '([^']*)'\s*$") { $writtenFolder = $Matches[1] }
    }
    if ($writtenFolder -and $writtenFolder -ine $TaskFolder) {
        $writtenStop = $null
        try { $writtenStop = $stopService.GetFolder("\$writtenFolder").GetTask('Core-Stop') } catch { $writtenStop = $null }
        if ($null -ne $writtenStop) {
            throw ("The shutdown script already flushes the core of \$writtenFolder\, and there is one shutdown script " +
                   "per computer. Run .\Set-GridcoinShutdownFlush.ps1 -Remove, and Uninstall-GridcoinCoreTask.ps1 " +
                   "-TaskFolder '$writtenFolder', first.")
        }
    }
}

# The shutdown script that SYSTEM runs. It parses no file of the wallet user and starts no
# Gridcoin program: the Core-Stop task stops the core as the wallet user, and the script waits for
# the process of the running Core-Start instance, the launcher, which exits only after the core.
$psText = @'
#Requires -Version 5.1
# Gridcoin graceful shutdown flush (Group Policy Computer->Shutdown script; runs as SYSTEM).
# Written by Set-GridcoinShutdownFlush.ps1 -- do not edit by hand.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$flushWait = @WAIT@
$flushFolder = '@FOLDER@'
$service = New-Object -ComObject 'Schedule.Service'
$service.Connect()
$startPath = '\' + $flushFolder + '\Core-Start'
$startPid = 0
$isRunning = $false
foreach ($runningTask in $service.GetRunningTasks(1)) {
    if ([string]$runningTask.Path -eq $startPath) {
        $isRunning = $true
        $startPid = [int]$runningTask.EnginePID
    }
}
if (-not $isRunning) { exit 0 }
$folder = $service.GetFolder('\' + $flushFolder)
$null = $folder.GetTask('Core-Stop').Run($null)
if ($startPid -gt 0) {
    try { Wait-Process -Id $startPid -Timeout $flushWait } catch { Write-Verbose 'the launcher exited, or the wait timed out' }
} else {
    $startTask = $folder.GetTask('Core-Start')
    $deadline = (Get-Date).AddSeconds($flushWait)
    while (([int]$startTask.State -eq 4) -and ((Get-Date) -lt $deadline)) { Start-Sleep -Seconds 2 }
}
exit 0
'@
$psText = $psText.Replace('@WAIT@', [string]$WaitSeconds).Replace('@FOLDER@', $TaskFolder)

# The .cmd that Group Policy runs: one command line, with the full path of every system program.
$cmdLines = @(
    '@echo off',
    'rem BEGIN GENERATED',
    'rem Gridcoin graceful shutdown flush (Group Policy Computer->Shutdown script; runs as SYSTEM).',
    'rem Written by Set-GridcoinShutdownFlush.ps1 -- do not edit by hand.',
    '"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%SystemRoot%\System32\GroupPolicy\Machine\Scripts\Shutdown\GridcoinShutdownFlush.ps1"',
    'rem END GENERATED'
)
if (-not (Test-Path -LiteralPath $shutdownDir)) { New-Item -ItemType Directory -Path $shutdownDir -Force | Out-Null }
Set-Content -LiteralPath $ps1Path -Value $psText -Encoding Ascii
Set-Content -LiteralPath $cmdPath -Value $cmdLines -Encoding Ascii
Write-Host "Wrote shutdown scripts: $cmdPath and $ps1Path"

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
Write-Host "script is ignored at boot -- deploy the .cmd and .ps1 via a domain GPO instead (see README)."
Write-Host ""
Write-Host "VALIDATE: reboot once, then check the core's debug.log for a full graceful shutdown"
Write-Host "sequence ('Shutdown: ... Final flush of wallet database ...') just before the restart."
Write-Host "Remove with:  .\Set-GridcoinShutdownFlush.ps1 -Remove"
