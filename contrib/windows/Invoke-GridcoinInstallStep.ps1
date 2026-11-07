#Requires -Version 5.1
<#
.SYNOPSIS
    One step of the Windows installer around replacing the Gridcoin files (NSIS.template.in).

.DESCRIPTION
    The installer runs, elevated and in 64-bit Windows PowerShell, in this order:

      HardenInstallDir  before any file is replaced, for an install directory outside Program
                        Files: gives it the owner Administrators and a protected access list
                        (Administrators and SYSTEM full control, Users read and execute), so that
                        only administrators can change the programs in it. Refuses a directory
                        that another account owns, a link, a drive, a network path, and an
                        existing directory with other files than an earlier install.
      StopCore          before any file is replaced: stops each Gridcoin service that runs, then the
                        core of the \Gridcoin\Core-Start task (gracefully, through Core-Stop), and
                        refuses while a core started by hand from <InstallDir>\daemon still runs.
                        What was running is written to -StateFile. On a refusal, what was stopped
                        is started again.
      SwitchOver        after the files are installed: registers again with the S4U logon
                        the \Gridcoin\ tasks of this account that earlier versions registered,
                        and carries this account's autounlock file over. Another account's
                        such tasks are removed, and UPGRADE-NOTICE.txt says how that account
                        restores unattended start. Ends the earlier versions' autounlock helper and
                        task, replaces their shutdown script, and deletes their helper scripts and release-keys.txt.
      StartCore         starts again what StopCore found running.
      ReportPermissions runs the new core's -reportpermissions over this account's data
                        directories, and writes what it finds into UPGRADE-NOTICE.txt. It changes
                        nothing.
      UninstallCheck    before uninstalling: exit 3 while service mode is on.
      PreUninstall      before any file is deleted: StopCore, Uninstall-GridcoinCoreTask.ps1 -All,
                        and gridcoin-service.exe uninstall for each service that is installed.

    Exit codes: 0 done; 1 failed, the reason printed; 3 (UninstallCheck) service mode is on.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('StopCore', 'HardenInstallDir', 'SwitchOver', 'StartCore', 'ReportPermissions', 'UninstallCheck', 'PreUninstall')]
    [string]$Step,

    # The installation directory, $INSTDIR.
    [Parameter(Mandatory = $true)][string]$InstallDir,

    # Where StopCore records what was running, for StartCore.
    [string]$StateFile
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'GridcoinCommon.ps1')

# The access list of an install directory outside Program Files: owner Administrators; protected;
# Administrators and SYSTEM full control, Users read and execute (0x1200a9, which holds
# READ_CONTROL and FILE_READ_ATTRIBUTES: the service reads the access list of every directory above
# its program through Users); each inherited by files and directories.
$script:InstallDirSddl = 'O:BAD:PAI(A;OICI;FA;;;BA)(A;OICI;FA;;;SY)(A;OICI;0x1200a9;;;BU)'

$script:ProgramDataDir = [Environment]::GetFolderPath('CommonApplicationData')

# The services gridcoin-service.exe installs, and the directories that hold their wallets
# (src/winsvc/layout.h, NetDataDir).
$script:GridcoinServices = @(
    @{ Name = 'gridcoinresearchd'; Network = 'main'; NetDataDir = (Join-Path $script:ProgramDataDir 'Gridcoin\mainnet') },
    @{ Name = 'gridcointestnetd'; Network = 'test'; NetDataDir = (Join-Path $script:ProgramDataDir 'Gridcoin\testnet\testnet') }
)

# Set from -InstallDir before the step runs.
$script:InstallRoot = $null
$script:WindowsDir = $null
$script:CorePath = $null
$script:StartedServices = @()
$script:StartedTaskSid = ''
$script:NoticePath = $null

# Runs a program and returns its exit code and the lines it wrote, standard error after standard
# output. The arguments go to the program as written.
function Invoke-GridcoinProgram {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [string]$ArgumentText = ''
    )
    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $FilePath
    $startInfo.Arguments = $ArgumentText
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.StandardOutputEncoding = New-Object System.Text.UTF8Encoding($false)
    $startInfo.StandardErrorEncoding = New-Object System.Text.UTF8Encoding($false)
    $process = [System.Diagnostics.Process]::Start($startInfo)
    $errorRead = $process.StandardError.ReadToEndAsync()
    $outputText = $process.StandardOutput.ReadToEnd()
    $process.WaitForExit()
    $errorText = $errorRead.Result
    $outputLines = @(($outputText + "`n" + $errorText) -split "`r?`n" | Where-Object { $_ -ne '' })
    return [pscustomobject]@{ ExitCode = $process.ExitCode; Lines = $outputLines }
}

# Appends lines to UPGRADE-NOTICE.txt, which the installer's finish page shows. A new file starts
# with a title.
function Add-GridcoinUpgradeNotice {
    param([AllowEmptyString()][string[]]$Lines)
    if (-not (Test-Path -LiteralPath $script:NoticePath)) {
        Add-Content -LiteralPath $script:NoticePath -Encoding UTF8 -Value @('Notes from the Gridcoin upgrade', '')
    }
    Add-Content -LiteralPath $script:NoticePath -Encoding UTF8 -Value $Lines
}

