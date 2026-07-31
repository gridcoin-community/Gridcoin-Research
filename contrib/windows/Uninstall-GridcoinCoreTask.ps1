#Requires -Version 5.1
<#
.SYNOPSIS
    Remove the Gridcoin core Scheduled Tasks registered by Install-GridcoinCoreTask.ps1.

.DESCRIPTION
    Unregisters \Gridcoin\Core-Start and \Gridcoin\Core-Stop and removes the (now
    empty) \Gridcoin task folder. Tolerant of already-absent tasks (idempotent).

    Does NOT remove the opt-in autounlock task (\Gridcoin\Autounlock) -- that is
    owned by Set-GridcoinAutounlock.ps1 / its uninstall (Plan 5). It also does not
    stop a running core; run the Core-Stop task first if you want a graceful stop.
#>
[CmdletBinding()]
param([string]$TaskFolder = 'Gridcoin')

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

foreach ($name in 'Core-Start', 'Core-Stop') {
    $existing = Get-ScheduledTask -TaskPath "\$TaskFolder\" -TaskName $name -ErrorAction SilentlyContinue
    if ($existing) {
        Unregister-ScheduledTask -TaskPath "\$TaskFolder\" -TaskName $name -Confirm:$false
        Write-Host "Removed \$TaskFolder\$name"
    } else {
        Write-Host "\$TaskFolder\$name not present (ok)"
    }
}

# Remove the folder only if it is now empty (leaves \Gridcoin alone if the
# Autounlock task or anything else still lives there). Schedule.Service COM API,
# since the ScheduledTasks module has no Remove-ScheduledTaskFolder.
try {
    $remaining = Get-ScheduledTask -TaskPath "\$TaskFolder\*" -ErrorAction SilentlyContinue
    if (-not $remaining) {
        $service = New-Object -ComObject 'Schedule.Service'
        $service.Connect()
        $service.GetFolder('\').DeleteFolder($TaskFolder, 0)
        Write-Host "Removed empty task folder \$TaskFolder"
    } else {
        Write-Host "Task folder \$TaskFolder still has other tasks; left in place."
    }
} catch {
    Write-Host "Task folder \$TaskFolder not removed ($($_.Exception.Message)); left in place."
}
