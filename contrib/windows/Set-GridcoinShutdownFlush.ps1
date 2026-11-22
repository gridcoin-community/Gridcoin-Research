#Requires -Version 5.1
<#
.SYNOPSIS
    Remove the Group Policy shutdown script that earlier versions of Gridcoin set up to stop the
    multiprocess core when Windows shuts down. It sets up nothing any more.

.DESCRIPTION
    Earlier versions offered a Group Policy Computer->Shutdown script that started the Core-Stop
    task and waited for the core. It cannot work: by the time Windows runs shutdown scripts, Task
    Scheduler has ended the running Core-Start instance and refuses to start any task (Core-Stop
    fails with ERROR_SHUTDOWN_IN_PROGRESS). The core stops itself instead: Windows sends running
    programs a shutdown signal, and the core flushes its databases and exits. With Fast Startup,
    Shut down hibernates it with the system session and it carries on at the next start.

    -Remove takes the earlier script out of the LOCAL Group Policy machine shutdown scripts
    (scripts.ini, gpt.ini and the run-time state), deletes its two files from the machine shutdown
    script folder, and refreshes the computer policy. Without -Remove the script says so and changes
    nothing. The installer and Uninstall-GridcoinCoreTask.ps1 -All run -Remove.

.NOTES
    Requires an ELEVATED PowerShell (writes under %SystemRoot%\System32\GroupPolicy and HKLM).
#>
[CmdletBinding()]
param(
    # Accepted from earlier command lines, and unused.
    [int]$WaitSeconds = 300,
    [string]$TaskFolder = 'Gridcoin',

    [switch]$Remove
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# An elevated run loads modules only from the system's own module directories.
$env:PSModulePath = "$PSHOME\Modules;$env:ProgramFiles\WindowsPowerShell\Modules"

. (Join-Path $PSScriptRoot 'GridcoinCommon.ps1')

# --- without -Remove --------------------------------------------------------------------
if (-not $Remove) {
    Write-Host ("Nothing to set up: the Gridcoin core stops itself, flushing its databases, when Windows sends running " +
                "programs its shutdown signal, and a Group Policy shutdown script cannot start the Core-Stop task. " +
                "Run with -Remove to take out a shutdown script an earlier version set up.")
    return
}

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
    # gpupdate. Version packs the user version in the HIGH 16 bits and the computer version in the
    # LOW 16 bits. Shutdown scripts are Computer Configuration, so the low word is bumped (from
    # 0xFFFF back to 1, never to 0): a computer version of 0 makes Group Policy treat the local
    # GPO's computer half as empty, and it then never runs the script.
    $lines = Read-IniLines $gptIni
    if ($lines.Count -eq 0) { $lines = @('[General]', 'Version=0') }
    $hasVer = $false; $hasCse = $false
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match '^\s*Version\s*=\s*(\d+)\s*$') {
            $version = [long]$Matches[1]
            $userVersion = [long][Math]::Floor($version / 65536)
            $computerVersion = $version % 65536
            $lines[$i] = 'Version=' + ($userVersion * 65536 + ($computerVersion % 65535) + 1); $hasVer = $true
        }
        if ($lines[$i] -match '^\s*gPCMachineExtensionNames\s*=') {
            if ($lines[$i] -notmatch '42B5FAAE-6536-11D2-AE5A-0000F87571E3') {
                $lines[$i] = $lines[$i].TrimEnd() + $scriptsCse
            }
            $hasCse = $true
        }
    }
    if (-not $hasVer) { $lines += 'Version=1' }
    if (-not $hasCse) { $lines += ('gPCMachineExtensionNames=' + $scriptsCse) }
    Write-IniLines $gptIni $lines
}

# --- run-time State ---------------------------------------------------------------------
# Where earlier versions recorded the script for the local GPO, in gpedit's layout:
#   State\...\Shutdown\<gpo>       GPO-ID=LocalGPO SOM-ID=Local FileSysPath=<...\Machine>
#                                  DisplayName/GPOName='Local Group Policy' PSScriptOrder=<count>
#   State\...\Shutdown\<gpo>\<n>   Script=<full .cmd path> Parameters='' ExecTime=0 (QWORD)
$stateBase = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Group Policy\State\Machine\Scripts\Shutdown'

# Group Policy runs the shutdown scripts from the keys its own processing writes, at the next
# policy refresh or start; until then a shutdown would run the old list. With the computer version
# just bumped, a refresh of the computer policy picks the change up now.
function Update-ComputerPolicy {
    & (Join-Path $env:SystemRoot 'System32\gpupdate.exe') /target:computer | Out-Host
    if ($LASTEXITCODE -ne 0) {
        Write-Host ("gpupdate /target:computer exited with $LASTEXITCODE; the change takes effect at the next policy " +
                    "refresh or restart.") -ForegroundColor Yellow
    }
}

function Read-RegString([string]$path, [string]$name) {
    $i = Get-ItemProperty -LiteralPath $path -Name $name -EA SilentlyContinue
    if ($i) { return [string]$i.$name }
    return $null
}
# Index of the GPO subkey whose GPO-ID = LocalGPO, or $null.
function Get-LocalGpoStateIndex {
    $found = $null
    if (Test-Path -LiteralPath $stateBase) {
        foreach ($g in Get-ChildItem -LiteralPath $stateBase -EA SilentlyContinue) {
            if ($g.PSChildName -notmatch '^\d+$') { continue }
            $n = [int]$g.PSChildName
            if ((Read-RegString $g.PSPath 'GPO-ID') -eq 'LocalGPO') { $found = $n }
        }
    }
    return $found
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
    $refresh = $false
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
            $refresh = $true
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
    if ($refresh) { Update-ComputerPolicy }
    return
}