# The gridcoinresearchd.exe processes that run <InstallDir>\daemon\gridcoinresearchd.exe, and those
# whose path cannot be read, which may.
function Get-GridcoinInstalledCoreProcess {
    $found = @()
    foreach ($process in @(Get-CimInstance -ClassName Win32_Process -Filter "Name = 'gridcoinresearchd.exe'")) {
        $image = [string]$process.ExecutablePath
        if (-not $image -or [string]::Equals($image, $script:CorePath, [System.StringComparison]::OrdinalIgnoreCase)) {
            $found += $process
        }
    }
    return $found
}

# The \Gridcoin\ task with this name, or $null.
function Get-GridcoinTask {
    param([Parameter(Mandatory = $true)][string]$Name)
    $service = Get-GridcoinTaskService
    try {
        return $service.GetFolder('\Gridcoin').GetTask($Name)
    } catch {
        return $null
    }
}

# Starts the Gridcoin services named and, with CoreStartTask, the \Gridcoin\Core-Start task: only
# when it exists and, with RequireS4U, only when it is registered without a stored password.
# Returns the problems, empty when everything started.
function Start-GridcoinCore {
    param(
        [string[]]$Services = @(),
        [bool]$CoreStartTask = $false,
        [switch]$RequireS4U
    )
    $problems = @()
    $knownNames = @($script:GridcoinServices | ForEach-Object { $_.Name })
    foreach ($serviceName in $Services) {
        if ($knownNames -notcontains $serviceName) { continue }
        try {
            Start-Service -Name $serviceName
            Write-Host "Started the service $serviceName."
            $script:StartedServices += $serviceName
        } catch {
            $problems += "the service $serviceName did not start ($($_.Exception.Message))"
        }
    }
    if ($CoreStartTask) {
        $startTask = Get-GridcoinTask -Name 'Core-Start'
        if ($null -eq $startTask) {
            Write-Host 'The core of \Gridcoin\Core-Start is not started again: that task no longer exists (see UPGRADE-NOTICE.txt).'
        } elseif (-not [bool]$startTask.Enabled) {
            Write-Host 'The core of \Gridcoin\Core-Start is not started again: the task is disabled, as it was before.'
        } elseif ($RequireS4U -and [int]$startTask.Definition.Principal.LogonType -ne 2) {
            $problems += '\Gridcoin\Core-Start is still registered by an earlier version, so it was not started; run Install-GridcoinCoreTask.ps1'
        } else {
            try {
                $null = $startTask.Run($null)
                Write-Host 'Started \Gridcoin\Core-Start.'
                $script:StartedTaskSid = Get-GridcoinTaskPrincipalSid $startTask
            } catch {
                $problems += "\Gridcoin\Core-Start did not start ($($_.Exception.Message))"
            }
        }
    }
    return $problems
}

# StopCore. With SavePath, what was running is written there for StartCore.
function Stop-GridcoinCore {
    param([string]$SavePath)
    $stoppedServices = @()
    $stoppedTask = $false
    try {
        foreach ($entry in $script:GridcoinServices) {
            $controller = Get-Service -Name $entry.Name -ErrorAction SilentlyContinue
            if ($null -eq $controller) { continue }
            $status = [string]$controller.Status
            if ($status -eq 'Stopped') { continue }
            Write-Host "Stopping the service $($entry.Name) ..."
            if ($status -ne 'StopPending') { $controller.Stop() }
            try {
                $controller.WaitForStatus([System.ServiceProcess.ServiceControllerStatus]::Stopped, (New-TimeSpan -Seconds 600))
            } catch {
                throw "the service $($entry.Name) did not stop within 600 seconds."
            }
            # One that was already stopping was not running for this upgrade to restart.
            if ($status -ne 'StopPending') { $stoppedServices += $entry.Name }
            Write-Host "Stopped the service $($entry.Name)."
        }

        if (@(Get-GridcoinCoreStartPids).Count -gt 0) {
            Write-Host 'Stopping the core of \Gridcoin\Core-Start through \Gridcoin\Core-Stop ...'
            $stoppedTask = $true
            if (-not (Wait-GridcoinCoreStopped -TimeoutSec 300)) {
                throw 'the core of \Gridcoin\Core-Start did not stop within 300 seconds.'
            }
            Write-Host 'Stopped the core of \Gridcoin\Core-Start.'
        }

        # A service or a task that has just stopped can take a moment to leave; a core started
        # by hand does not.
        $deadline = (Get-Date).AddSeconds(30)
        $left = @(Get-GridcoinInstalledCoreProcess)
        while ($left.Count -gt 0 -and (Get-Date) -lt $deadline) {
            Start-Sleep -Seconds 2
            $left = @(Get-GridcoinInstalledCoreProcess)
        }
        if ($left.Count -gt 0) {
            $described = @()
            foreach ($process in $left) {
                $owner = ''
                try {
                    $ownerInfo = Invoke-CimMethod -InputObject $process -MethodName GetOwner
                    if ($ownerInfo.User) { $owner = ", run by $($ownerInfo.Domain)\$($ownerInfo.User)" }
                } catch {
                    $owner = ''
                }
                $described += "process id $($process.ProcessId)$owner"
            }
            throw ("a core started by hand is still running ($script:CorePath; " + ($described -join '; ') +
                   '); stop it and retry.')
        }
    } catch {
        $stopFailure = $_.Exception.Message
        $restartProblems = @(Start-GridcoinCore -Services $stoppedServices -CoreStartTask $stoppedTask)
        foreach ($problem in $restartProblems) { Write-Host "WARNING: $problem." }
        throw $stopFailure
    }

    # The service names are one comma-separated string: Windows PowerShell 5.1 does not always
    # write an array as a JSON array.
    if ($SavePath) {
        $saved = [ordered]@{ services = ($stoppedServices -join ','); coreStartTask = $stoppedTask }
        Set-Content -LiteralPath $SavePath -Encoding UTF8 -Value ($saved | ConvertTo-Json)
    }
    if ($stoppedServices.Count -eq 0 -and -not $stoppedTask) { Write-Host 'No Gridcoin core was running.' }
}

