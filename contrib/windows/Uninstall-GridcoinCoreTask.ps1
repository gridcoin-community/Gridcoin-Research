#Requires -Version 5.1
<#
.SYNOPSIS
    Remove the Gridcoin core Scheduled Tasks registered by Install-GridcoinCoreTask.ps1.

.DESCRIPTION
    Unregisters \Gridcoin\Core-Start and \Gridcoin\Core-Stop, removes the inbound
    firewall rule Install-GridcoinCoreTask.ps1 created for the daemon, and removes the
    (now empty) \Gridcoin task folder. Tolerant of already-absent items (idempotent).

    Does NOT remove the opt-in autounlock task (\Gridcoin\Autounlock) -- that is owned by
    Set-GridcoinAutounlock.ps1 (`-Remove`) -- nor the opt-in shutdown-flush Group Policy
    script -- owned by Set-GridcoinShutdownFlush.ps1 (`-Remove`). It also does not stop a
    running core; run the Core-Stop task first if you want a graceful stop.
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

# Remove the folder only if it is now empty (leaves \Gridcoin alone if the Plan-5
# Autounlock task or anything else still lives there). Enumerate via the COM folder
# object -- Get-ScheduledTask's -TaskPath does not reliably support wildcards, and a
# false "empty" result would wrongly delete the folder and its remaining tasks.
try {
    $service = New-Object -ComObject 'Schedule.Service'
    $service.Connect()
    $folder = $service.GetFolder("\$TaskFolder")
    $remaining = @($folder.GetTasks(1))   # 1 = TASK_ENUM_HIDDEN (count hidden too)
    if ($remaining.Count -eq 0) {
        $service.GetFolder('\').DeleteFolder($TaskFolder, 0)
        Write-Host "Removed empty task folder \$TaskFolder"
    } else {
        Write-Host "Task folder \$TaskFolder still has $($remaining.Count) task(s); left in place."
    }
} catch {
    Write-Host "Task folder \$TaskFolder not removed ($($_.Exception.Message)); left in place."
}
