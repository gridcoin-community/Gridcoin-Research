#Requires -Version 5.1
<#
.SYNOPSIS
    Register the Gridcoin multiprocess core as background Scheduled Tasks, run as your own account.

.DESCRIPTION
    Registers two single-purpose tasks under the \Gridcoin\ folder. Both run as the account that
    runs this script (never SYSTEM: the IPC socket and cookie are owner-only, and a SYSTEM core
    would lock your own GUI out). They are registered with the S4U logon, so no password is asked
    for or stored anywhere.

      Core-Start : boot trigger -> a generated launcher (core-autostart.cmd) that runs
                   gridcoinresearchd.exe -multiprocess -autounlockblob and retries once, after
                   2 minutes, when the core exits non-zero.
      Core-Stop  : gridcoinresearchd.exe stop (a graceful RPC stop that flushes the databases).
                   On demand only: to stop the core, START this task. Never end Core-Start, which
                   is a hard kill.

    When Windows shuts down, the core stops itself on Windows' shutdown signal and flushes its
    databases; there is nothing more to set up.

    It adds an inbound Windows Firewall rule for the daemon (skip with -SkipFirewallRule): the
    headless core gets no firewall prompt, and in multiprocess mode the core does the P2P.

    Switch-over from an earlier version: the tasks an earlier version registered are
    unregistered, the resident autounlock helper is ended and removed, and its passphrase file is
    converted into the file the core reads itself (skip with -NoAutounlockMigration).

    One installation registers one pair of tasks: they share its launcher.
    -TaskFolder names another folder for that pair; the script refuses while the pair is already
    registered in another folder (Uninstall-GridcoinCoreTask.ps1 -TaskFolder <that folder> first).
    A second wallet runs in the service mode instead.

    The tasks are for the account that runs the script. It refuses when another account is signed in
    to the session, as when an administrator's credentials were given for a standard account: that
    account's wallet uses the service mode.

.NOTES
    Run from an elevated console: Windows lets only administrators create a start-at-boot task.
    The scripts must sit where only administrators can write (for example Program Files).