# HardenInstallDir.
function Set-GridcoinInstallDirAcl {
    $programFiles = [Environment]::GetFolderPath('ProgramFiles').TrimEnd('\')
    if ($script:InstallRoot -ieq $programFiles -or
        $script:InstallRoot.StartsWith($programFiles + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
        Write-Host "$script:InstallRoot is under $programFiles, whose access list keeps it for administrators."
        return
    }
    $fullPath = [System.IO.Path]::GetFullPath($InstallDir)
    $root = [System.IO.Path]::GetPathRoot($fullPath)
    if ($fullPath.StartsWith('\\') -or $fullPath.TrimEnd('\') -ieq $root.TrimEnd('\')) {
        throw "the install directory $fullPath is a drive or a network path. Choose a directory on a local drive, such as C:\Gridcoin."
    }

    # The list set below protects the directory only while every folder above it is closed to
    # non-administrators too: whoever can write a parent can rename the directory away and put
    # their own in its place. Checked before anything changes, so a refusal leaves every core running.
    $parentPath = Split-Path -Parent $script:InstallRoot
    $parentOffender = Get-GridcoinWriteVector -Path $parentPath -AllowedSids (Get-GridcoinPrivilegedSids) -ChainMasks
    if ($parentOffender) {
        throw ("a folder above the install directory $script:InstallRoot can be changed by accounts that are not administrators, " +
               "so no access list on the directory itself can protect it. Found: $parentOffender. Install under Program Files " +
               '(the default), or remove the write access of non-administrators from that folder, and retry.')
    }

    $security = New-Object System.Security.AccessControl.DirectorySecurity
    $security.SetSecurityDescriptorSddlForm($script:InstallDirSddl, [System.Security.AccessControl.AccessControlSections]'Owner, Access')

    if (Test-Path -LiteralPath $script:InstallRoot) {
        $item = Get-Item -LiteralPath $script:InstallRoot -Force
        if (-not $item.PSIsContainer) { throw "$script:InstallRoot is a file, not a directory." }
        if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "$script:InstallRoot is a link or a junction to another place. Choose another directory."
        }
        $ownerSid = (Get-Acl -LiteralPath $script:InstallRoot).GetOwner([System.Security.Principal.SecurityIdentifier]).Value
        if ((Get-GridcoinPrivilegedSids) -notcontains $ownerSid) {
            throw ("$script:InstallRoot already exists and its owner, $(Get-GridcoinAccountName $ownerSid), is not an " +
                   'administrator, so it may have been made by another account to be used later. Choose another directory, or ' +
                   'remove this one first.')
        }
        # An existing directory gets this list only when it is empty or holds an earlier install,
        # never another program's files.
        $entries = @(Get-ChildItem -LiteralPath $script:InstallRoot -Force)
        $earlier = (Test-Path -LiteralPath (Join-Path $script:InstallRoot 'gridcoinresearch.exe')) -or
                   (Test-Path -LiteralPath (Join-Path $script:InstallRoot 'uninstall.exe'))
        if ($entries.Count -gt 0 -and -not $earlier) {
            throw "$script:InstallRoot already holds other files. Choose an empty or a new directory, or the directory of an earlier Gridcoin install."
        }
        $item.SetAccessControl($security)
        Write-Host "Gave $script:InstallRoot the owner Administrators and an access list that only administrators can change."
    } else {
        [void][System.IO.Directory]::CreateDirectory($script:InstallRoot, $security)
        Write-Host "Created $script:InstallRoot with the owner Administrators and an access list that only administrators can change."
    }

    $check = Get-Acl -LiteralPath $script:InstallRoot
    $offender = Get-GridcoinWriteVector -Path $script:InstallRoot -AllowedSids (Get-GridcoinPrivilegedSids)
    if (-not $check.AreAccessRulesProtected -or $offender) {
        throw "the access list of $script:InstallRoot is not the one just set: $offender"
    }
}

# The data directory an earlier version's task names with -datadir="..." (the core tasks) or
# -DataDir "..." (the autounlock task), or $null for the default.
function Get-GridcoinTaskDataDir {
    param($RegisteredTask)
    foreach ($action in $RegisteredTask.Definition.Actions) {
        if ([int]$action.Type -ne 0) { continue }
        $actionText = [string]$action.Arguments
        if ($actionText -match '-datadir(?:=|\s+)"([^"]+)"') { return $Matches[1] }
    }
    return $null
}

# Whether an earlier task started by itself at boot: some boot trigger of it is enabled.
function Get-GridcoinBootTriggerOn {
    param($RegisteredTask)
    foreach ($taskTrigger in $RegisteredTask.Definition.Triggers) {
        if ([int]$taskTrigger.Type -eq 8 -and [bool]$taskTrigger.Enabled) { return $true }
    }
    return $false
}

# The notice line for an account whose earlier start tasks this upgrade removed. Task mode is for an
# administrator's own wallet, so a standard account is pointed at the service mode only.
# AccountDataDir is the account's data directory when its tasks named one, else empty.
function Add-GridcoinStartNotice {
    param(
        [Parameter(Mandatory = $true)][string]$AccountName,
        [bool]$Administrator = $false,
        [string]$AccountDataDir = '',
        [string]$Reason = ''
    )
    $because = ''
    if ($Reason) { $because = " ($Reason)" }
    if ($Administrator) {
        $dataDirText = ''
        if ($AccountDataDir) { $dataDirText = " -DataDir `"$AccountDataDir`"" }
        $line = ("Unattended start for $AccountName was removed by the upgrade$because. Sign in as $AccountName and " +
                 "run, from an elevated PowerShell: & `"$script:WindowsDir\Install-GridcoinCoreTask.ps1`"$dataDirText " +
                 '-- or ask an administrator to set up the service mode.')
    } else {
        $fromText = ''
        if ($AccountDataDir) { $fromText = " --from=`"$AccountDataDir`"" }
        $tool = Join-Path $script:InstallRoot 'daemon\gridcoin-service.exe'
        $line = ("Unattended start for $AccountName was removed by the upgrade$because. Task mode is for an " +
                 "administrator's own wallet; for this one, ask an administrator to set up the service mode, from an " +
                 "elevated command prompt: `"$tool`" install, then `"$tool`" enable --user=`"$AccountName`"$fromText.")
    }
    Add-GridcoinUpgradeNotice -Lines @($line, '')
}

# The notice line for an account whose earlier autounlock is gone, and what became of its old
# passphrase file. Off: it was turned off before the upgrade and stays off. CredPath is the file
# (empty when it is not known), NotDeleted what Remove-GridcoinLegacyPassphraseFile could not delete;
# NoFile: there was none to delete.
function Add-GridcoinAutounlockNotice {
    param(
        [Parameter(Mandatory = $true)][string]$AccountName,
        [bool]$Off = $false,
        [string]$Reason = '',
        [string]$CredPath = '',
        [AllowEmptyCollection()][string[]]$NotDeleted = @(),
        [switch]$NoFile
    )
    if ($Off) {
        $line = "Unattended autounlock for $AccountName was turned off before the upgrade and stays off."
    } else {
        $because = ''
        if ($Reason) { $because = " ($Reason)" }
        $line = "Unattended autounlock for $AccountName was removed by the upgrade and not carried over$because."
    }
    $credLeft = @($NotDeleted | Where-Object { $CredPath -and $_.StartsWith($CredPath) })
    $logsLeft = @($NotDeleted | Where-Object { -not ($CredPath -and $_.StartsWith($CredPath)) })
    if ($NoFile) {
        $line += ' No old passphrase file was found.'
    } elseif (-not $CredPath) {
        $line += (" If that account's data directory holds autounlock\passphrase.cred, delete it: it holds the wallet " +
                  'passphrase, and the core does not read it.')
    } elseif ($credLeft.Count -gt 0) {
        $line += (" Its old passphrase file could not be deleted: $($credLeft -join '; '). It still holds the wallet " +
                  "passphrase, and the core does not read it: " +
                  'delete it.')
    } else {
        $line += " Its old passphrase file, $CredPath, was deleted."
    }
    if ($logsLeft.Count -gt 0) { $line += " The old helper's logs could not be deleted: $($logsLeft -join '; ')." }
    $line += (' To set autounlock up, open the wallet as that account and use Settings -> Autounlock for ' +
              'staking..., or run setautounlock.')
    Add-GridcoinUpgradeNotice -Lines @($line, '')
}

# SwitchOver. Throws at the end, after every cleanup has run, when a part of it failed.
function Invoke-GridcoinSwitchOver {
    # The launcher Register-GridcoinCoreTasks writes goes beside the GridcoinCommon.ps1 it loaded,
    # which must be the installed one.
    $scriptDir = [System.IO.Path]::GetFullPath($PSScriptRoot).TrimEnd('\')
    if ($scriptDir -ine $script:WindowsDir) {
        throw "SwitchOver runs from $script:WindowsDir, not from $scriptDir."
    }
    # Everything below runs inside one try, so that an unexpected throw anywhere in the first part
    # (the task enumeration, the classification, the other accounts' notices) still reaches the
    # finally block, which removes the earlier autounlock helper (it holds the decrypted passphrase
    # in memory), replaces the earlier shutdown script and deletes the earlier scripts. Each item
    # of the first part records its own failure and the next one goes on.
    $failures = @()
    $helperDone = $false
    $dataDir = $null
    try {
        if (Test-Path -LiteralPath $script:NoticePath) { Remove-Item -LiteralPath $script:NoticePath -Force }

        $oldTaskNames = @('Core-Start', 'Core-Stop', 'Autounlock')
        $mySid = Get-GridcoinCurrentSid
        $myAccount = Get-GridcoinAccountName $mySid
        $taskService = Get-GridcoinTaskService
        $folder = $null
        try { $folder = $taskService.GetFolder('\Gridcoin') } catch { $folder = $null }

        # The tasks earlier versions registered (logon type 1), by their names,
        # with what each says: whether it was on, the data directory and the passphrase file it names.
        # Another account's cannot be registered again without that account, so they go.
        $own = @{}
        $others = [ordered]@{}
        $oldTasks = @()
        if ($null -ne $folder) {
            try {
                $oldTasks = @($folder.GetTasks(1))
            } catch {
                $failures += "the tasks in \Gridcoin could not be listed ($($_.Exception.Message))"
            }
        }
        if ($null -ne $folder) {
            foreach ($oldTask in $oldTasks) {
                $taskName = ''
                try {
                    $taskName = [string]$oldTask.Name
                    if ($oldTaskNames -notcontains $taskName) { continue }
                    if ([int]$oldTask.Definition.Principal.LogonType -ne 1) { continue }
                    $taskFacts = @{
                        Enabled = [bool]$oldTask.Enabled
                        BootOn = (Get-GridcoinBootTriggerOn $oldTask)
                        DataDir = (Get-GridcoinTaskDataDir $oldTask)
                        CredPath = $null
                    }
                    foreach ($oldAction in $oldTask.Definition.Actions) {
                        if ([int]$oldAction.Type -ne 0) { continue }
                        if (([string]$oldAction.Arguments) -match '-CredPath\s+"([^"]+)"') { $taskFacts.CredPath = $Matches[1] }
                    }
                    $taskSid = Get-GridcoinTaskPrincipalSid $oldTask
                    if ($taskSid -and $taskSid -eq $mySid) {
                        $own[$taskName] = $taskFacts
                        continue
                    }
                    $account = Get-GridcoinAccountName $taskSid
                    if (-not $taskSid) { $account = [string]$oldTask.Definition.Principal.UserId }
                    if (-not $others.Contains($account)) { $others[$account] = @{ Sid = $taskSid; Tasks = @{} } }
                    $others[$account].Tasks[$taskName] = $taskFacts
                    try {
                        try { $oldTask.Stop(0) } catch { Write-Verbose "\Gridcoin\$taskName was not running" }
                        $folder.DeleteTask($taskName, 0)
                        Write-Host "Removed \Gridcoin\$taskName of $account, which an earlier version registered."
                    } catch {
                        $failures += "\Gridcoin\$taskName of $account could not be removed ($($_.Exception.Message))"
                    }
                } catch {
                    $failures += "the task \Gridcoin\$taskName could not be examined or removed ($($_.Exception.Message))"
                }
            }
        }

        # Another account's notice lines. Its old passphrase file is deleted, as it is not converted.
        foreach ($account in @($others.Keys)) {
            try {
                $accountTasks = $others[$account].Tasks
                $coreDir = ''
                foreach ($coreName in @('Core-Stop', 'Core-Start')) {
                    if (-not $coreDir -and $accountTasks.Contains($coreName) -and $accountTasks[$coreName].DataDir) {
                        $coreDir = $accountTasks[$coreName].DataDir
                    }
                }
                if ($accountTasks.Contains('Core-Start') -or $accountTasks.Contains('Core-Stop')) {
                    $startReason = ''
                    if ($accountTasks.Contains('Core-Start') -and
                        -not ($accountTasks['Core-Start'].Enabled -and $accountTasks['Core-Start'].BootOn)) {
                        $startReason = 'it was turned off'
                    }
                    Add-GridcoinStartNotice -AccountName $account -AccountDataDir $coreDir -Reason $startReason `
                        -Administrator (Test-GridcoinLocalAdministrator $others[$account].Sid)
                }
                if ($accountTasks.Contains('Autounlock')) {
                    $unlockFacts = $accountTasks['Autounlock']
                    $otherCred = $unlockFacts.CredPath
                    if (-not $otherCred -and $unlockFacts.DataDir) { $otherCred = Join-Path $unlockFacts.DataDir 'autounlock\passphrase.cred' }
                    $otherLeft = @()
                    if ($otherCred) { $otherLeft = @(Remove-GridcoinLegacyPassphraseFile -CredPath $otherCred) }
                    Add-GridcoinAutounlockNotice -AccountName $account -Off (-not ($unlockFacts.Enabled -and $unlockFacts.BootOn)) `
                        -CredPath ([string]$otherCred) -NotDeleted $otherLeft
                }
            } catch {
                $failures += "the notice or the cleanup for the tasks of $account failed ($($_.Exception.Message))"
            }
        }

        # This account's data directory is the one its core tasks name (none: the default). The
        # autounlock task names one too, which decides only when there is no core task.
        $defaultDataDir = Join-Path $env:APPDATA 'GridcoinResearch'
        $dataDir = $defaultDataDir
        $ownCore = $own.Contains('Core-Start') -or $own.Contains('Core-Stop')
        $namedDir = $null
        foreach ($coreName in @('Core-Stop', 'Core-Start')) {
            if (-not $namedDir -and $own.Contains($coreName) -and $own[$coreName].DataDir) { $namedDir = $own[$coreName].DataDir }
        }
        if ($namedDir) {
            $dataDir = $namedDir
        } elseif (-not $ownCore -and $own.Contains('Autounlock') -and $own['Autounlock'].DataDir) {
            $dataDir = $own['Autounlock'].DataDir
        }

        # This account's core tasks are registered again, S4U, for the same data directory and in the
        # state they were in: a task or a boot trigger that was off stays off. When that fails, its
        # tasks go as another account's do, whatever was registered before the failure.
        $reregistered = $false
        $registrationWhy = ''
        if ($ownCore) {
            $startOn = $true
            $bootOn = $true
            if ($own.Contains('Core-Start')) {
                $startOn = $own['Core-Start'].Enabled
                $bootOn = $own['Core-Start'].BootOn
            }
            $stopOn = $true
            if ($own.Contains('Core-Stop')) { $stopOn = $own['Core-Stop'].Enabled }
            try {
                if ($dataDir -match '["%]' -or $dataDir -match '\\$') {
                    throw "the data directory '$dataDir' of the old tasks cannot be written into the new launcher"
                }
                $ownAndPrivileged = @(Get-GridcoinPrivilegedSids) + @($mySid)
                foreach ($guarded in @($script:WindowsDir, $script:CorePath)) {
                    $offender = Get-GridcoinWriteVector -Path $guarded -AllowedSids $ownAndPrivileged -ChainMasks
                    if ($offender) { throw "refusing to register a boot task that runs from '$guarded': $offender" }
                }
                $null = Register-GridcoinCoreTasks -CorePath $script:CorePath -DataDir $dataDir -StartEnabled $startOn `
                    -BootTriggerEnabled $bootOn -StopEnabled $stopOn
                Write-Host 'Registered \Gridcoin\Core-Start and \Gridcoin\Core-Stop again for this account, without a stored password (S4U).'
                $reregistered = $true
                $keptOff = @()
                if (-not $startOn) {
                    $keptOff += '\Gridcoin\Core-Start is disabled'
                } elseif (-not $bootOn) {
                    $keptOff += 'the boot trigger of \Gridcoin\Core-Start is off'
                }
                if (-not $stopOn) { $keptOff += '\Gridcoin\Core-Stop is disabled' }
                if ($keptOff.Count -gt 0) {
                    Add-GridcoinUpgradeNotice -Lines @(
                        ("The tasks of $myAccount are registered again without a stored password, and what was off before " +
                         "the upgrade stays off: $($keptOff -join ', and '). Turn it on in Task Scheduler when you want it."),
                        '')
                }
            } catch {
                $registrationWhy = $_.Exception.Message
                Write-Host "WARNING: the tasks of this account were not registered again: $registrationWhy."
                $failures += "the tasks of $myAccount were not registered again ($registrationWhy)"
                foreach ($taskName in $oldTaskNames) {
                    try {
                        $ownTask = $taskService.GetFolder('\Gridcoin').GetTask($taskName)
                    } catch {
                        continue
                    }
                    if ((Get-GridcoinTaskPrincipalSid $ownTask) -ne $mySid) { continue }
                    try {
                        try { $ownTask.Stop(0) } catch { Write-Verbose "\Gridcoin\$taskName was not running" }
                        $taskService.GetFolder('\Gridcoin').DeleteTask($taskName, 0)
                        Write-Host "Removed \Gridcoin\$taskName of $myAccount."
                    } catch {
                        $failures += "\Gridcoin\$taskName of $myAccount could not be removed ($($_.Exception.Message))"
                    }
                }
                $ownDirText = ''
                if ($dataDir -ine $defaultDataDir) { $ownDirText = $dataDir }
                Add-GridcoinStartNotice -AccountName $myAccount -Administrator $true -AccountDataDir $ownDirText -Reason $registrationWhy
            }
        }

        # The earlier versions' autounlock helper goes for every account. This account's file is
        # converted only when its Autounlock task was on and its core tasks are registered again;
        # otherwise it is deleted, and the notice says so.
        $ownUnlock = $own.Contains('Autounlock')
        $unlockOn = $ownUnlock -and $own['Autounlock'].Enabled -and $own['Autounlock'].BootOn
        $converted = $false
        $unlockWhy = ''
        $oldBlob = $null
        try {
            $oldBlob = Remove-GridcoinLegacyAutounlock -DataDir $dataDir -AllAccounts
        } catch {
            $failures += "the earlier autounlock helper was not removed ($($_.Exception.Message))"
        }
        $helperDone = $true
        if (-not $oldBlob -and $ownUnlock -and $own['Autounlock'].CredPath -and (Test-Path -LiteralPath $own['Autounlock'].CredPath)) {
            $oldBlob = $own['Autounlock'].CredPath
        }
        if ($reregistered -and $unlockOn -and $oldBlob) {
            $networkName = ''
            try {
                $networkName = Get-GridcoinConfNetwork -DataDir $dataDir
                if (Convert-GridcoinLegacyAutounlockBlob -OldPath $oldBlob -DataDir $dataDir -Network $networkName) {
                    Write-Host "Converted the autounlock passphrase file for the $networkName network. The old file and its logs are deleted."
                    $converted = $true
                }
            } catch {
                $unlockWhy = "the old file could not be converted: $($_.Exception.Message)"
                Write-Host "WARNING: $unlockWhy."
            }
        } elseif ($registrationWhy) {
            $unlockWhy = 'the tasks of this account were not registered again'
        } elseif ($ownUnlock -and -not $ownCore) {
            $unlockWhy = 'this account had no core tasks to start the core'
        }
        if (-not $converted -and ($ownUnlock -or $oldBlob)) {
            $ownLeft = @()
            if ($oldBlob) { $ownLeft = @(Remove-GridcoinLegacyPassphraseFile -CredPath $oldBlob) }
            if (-not $ownUnlock -and -not $unlockWhy) { $unlockWhy = 'the earlier Autounlock task was already gone' }
            Add-GridcoinAutounlockNotice -AccountName $myAccount -Off ($ownUnlock -and -not $unlockOn) -Reason $unlockWhy `
                -CredPath ([string]$oldBlob) -NotDeleted $ownLeft -NoFile:(-not $oldBlob)
        }
    } catch {
        $failures += "SwitchOver stopped early ($($_.Exception.Message))"
    } finally {
        # Whatever happened above, the earlier helper goes (its decrypted passphrase would stay in memory), the earlier
        # shutdown script is replaced and the earlier scripts are deleted. Their failures are recorded.
        if (-not $helperDone) {
            try {
                $fallbackDir = $dataDir
                if (-not $fallbackDir) { $fallbackDir = Join-Path $env:APPDATA 'GridcoinResearch' }
                $leftBlob = Remove-GridcoinLegacyAutounlock -DataDir $fallbackDir -AllAccounts
                if ($leftBlob) { $failures += "the earlier autounlock passphrase file $leftBlob was left in place" }
            } catch {
                $failures += "the earlier autounlock helper was not removed ($($_.Exception.Message))"
            }
        }

        # The earlier versions' shutdown script ran the core as SYSTEM. It is replaced by the one that
        # starts Core-Stop, or removed when there is no Core-Stop to start.
        try {
            $shutdownCmd = Join-Path $env:SystemRoot 'System32\GroupPolicy\Machine\Scripts\Shutdown\GridcoinShutdownFlush.cmd'
            if (Test-Path -LiteralPath $shutdownCmd) {
                $current = [string](Get-Content -LiteralPath $shutdownCmd -Raw)
                if ($current -notmatch 'GridcoinShutdownFlush\.ps1') {
                    $flushScript = Join-Path $script:WindowsDir 'Set-GridcoinShutdownFlush.ps1'
                    if ($null -ne (Get-GridcoinTask -Name 'Core-Stop')) {
                        & $flushScript
                    } else {
                        & $flushScript -Remove
                    }
                }
            }
        } catch {
            $failures += "the shutdown script was not replaced ($($_.Exception.Message))"
        }

        foreach ($oldName in @('Set-GridcoinAutounlock.ps1', 'Invoke-GridcoinAutounlock.ps1', 'release-keys.txt')) {
            $oldPath = Join-Path $script:WindowsDir $oldName
            try {
                if (Test-Path -LiteralPath $oldPath) {
                    Remove-Item -LiteralPath $oldPath -Force
                    Write-Host "Deleted $oldPath, which this version no longer uses."
                }
            } catch {
                $failures += "$oldPath was not deleted ($($_.Exception.Message))"
            }
        }
    }

    if ($failures.Count -gt 0) { throw (($failures -join '; ') + '.') }
}

# The -datadir that the core line of the Core-Start launcher names, with the doubled % undone, or
# '' when it names none (or there is no launcher).
function Get-GridcoinLauncherDataDir {
    $launcher = Join-Path $script:WindowsDir 'core-autostart.cmd'
    if (-not (Test-Path -LiteralPath $launcher)) { return '' }
    $inside = $false
    foreach ($line in @(Get-Content -LiteralPath $launcher)) {
        if ($line -eq 'rem BEGIN GENERATED') { $inside = $true; continue }
        if ($line -eq 'rem END GENERATED') { break }
        if ($inside -and $line -match '-datadir="([^"]+)"') { return ($Matches[1] -replace '%%', '%') }
    }
    return ''
}

# What the finish page's Run does, from what StartCore restarted. The first four characters of the
# file are the case, the rest is the data directory:
#   task  the installing account's own Core-Start was restarted: the GUI attaches to it
#   othr  a service, or a core of another account, was restarted: Run is not ticked
#   none  nothing was restarted: the plain GUI
# The file is written in the ANSI code page, which the installer reads it in.
function Write-GridcoinFinishRun {
    param([bool]$OwnTask, [bool]$Other)
    $mode = 'none'
    $dataDirText = ''
    if ($OwnTask) {
        $mode = 'task'
        $dataDirText = Get-GridcoinLauncherDataDir
        $ansi = [System.Text.Encoding]::Default
        if ($ansi.GetString($ansi.GetBytes($dataDirText)) -cne $dataDirText) {
            $mode = 'othr'
            $dataDirText = ''
        }
    } elseif ($Other) {
        $mode = 'othr'
    }
    $finishPath = Join-Path (Split-Path -Parent $StateFile) 'finish-run.txt'
    [System.IO.File]::WriteAllText($finishPath, ($mode + $dataDirText), [System.Text.Encoding]::Default)
}

# StartCore.
function Invoke-GridcoinStartCore {
    $script:StartedServices = @()
    $script:StartedTaskSid = ''
    if (-not $StateFile -or -not (Test-Path -LiteralPath $StateFile)) {
        Write-Host 'Nothing was recorded as running before the upgrade, so nothing is started.'
        if ($StateFile) { Write-GridcoinFinishRun -OwnTask $false -Other $false }
        return
    }
    $saved = Get-Content -LiteralPath $StateFile -Raw | ConvertFrom-Json
    $serviceNames = @(([string]$saved.services) -split ',' | Where-Object { $_ })
    $problems = @(Start-GridcoinCore -Services $serviceNames -CoreStartTask ([bool]$saved.coreStartTask) -RequireS4U)

    $ownTask = ($script:StartedTaskSid -and $script:StartedTaskSid -eq (Get-GridcoinCurrentSid))
    $otherTask = ($script:StartedTaskSid -and -not $ownTask)
    $serviceStarted = (@($script:StartedServices).Count -gt 0)
    Write-GridcoinFinishRun -OwnTask $ownTask -Other ($otherTask -or $serviceStarted)
    if ($serviceStarted -and -not $ownTask) {
        Add-GridcoinUpgradeNotice -Lines @(
            ('The Gridcoin service was restarted. To attach a GUI, run, from a normal (not elevated) PowerShell: ' +
             (Join-Path $script:WindowsDir 'Start-GridcoinGui.ps1') + ' -Service (add -Network test for testnet).'),
            '')
    }
    if ($problems.Count -gt 0) { throw (($problems -join '; ') + '.') }
}

# ReportPermissions.
function Invoke-GridcoinReportPermissions {
    $baseDir = Join-Path $env:APPDATA 'GridcoinResearch'
    $checks = @()
    if (Test-Path -LiteralPath $baseDir -PathType Container) { $checks += '-reportpermissions' }
    if (Test-Path -LiteralPath (Join-Path $baseDir 'testnet') -PathType Container) { $checks += '-reportpermissions -testnet' }

    # A finding is a line that starts with "WARNING:" (one line each, its fix included). Anything else
    # the core wrote is logged, never filed as a finding.
    $findings = @()
    foreach ($checkArguments in $checks) {
        $result = Invoke-GridcoinProgram -FilePath $script:CorePath -ArgumentText $checkArguments
        if ($result.ExitCode -eq 0) { continue }
        $warnings = @($result.Lines | Where-Object { $_ -like 'WARNING:*' })
        foreach ($other in @($result.Lines | Where-Object { $_ -notlike 'WARNING:*' })) { Write-Host $other }
        if ($result.ExitCode -eq 1 -and $warnings.Count -gt 0) {
            $findings += $warnings
            continue
        }
        Write-Host "The permission check ($checkArguments) found nothing to report: exit code $($result.ExitCode)."
    }
    if ($findings.Count -eq 0) {
        Write-Host 'No file of this account''s Gridcoin data directories gives another account access.'
        return
    }
    foreach ($finding in $findings) { Write-Host $finding }
    Add-GridcoinUpgradeNotice -Lines (@(
        'Files whose permissions are wider than this release sets for new files:',
        ('The installer changed nothing. Wider access may be what you set up on purpose; if it is not, ' +
         'each line says how to remove that access.'),
        '') + $findings + @(''))
}

# UninstallCheck. Returns the exit code.
function Test-GridcoinServiceModeOff {
    $serviceModeOn = $false
    foreach ($entry in $script:GridcoinServices) {
        $controller = Get-Service -Name $entry.Name -ErrorAction SilentlyContinue
        if ($null -ne $controller -and [string]$controller.StartType -ne 'Disabled') {
            Write-Host "The service $($entry.Name) is installed and not disabled."
            $serviceModeOn = $true
        }
        if (Test-Path -LiteralPath (Join-Path $entry.NetDataDir 'wallet.dat')) {
            Write-Host "The service's data directory $($entry.NetDataDir) holds a wallet."
            $serviceModeOn = $true
        }
    }
    if ($serviceModeOn) { return 3 }
    return 0
}

# PreUninstall.
function Invoke-GridcoinPreUninstall {
    Stop-GridcoinCore
    & (Join-Path $script:WindowsDir 'Uninstall-GridcoinCoreTask.ps1') -All
    $tool = Join-Path $script:InstallRoot 'daemon\gridcoin-service.exe'
    foreach ($entry in $script:GridcoinServices) {
        if ($null -eq (Get-Service -Name $entry.Name -ErrorAction SilentlyContinue)) { continue }
        $result = Invoke-GridcoinProgram -FilePath $tool -ArgumentText ('uninstall --network=' + $entry.Network)
        foreach ($line in $result.Lines) { Write-Host $line }
        if ($result.ExitCode -ne 0) {
            throw "gridcoin-service.exe uninstall --network=$($entry.Network) exited with code $($result.ExitCode)."
        }
    }
}

$exitCode = 1
try {
    $script:InstallRoot = [System.IO.Path]::GetFullPath($InstallDir).TrimEnd('\')
    $script:WindowsDir = Join-Path $script:InstallRoot 'windows'
    $script:CorePath = Join-Path $script:InstallRoot 'daemon\gridcoinresearchd.exe'
    $script:NoticePath = Join-Path $script:WindowsDir 'UPGRADE-NOTICE.txt'

    if (-not (Test-GridcoinElevated)) { throw 'the installer must run with administrator rights.' }
    if ([Environment]::Is64BitOperatingSystem -and -not [Environment]::Is64BitProcess) {
        throw 'this must run in 64-bit Windows PowerShell; the installer starts it with file system redirection off.'
    }
    switch ($Step) {
        'StopCore' { Stop-GridcoinCore -SavePath $StateFile; $exitCode = 0 }
        'HardenInstallDir' { Set-GridcoinInstallDirAcl; $exitCode = 0 }
        'SwitchOver' { Invoke-GridcoinSwitchOver; $exitCode = 0 }
        'StartCore' { Invoke-GridcoinStartCore; $exitCode = 0 }
        'ReportPermissions' { Invoke-GridcoinReportPermissions; $exitCode = 0 }
        'UninstallCheck' { $exitCode = Test-GridcoinServiceModeOff }
        'PreUninstall' { Invoke-GridcoinPreUninstall; $exitCode = 0 }
    }
} catch {
    Write-Host "Invoke-GridcoinInstallStep ${Step}: $($_.Exception.Message)"
    $exitCode = 1
}
exit $exitCode
