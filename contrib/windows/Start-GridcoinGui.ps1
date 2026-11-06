#Requires -Version 5.1
<#
.SYNOPSIS
    Launch the Gridcoin -multiprocess GUI, attaching to the running core.

.DESCRIPTION
    Task mode (the default): starts gridcoinresearch.exe -multiprocess -datadir=<dd>, which
    connects to the core that the Core-Start task runs for the same account on the same datadir.

    Service mode (-Service [-Network main|test]): starts the GUI against the Windows service that
    gridcoin-service.exe set up, which runs the core under its own account. The GUI finds the
    core's socket in %ProgramData%\Gridcoin\ipc\<network> and checks that the core runs as
    NT SERVICE\gridcoinresearchd (or gridcointestnetd). Your account must be a member of the
    "Gridcoin GUI Users" ("Gridcoin Testnet GUI Users") group, which the service setup prints.

    With -CreateShortcut it writes a per-user Start Menu shortcut with the same arguments instead.

    The GUI refuses to run as an administrator in multiprocess mode, and so does this script: start
    it from a normal, not elevated, session. A different Windows user cannot attach in task mode:
    the core's socket and cookie are owner-only. If no core is running, the GUI shows the
    connect-failure dialog; it does not start a core.
#>
[CmdletBinding()]
param(
    [string]$GuiPath = (Join-Path $PSScriptRoot '..\gridcoinresearch.exe'),
    [string]$DataDir = (Join-Path $env:APPDATA 'GridcoinResearch'),
    [switch]$Service,
    [ValidateSet('main', 'test')]
    [string]$Network = 'main',
    [switch]$CreateShortcut
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'GridcoinCommon.ps1')

if (Test-GridcoinElevated) {
    throw ("The Gridcoin wallet must not run as administrator in multiprocess mode. Start it from a normal " +
           "(not elevated) session. This happens only when Gridcoin could not give up the administrator rights it was started with.")
}

if ($PSBoundParameters.ContainsKey('Network') -and -not $Service) {
    throw '-Network is for the service mode: use it with -Service.'
}

if (-not (Test-Path -LiteralPath $GuiPath)) {
    throw "gridcoinresearch.exe not found at '$GuiPath'. Pass -GuiPath explicitly."
}
$GuiPath = (Resolve-Path -LiteralPath $GuiPath).Path

if ($DataDir -match '"') {
    throw '-DataDir must not contain a double-quote character.'
}
# A trailing backslash escapes the closing quote when the command line is split, which corrupts
# every argument after it.
if ($DataDir -match '\\$') {
    throw "-DataDir must not end with a backslash ('$DataDir'): it would escape the closing quote of the argument."
}

# One argument string, used for the direct launch and for the shortcut.
if ($Service) {
    $serviceName = 'gridcoinresearchd'
    $ipcLeaf = 'mainnet'
    if ($Network -eq 'test') {
        $serviceName = 'gridcointestnetd'
        $ipcLeaf = 'testnet'
    }
    $ipcDir = Join-Path $env:ProgramData "Gridcoin\ipc\$ipcLeaf"
    $arguments = '-multiprocess -ipcdir="' + $ipcDir + '" -ipcserviceuser="NT SERVICE\' + $serviceName + '"'
    if ($Network -eq 'test') { $arguments = $arguments + ' -testnet' }
    $where = "the $Network network service ($serviceName)"
} else {
    $arguments = '-multiprocess -datadir="' + $DataDir + '"'
    $where = "datadir '$DataDir'"
}

if ($CreateShortcut) {
    $programs = [Environment]::GetFolderPath('Programs')
    $linkName = 'Gridcoin (multiprocess).lnk'
    if ($Service) { $linkName = "Gridcoin (service, $Network).lnk" }
    $linkPath = Join-Path $programs $linkName
    $shell = New-Object -ComObject 'WScript.Shell'
    $shortcut = $shell.CreateShortcut($linkPath)
    $shortcut.TargetPath = $GuiPath
    $shortcut.Arguments = $arguments
    $shortcut.WorkingDirectory = (Split-Path -Parent $GuiPath)
    $shortcut.Description = 'Gridcoin GUI attaching to the running multiprocess core'
    $shortcut.IconLocation = $GuiPath
    $shortcut.Save()
    Write-Host "Created Start-Menu shortcut: $linkPath" -ForegroundColor Green
    return
}

# Launch detached; do not block this shell on the GUI.
Start-Process -FilePath $GuiPath -ArgumentList $arguments -WorkingDirectory (Split-Path -Parent $GuiPath)
Write-Host "Launched the Gridcoin GUI (-multiprocess) on $where."