#>
[CmdletBinding()]
param(
    # Path to gridcoinresearchd.exe. Default: the installer's ...\daemon\ layout, else the folder
    # above these scripts.
    [string]$CorePath,

    # The datadir the core owns. Default: %APPDATA%\GridcoinResearch of this account.
    [string]$DataDir,

    # The task folder of this installation's one pair of tasks (see the description).
    [string]$TaskFolder = 'Gridcoin',

    # Skip the inbound firewall rule for the daemon.
    [switch]$SkipFirewallRule,

    # Firewall profiles for the inbound rule. Public is left out on purpose.
    [ValidateSet('Domain', 'Private', 'Public')]
    [string[]]$FirewallProfile = @('Domain', 'Private'),

    # Do not convert the old autounlock passphrase file (the old file is then left in place).
    [switch]$NoAutounlockMigration
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# An elevated run loads modules only from the system's own module directories.
$env:PSModulePath = "$PSHOME\Modules;$env:ProgramFiles\WindowsPowerShell\Modules"

. (Join-Path $PSScriptRoot 'GridcoinCommon.ps1')

if (-not (Test-GridcoinElevated)) {
    throw ("Task mode needs your own account to be an administrator, run from an elevated console " +
           "(Windows lets only administrators create a start-at-boot task, and registering a task for " +
           "another account would need that account's password). For a standard-user wallet, ask an " +
           "administrator to set up the service mode: gridcoin-service.exe install, then enable.")
}
$mySid = Get-GridcoinCurrentSid

# The tasks are this account's. An elevated console opened with another administrator's credentials,
# from a session someone else is signed in to, would register them for that administrator and leave
# the signed-in account's wallet without them.
$sessionUser = Get-GridcoinSessionUserSid
if ($sessionUser -and $sessionUser -ne $mySid) {
    throw ("This console runs as $(Get-GridcoinAccountName $mySid), but $(Get-GridcoinAccountName $sessionUser) " +
           "is signed in to this session, so the tasks would start $(Get-GridcoinAccountName $mySid)'s wallet, not " +
           "the signed-in account's. Task mode is for an administrator's own wallet: run this signed in as the " +
           "administrator whose wallet it is. For a standard account's wallet, ask an administrator to set up " +
           "the service mode: gridcoin-service.exe install, then gridcoin-service.exe enable " +
           "--user=`"$(Get-GridcoinAccountName $sessionUser)`".")
}

if (-not $CorePath) {
    $CorePath = @(
        (Join-Path $PSScriptRoot '..\daemon\gridcoinresearchd.exe'),
        (Join-Path $PSScriptRoot '..\gridcoinresearchd.exe')
    ) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (-not $CorePath) { $CorePath = (Join-Path $PSScriptRoot '..\daemon\gridcoinresearchd.exe') }
}
if (-not (Test-Path -LiteralPath $CorePath)) {
    throw "gridcoinresearchd.exe not found at '$CorePath'. Pass -CorePath explicitly."
}
$CorePath = (Resolve-Path -LiteralPath $CorePath).Path

if (-not $DataDir) { $DataDir = Join-Path $env:APPDATA 'GridcoinResearch' }
if ($DataDir -match '"') {
    throw '-DataDir must not contain a double-quote character.'
}
if ($DataDir -match '%') {
    throw '-DataDir must not contain a percent character (it is written into a batch file).'
}
if ($CorePath -match '["%]') {
    throw '-CorePath must not contain a double-quote or percent character (it is written into a batch file).'
}
if ($TaskFolder -notmatch '^[A-Za-z0-9 _.-]+$') {
    throw '-TaskFolder may contain only letters, digits, space, underscore, dot and hyphen.'
}
# A trailing backslash escapes the closing quote when a command line is split, so -datadir="C:\dd\"
# swallows the quote and corrupts every argument after it. Reject it instead of escaping it for
# the parsers of cmd.exe, the task and the core at once.
if ($DataDir -match '\\$') {
    throw ("-DataDir must not end with a backslash ('$DataDir'): it would escape the closing quote when " +
           "the command line is parsed. Drop the trailing backslash (use a subdirectory, not a drive root).")
}

# The service mode and this mode cannot run one core each on the same wallet.
$mainService = Get-Service -Name 'gridcoinresearchd' -ErrorAction SilentlyContinue
if ($null -ne $mainService -and [string]$mainService.StartType -ne 'Disabled') {
    throw ("The Windows service 'gridcoinresearchd' is installed and not disabled. Task mode and service " +
           "mode cannot both run. Run 'gridcoin-service.exe disable' first, or use the service.")
}

# Another account's boot task is not ours to replace.
$taskService = Get-GridcoinTaskService
$existingFolder = $null
try { $existingFolder = $taskService.GetFolder("\$TaskFolder") } catch { $existingFolder = $null }
if ($null -ne $existingFolder) {
    $existingStart = $null
    try { $existingStart = $existingFolder.GetTask('Core-Start') } catch { $existingStart = $null }
    if ($null -ne $existingStart) {
        $existingSid = Get-GridcoinTaskPrincipalSid $existingStart
        if ($existingSid -ne $mySid) {
            throw ("\$TaskFolder\Core-Start already exists and runs as another account ($existingSid). " +
                   "That account (or an administrator) must run Uninstall-GridcoinCoreTask.ps1 first.")
        }
    }
}

# SECURITY: Core-Start runs the generated launcher, then the core, at boot as this account with
# nobody watching. Anyone who can replace the launcher, the core or cmd.exe, or anything on the way
# to them, gets code run as this account, which owns the wallet and the autounlock file. Fail
# closed unless every one of them can be written only by SYSTEM, Administrators, TrustedInstaller
# and, where it is this account's own, this account.
$privileged = @(Get-GridcoinPrivilegedSids)
$ownAndPrivileged = $privileged + @($mySid)

$launcherPath = Get-GridcoinLauncherPath

# One pair of tasks per installation: they share the launcher.
$otherFolders = @(Get-GridcoinLauncherFolders -LauncherPath $launcherPath -ExceptFolder $TaskFolder)
if ($otherFolders.Count -gt 0) {
    throw ("The tasks of this installation are already registered in \$($otherFolders[0])\, and one installation " +
           "registers one pair: they share the launcher '$launcherPath'. Run " +
           "Uninstall-GridcoinCoreTask.ps1 -TaskFolder '$($otherFolders[0])' first, or run a second wallet in the " +
           "service mode.")
}
$launcherGuard = $launcherPath
if (-not (Test-Path -LiteralPath $launcherGuard)) { $launcherGuard = $PSScriptRoot }
$offender = Get-GridcoinWriteVector -Path $launcherGuard -AllowedSids $ownAndPrivileged -ChainMasks
if ($offender) {
    throw ("Refusing to register a boot task that runs '$launcherPath': $offender. Install these scripts " +
           "under an administrator-only location (for example Program Files), with every parent directory " +
           "owned by an administrator, and run them from there.")
}
$offender = Get-GridcoinWriteVector -Path $CorePath -AllowedSids $ownAndPrivileged -ChainMasks
if ($offender) {
    throw ("Refusing to register a boot task that runs '$CorePath': $offender. Install the core under an " +
           "administrator-only location (for example Program Files).")
}
foreach ($systemTool in @('cmd.exe', 'PING.EXE')) {
    $toolPath = Join-Path $env:SystemRoot "System32\$systemTool"
    $offender = Get-GridcoinWriteVector -Path $toolPath -AllowedSids $privileged
    if ($offender) { throw "Refusing to register a boot task that runs '$toolPath': $offender." }
}
if (Test-Path -LiteralPath $DataDir) {
    $offender = Get-GridcoinWriteVector -Path $DataDir -AllowedSids $ownAndPrivileged
    if ($offender) { throw "Refusing to use the datadir '$DataDir': $offender." }
    $confPath = Join-Path $DataDir 'gridcoinresearch.conf'
    if (Test-Path -LiteralPath $confPath) {
        $offender = Get-GridcoinWriteVector -Path $confPath -AllowedSids $ownAndPrivileged
        if ($offender) { throw "Refusing to use the configuration file '$confPath': $offender." }
    }
}

# Switch-over from an earlier version. The tasks with a stored password go first, then the
# resident autounlock helper, then its passphrase file becomes the new one.
$removedTasks = @(Remove-GridcoinPasswordLogonTasks -TaskFolder $TaskFolder)
foreach ($removedName in $removedTasks) {
    Write-Host "Removed the old \$TaskFolder\$removedName, which an earlier version registered."
}
$oldBlob = Remove-GridcoinLegacyAutounlock -DataDir $DataDir -TaskFolder $TaskFolder
if ($oldBlob) {
    if ($NoAutounlockMigration) {
        Write-Host "Left the old autounlock file '$oldBlob' in place (-NoAutounlockMigration). The core does not read it." -ForegroundColor Yellow
    } else {
        $networkName = Get-GridcoinConfNetwork -DataDir $DataDir
        # A failed conversion must never leave the user without a boot task: warn and carry on.
        try {
            if (Convert-GridcoinLegacyAutounlockBlob -OldPath $oldBlob -DataDir $DataDir -Network $networkName) {
                Write-Host "Converted the autounlock passphrase file for the $networkName network. The old file and its logs are deleted."
            }
        } catch {
            Write-Host ("WARNING: could not convert the old autounlock file '$oldBlob' ($($_.Exception.Message)). " +
                        "It is left in place and the core does not read it. Set autounlock up again in the wallet (setautounlock).") -ForegroundColor Yellow
        }
    }
}

Write-Host "Registering \$TaskFolder\Core-Start and \$TaskFolder\Core-Stop for $(Get-GridcoinAccountName $mySid), with the data directory $DataDir."
$launcher = Register-GridcoinCoreTasks -CorePath $CorePath -DataDir $DataDir -TaskFolder $TaskFolder

# Inbound firewall rule for the daemon. Program based, so it covers both networks. A failure warns
# and does not undo the tasks.
$fwName = 'Gridcoin multiprocess core (gridcoinresearchd)'
if ($SkipFirewallRule) {
    Write-Host "Skipped the inbound firewall rule (-SkipFirewallRule). The core is outbound-only until you add one for '$CorePath'." -ForegroundColor Yellow
} else {
    try {
        Get-NetFirewallRule -DisplayName $fwName -ErrorAction SilentlyContinue | Remove-NetFirewallRule -ErrorAction SilentlyContinue
        New-NetFirewallRule -DisplayName $fwName -Direction Inbound -Action Allow -Program $CorePath `
            -Protocol TCP -Profile $FirewallProfile -Enabled True `
            -Description 'Allow inbound P2P connections to the Gridcoin multiprocess core (gridcoinresearchd.exe).' | Out-Null
        Write-Host "Added inbound firewall rule '$fwName' for $CorePath (profiles: $($FirewallProfile -join ', '))." -ForegroundColor Green
    } catch {
        Write-Host ("WARNING: could not add the inbound firewall rule ($($_.Exception.Message)). The core is " +
                    "outbound-only until you add an inbound rule for '$CorePath'.") -ForegroundColor Yellow
    }
}

Write-Host "Registered \$TaskFolder\Core-Start (boot, through $launcher) and \$TaskFolder\Core-Stop (on demand), S4U, as SID $mySid." -ForegroundColor Green
Write-Host ""
Write-Host "  Start the core now :  Start-ScheduledTask -TaskPath '\$TaskFolder\' -TaskName 'Core-Start'"
Write-Host "  Stop the core      :  Start-ScheduledTask -TaskPath '\$TaskFolder\' -TaskName 'Core-Stop'   (graceful; never end Core-Start)"
Write-Host "  Attach the GUI     :  .\Start-GridcoinGui.ps1 -DataDir '$DataDir'   (from a normal, not elevated, console)"
