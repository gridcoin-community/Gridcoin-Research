#Requires -Version 5.1
<#
.SYNOPSIS
    Launch the Gridcoin -multiprocess GUI, attaching to the running core.

.DESCRIPTION
    Starts gridcoinresearch.exe -multiprocess -datadir=<dd>, which connects to the
    core that Core-Start is running on the same datadir. With -CreateShortcut it
    instead writes a per-user Start-Menu shortcut with the same arguments.

    Single-user by design (not a bug to fix): the core's node.sock/ipc.cookie are
    owner-only NTFS-ACL'd (#3234) and a per-connection SO_PEERCRED/peer-UID check
    (#3242) enforces same-user, so only the wallet user can attach. A DIFFERENT
    Windows user gets "could not connect to the daemon" -- that is the ACL working.

    If no core is running, the GUI shows the same connect-failure dialog (the
    datadir chooser is suppressed in -multiprocess, Plan 1); it will not start a
    core for you.
#>
[CmdletBinding()]
param(
    [string]$GuiPath = (Join-Path $PSScriptRoot '..\gridcoinresearch.exe'),
    [string]$DataDir = (Join-Path $env:APPDATA 'GridcoinResearch'),
    [switch]$CreateShortcut
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (-not (Test-Path -LiteralPath $GuiPath)) {
    throw "gridcoinresearch.exe not found at '$GuiPath'. Pass -GuiPath explicitly."
}
$GuiPath = (Resolve-Path -LiteralPath $GuiPath).Path

# One arg string reused for both the direct launch and the shortcut.
$ddQuoted = '"' + $DataDir + '"'
$arguments = "-multiprocess -datadir=$ddQuoted"

if ($CreateShortcut) {
    $programs = [Environment]::GetFolderPath('Programs')   # per-user Start Menu\Programs
    $lnkPath = Join-Path $programs 'Gridcoin (multiprocess).lnk'
    $shell = New-Object -ComObject 'WScript.Shell'
    $shortcut = $shell.CreateShortcut($lnkPath)
    $shortcut.TargetPath = $GuiPath
    $shortcut.Arguments = $arguments
    $shortcut.WorkingDirectory = (Split-Path -Parent $GuiPath)
    $shortcut.Description = 'Gridcoin GUI attaching to the running multiprocess core'
    $shortcut.IconLocation = $GuiPath
    $shortcut.Save()
    Write-Host "Created Start-Menu shortcut: $lnkPath" -ForegroundColor Green
    return
}

# Launch detached; do not block this shell on the GUI.
Start-Process -FilePath $GuiPath -ArgumentList $arguments -WorkingDirectory (Split-Path -Parent $GuiPath)
Write-Host "Launched the Gridcoin GUI (-multiprocess) on datadir '$DataDir'."
