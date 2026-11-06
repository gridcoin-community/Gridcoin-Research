#Requires -Version 5.1
<#
.SYNOPSIS
    Remove the Gridcoin core Scheduled Tasks registered by Install-GridcoinCoreTask.ps1.

.DESCRIPTION
    Unregisters \Gridcoin\Core-Start and \Gridcoin\Core-Stop, removes the Core-Start launcher
    (core-autostart.cmd) and the inbound firewall rule Install-GridcoinCoreTask.ps1 created for the
    daemon, and removes the (now empty) \Gridcoin task folder. Tolerant of already-absent items
    (idempotent).

    With -All it also removes the Group Policy shutdown script (Set-GridcoinShutdownFlush.ps1
    -Remove) and the resident autounlock helper and task that earlier versions left (the old
    passphrase file is reported, not deleted). Needs an elevated console.

    It never deletes a wallet file or the autounlock file the core reads, and it does not stop a
    running core: run the Core-Stop task first for a graceful stop.
#>
[CmdletBinding()]
param(
    [string]$TaskFolder = 'Gridcoin',

    # The datadir whose old autounlock files are looked for (-All only).
    [string]$DataDir = (Join-Path $env:APPDATA 'GridcoinResearch'),

    [switch]$All
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'GridcoinCommon.ps1')

if ($All) {
    if (-not (Test-GridcoinElevated)) {
        throw '-All needs an elevated console (it changes Group Policy and ends the old helper).'
    }
    & (Join-Path $PSScriptRoot 'Set-GridcoinShutdownFlush.ps1') -Remove -TaskFolder $TaskFolder
    $oldBlob = Remove-GridcoinLegacyAutounlock -DataDir $DataDir -TaskFolder $TaskFolder
    if ($oldBlob) {
        Write-Host "An old autounlock file remains at '$oldBlob'; the core does not read it. Delete it when you no longer need it." -ForegroundColor Yellow
    }
}

foreach ($name in 'Core-Start', 'Core-Stop') {
    $existing = Get-ScheduledTask -TaskPath "\$TaskFolder\" -TaskName $name -ErrorAction SilentlyContinue
    if ($existing) {
        Unregister-ScheduledTask -TaskPath "\$TaskFolder\" -TaskName $name -Confirm:$false
        Write-Host "Removed \$TaskFolder\$name"
    } else {
        Write-Host "\$TaskFolder\$name not present (ok)"
    }
}

# Remove the Core-Start launcher Install-GridcoinCoreTask.ps1 generated (idempotent).
$launcher = Join-Path $PSScriptRoot 'core-autostart.cmd'
if (Test-Path -LiteralPath $launcher) {
    Remove-Item -LiteralPath $launcher -Force
    Write-Host "Removed the Core-Start launcher ($launcher)."
}

# Remove the inbound firewall rule Install-GridcoinCoreTask.ps1 added for the daemon (idempotent).
try {
    $fw = @(Get-NetFirewallRule -DisplayName 'Gridcoin multiprocess core (gridcoinresearchd)' -ErrorAction SilentlyContinue)
    if ($fw.Count -gt 0) {
        $fw | Remove-NetFirewallRule
        Write-Host "Removed the daemon inbound firewall rule."
    } else {
        Write-Host "Daemon inbound firewall rule not present (ok)."
    }
} catch {
    Write-Host "Could not remove the daemon firewall rule ($($_.Exception.Message)); left in place."
}

# Remove the folder only if it is now empty. Enumerate through the COM folder object, hidden
# tasks included: a false "empty" would delete the folder and the tasks still in it.
try {
    $taskService = Get-GridcoinTaskService
    $folder = $taskService.GetFolder("\$TaskFolder")
    $remaining = @($folder.GetTasks(1))
    if ($remaining.Count -eq 0) {
        $taskService.GetFolder('\').DeleteFolder($TaskFolder, 0)
        Write-Host "Removed empty task folder \$TaskFolder"
    } else {
        Write-Host "Task folder \$TaskFolder still has $($remaining.Count) task(s); left in place."
    }
} catch {
    Write-Host "Task folder \$TaskFolder not removed ($($_.Exception.Message)); left in place."
}
