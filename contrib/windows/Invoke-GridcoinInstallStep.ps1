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
                        task, replaces their shutdown script, and deletes their helper scripts.
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
    $lines = @(($outputText + "`n" + $errorText) -split "`r?`n" | Where-Object { $_ -ne '' })
    return [pscustomobject]@{ ExitCode = $process.ExitCode; Lines = $lines }
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
        } catch {
            $problems += "the service $serviceName did not start ($($_.Exception.Message))"
        }
    }
    if ($CoreStartTask) {
        $startTask = Get-GridcoinTask -Name 'Core-Start'
        if ($null -eq $startTask) {
            Write-Host 'The core of \Gridcoin\Core-Start is not started again: that task no longer exists (see UPGRADE-NOTICE.txt).'
        } elseif ($RequireS4U -and [int]$startTask.Definition.Principal.LogonType -ne 2) {
            $problems += '\Gridcoin\Core-Start is still registered by an earlier version, so it was not started; run Install-GridcoinCoreTask.ps1'
        } else {
            try {
                $null = $startTask.Run($null)
                Write-Host 'Started \Gridcoin\Core-Start.'
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
        $reason = $_.Exception.Message
        $restartProblems = @(Start-GridcoinCore -Services $stoppedServices -CoreStartTask $stoppedTask)
        foreach ($problem in $restartProblems) { Write-Host "WARNING: $problem." }
        throw $reason
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

# The account name of a SID, or the SID when it has none.
function Get-GridcoinAccountName {
    param([string]$Sid)
    if (-not $Sid) { return '(an account that cannot be named)' }
    try {
        return (New-Object System.Security.Principal.SecurityIdentifier($Sid)).Translate([System.Security.Principal.NTAccount]).Value
    } catch {
        return $Sid
    }
}

# The data directory an earlier version's task names with -datadir="..." (the core tasks) or
# -DataDir "..." (the autounlock task), or $null for the default.
function Get-GridcoinTaskDataDir {
    param($Task)
    foreach ($action in $Task.Definition.Actions) {
        if ([int]$action.Type -ne 0) { continue }
        $actionText = [string]$action.Arguments
        if ($actionText -match '-datadir(?:=|\s+)"([^"]+)"') { return $Matches[1] }
    }
    return $null
}

# SwitchOver.
function Invoke-GridcoinSwitchOver {
    # The launcher Register-GridcoinCoreTasks writes goes beside the GridcoinCommon.ps1 it loaded,
    # which must be the installed one.
    $scriptDir = [System.IO.Path]::GetFullPath($PSScriptRoot).TrimEnd('\')
    if ($scriptDir -ine $script:WindowsDir) {
        throw "SwitchOver runs from $script:WindowsDir, not from $scriptDir."
    }
    if (Test-Path -LiteralPath $script:NoticePath) { Remove-Item -LiteralPath $script:NoticePath -Force }

    $mySid = Get-GridcoinCurrentSid
    $taskService = Get-GridcoinTaskService
    $folder = $null
    try { $folder = $taskService.GetFolder('\Gridcoin') } catch { $folder = $null }

    # The tasks earlier versions registered (logon type 1), by their names.
    $ownTasks = @()
    $otherAccounts = @()
    $coreDataDir = $null
    $autounlockDataDir = $null
    if ($null -ne $folder) {
        foreach ($task in @($folder.GetTasks(1))) {
            $taskName = [string]$task.Name
            if (@('Core-Start', 'Core-Stop', 'Autounlock') -notcontains $taskName) { continue }
            if ([int]$task.Definition.Principal.LogonType -ne 1) { continue }
            $taskSid = Get-GridcoinTaskPrincipalSid $task
            if ($taskSid -and $taskSid -eq $mySid) {
                $ownTasks += $taskName
                $named = Get-GridcoinTaskDataDir $task
                if ($named -and $taskName -eq 'Autounlock') { $autounlockDataDir = $named }
                if ($named -and $taskName -ne 'Autounlock' -and $null -eq $coreDataDir) { $coreDataDir = $named }
                continue
            }
            # Another account's: it cannot be registered again without that account, so it goes.
            $account = Get-GridcoinAccountName $taskSid
            if (-not $taskSid) { $account = [string]$task.Definition.Principal.UserId }
            try { $task.Stop(0) } catch { Write-Verbose "\Gridcoin\$taskName was not running" }
            $folder.DeleteTask($taskName, 0)
            Write-Host "Removed \Gridcoin\$taskName of $account, which an earlier version registered."
            if ($otherAccounts -notcontains $account) { $otherAccounts += $account }
        }
    }
    # The core's data directory is the one its tasks name (none: the default). The autounlock task
    # names one too, which decides only when there is no core task.
    $dataDir = Join-Path $env:APPDATA 'GridcoinResearch'
    if ($coreDataDir) {
        $dataDir = $coreDataDir
    } elseif ($autounlockDataDir -and $ownTasks -notcontains 'Core-Start' -and $ownTasks -notcontains 'Core-Stop') {
        $dataDir = $autounlockDataDir
    }

    foreach ($account in $otherAccounts) {
        Add-GridcoinUpgradeNotice -Lines @(
            ("Unattended start for $account was removed by the upgrade. Sign in as $account and run, from an " +
             "elevated PowerShell: $script:WindowsDir\Install-GridcoinCoreTask.ps1 -- or ask an administrator " +
             'to set up the service mode.'),
            '')
    }

    # This account's core tasks are registered again, S4U, for the same data directory.
    $reregistered = $false
    if ($ownTasks -contains 'Core-Start' -or $ownTasks -contains 'Core-Stop') {
        if ($dataDir -match '["%]' -or $dataDir -match '\\$') {
            throw "the data directory '$dataDir' of the old tasks cannot be written into the new launcher; run Install-GridcoinCoreTask.ps1 -DataDir with it."
        }
        $ownAndPrivileged = @(Get-GridcoinPrivilegedSids) + @($mySid)
        foreach ($guarded in @($script:WindowsDir, $script:CorePath)) {
            $offender = Get-GridcoinWriteVector -Path $guarded -AllowedSids $ownAndPrivileged -ChainMasks
            if ($offender) { throw "refusing to register a boot task that runs from '$guarded': $offender." }
        }
        $null = Register-GridcoinCoreTasks -CorePath $script:CorePath -DataDir $dataDir
        Write-Host 'Registered \Gridcoin\Core-Start and \Gridcoin\Core-Stop again for this account, without a stored password (S4U).'
        $reregistered = $true
    }

    # The earlier versions' autounlock helper goes for every account; its file is this account's.
    $ownAutounlock = $ownTasks -contains 'Autounlock'
    $oldBlob = Remove-GridcoinLegacyAutounlock -DataDir $dataDir -AllAccounts
    if ($reregistered -and $ownAutounlock -and $oldBlob) {
        $networkName = Get-GridcoinConfNetwork -DataDir $dataDir
        try {
            if (Convert-GridcoinLegacyAutounlockBlob -OldPath $oldBlob -DataDir $dataDir -Network $networkName) {
                Write-Host "Converted the autounlock passphrase file for the $networkName network. The old file and its logs are deleted."
            }
        } catch {
            $why = $_.Exception.Message
            Write-Host "WARNING: could not convert the old autounlock file '$oldBlob' ($why)."
            Add-GridcoinUpgradeNotice -Lines @(
                ("Autounlock could not be carried over for $(Get-GridcoinAccountName $mySid) ($why). The old file " +
                 "'$oldBlob' is left in place, and the core does not read it. Set autounlock up again in the wallet."),
                '')
        }
    }

    # The earlier versions' shutdown script ran the core as SYSTEM. It is replaced by the one that
    # starts Core-Stop, or removed when there is no Core-Stop to start.
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

    foreach ($oldName in @('Set-GridcoinAutounlock.ps1', 'Invoke-GridcoinAutounlock.ps1')) {
        $oldPath = Join-Path $script:WindowsDir $oldName
        if (Test-Path -LiteralPath $oldPath) {
            Remove-Item -LiteralPath $oldPath -Force
            Write-Host "Deleted $oldPath, which this version no longer uses."
        }
    }
}

# StartCore.
function Invoke-GridcoinStartCore {
    if (-not $StateFile -or -not (Test-Path -LiteralPath $StateFile)) {
        Write-Host 'Nothing was recorded as running before the upgrade, so nothing is started.'
        return
    }
    $saved = Get-Content -LiteralPath $StateFile -Raw | ConvertFrom-Json
    $serviceNames = @(([string]$saved.services) -split ',' | Where-Object { $_ })
    $problems = @(Start-GridcoinCore -Services $serviceNames -CoreStartTask ([bool]$saved.coreStartTask) -RequireS4U)
    if ($problems.Count -gt 0) { throw (($problems -join '; ') + '.') }
}

# ReportPermissions.
function Invoke-GridcoinReportPermissions {
    $baseDir = Join-Path $env:APPDATA 'GridcoinResearch'
    $checks = @()
    if (Test-Path -LiteralPath $baseDir -PathType Container) { $checks += '-reportpermissions' }
    if (Test-Path -LiteralPath (Join-Path $baseDir 'testnet') -PathType Container) { $checks += '-reportpermissions -testnet' }

    $findings = @()
    foreach ($checkArguments in $checks) {
        $result = Invoke-GridcoinProgram -FilePath $script:CorePath -ArgumentText $checkArguments
        if ($result.ExitCode -eq 0) { continue }
        if ($result.ExitCode -eq 1) {
            $findings += $result.Lines
            continue
        }
        Write-Host "The permission check ($checkArguments) did not run: exit code $($result.ExitCode). $($result.Lines -join ' ')"
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
