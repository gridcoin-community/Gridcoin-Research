#Requires -Version 5.1
<#
.SYNOPSIS
    Shared functions of the Gridcoin Windows scripts (dot-source it; it runs nothing).

.DESCRIPTION
    Install-GridcoinCoreTask.ps1, Uninstall-GridcoinCoreTask.ps1, Set-GridcoinShutdownFlush.ps1,
    Start-GridcoinGui.ps1 and the installer's steps dot-source this file from their own directory:

        . (Join-Path $PSScriptRoot 'GridcoinCommon.ps1')

    Same-account (task) mode registers its tasks with Task Scheduler's S4U logon (no password is
    stored anywhere) and keeps the wallet passphrase for autounlock in a DPAPI file that only the
    core reads. The file layout is the one src/wallet/autounlock_blob.h fixes.
#>
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:GridcoinScriptDir = $PSScriptRoot

# SYSTEM, Administrators, TrustedInstaller.
function Get-GridcoinPrivilegedSids {
    return @(
        'S-1-5-18',
        'S-1-5-32-544',
        'S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464'
    )
}

function Get-GridcoinCurrentSid {
    return [System.Security.Principal.WindowsIdentity]::GetCurrent().User.Value
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

function Test-GridcoinElevated {
    $currentIdentity = [System.Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object System.Security.Principal.WindowsPrincipal($currentIdentity)
    return $principal.IsInRole([System.Security.Principal.WindowsBuiltinRole]::Administrator)
}

function Get-GridcoinTaskService {
    $service = New-Object -ComObject 'Schedule.Service'
    $service.Connect()
    return $service
}

# The SID string of an ACE identity or an owner, which Get-Acl shows as an object or a bare string.
function Resolve-GridcoinSid {
    param($Identity)
    if ($null -eq $Identity) { return $null }
    if ($Identity -is [System.Security.Principal.SecurityIdentifier]) { return $Identity.Value }
    try {
        return $Identity.Translate([System.Security.Principal.SecurityIdentifier]).Value
    } catch {
        # a bare string has no Translate()
    }
    try {
        $account = New-Object System.Security.Principal.NTAccount(([string]$Identity))
        return $account.Translate([System.Security.Principal.SecurityIdentifier]).Value
    } catch {
        return $null
    }
}

# The SID of the account a registered task runs as, or $null when it cannot be told.
function Get-GridcoinTaskPrincipalSid {
    param($Task)
    $userId = [string]$Task.Definition.Principal.UserId
    if (-not $userId) { return $null }
    try {
        return (New-Object System.Security.Principal.SecurityIdentifier($userId)).Value
    } catch {
        # not a SID string, try it as an account name
    }
    try {
        $account = New-Object System.Security.Principal.NTAccount($userId)
        return $account.Translate([System.Security.Principal.SecurityIdentifier]).Value
    } catch {
        return $null
    }
}

# The first way an account outside AllowedSids can change what Path is or holds, or $null when
# there is none. It looks at the three things a plain scan of the access entries misses: an empty
# DACL (everyone has full control), the owner (who can rewrite the DACL), and Allow entries that
# grant write, delete or ownership rights. Only those rights are tested, not the composite Modify
# or FullControl, whose bit patterns overlap ReadAndExecute and Synchronize. On a file, appending
# counts. On a directory, the mask adds CreateFiles (a new entry can shadow the item) but leaves
# out AppendData, which the default volume root grants to Users. With ChainMasks, Path and every
# ancestor directory are checked, because write access to a containing directory is enough to
# replace the item. Without it, Path alone.
function Get-GridcoinWriteVector {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string[]]$AllowedSids,
        [switch]$ChainMasks
    )
    $fileMask = [int]([System.Security.AccessControl.FileSystemRights]'Write, Delete, DeleteSubdirectoriesAndFiles, ChangePermissions, TakeOwnership')
    $dirMask = [int]([System.Security.AccessControl.FileSystemRights]'CreateFiles, WriteExtendedAttributes, WriteAttributes, Delete, DeleteSubdirectoriesAndFiles, ChangePermissions, TakeOwnership')
    $inheritOnly = [int][System.Security.AccessControl.PropagationFlags]::InheritOnly

    $current = $Path
    while ($current) {
        try {
            $acl = Get-Acl -LiteralPath $current
        } catch {
            return "the access control list of '$current' could not be read"
        }
        $entries = @($acl.Access)
        if ($entries.Count -eq 0) { return "'$current' has an empty DACL (everyone has full control)" }

        $ownerSid = $null
        try { $ownerSid = $acl.GetOwner([System.Security.Principal.SecurityIdentifier]).Value } catch { $ownerSid = $null }
        if ($null -eq $ownerSid) { $ownerSid = Resolve-GridcoinSid $acl.Owner }
        if ($null -eq $ownerSid) { return "'$current' has an owner that cannot be resolved ($($acl.Owner))" }
        if ($AllowedSids -notcontains $ownerSid) {
            return "'$current' is owned by $($acl.Owner), who can rewrite its DACL at will"
        }

        $mask = $fileMask
        if (Test-Path -LiteralPath $current -PathType Container) { $mask = $dirMask }
        foreach ($entry in $entries) {
            if ($entry.AccessControlType -ne [System.Security.AccessControl.AccessControlType]::Allow) { continue }
            # An inherit-only entry grants nothing on this object; its children carry it as a real
            # entry, and the walk checks them in their own right.
            if (([int]$entry.PropagationFlags -band $inheritOnly) -ne 0) { continue }
            if (([int]$entry.FileSystemRights -band $mask) -eq 0) { continue }
            $entrySid = Resolve-GridcoinSid $entry.IdentityReference
            if ($null -eq $entrySid) {
                return "'$current': $($entry.IdentityReference) (a SID that cannot be resolved) has '$($entry.FileSystemRights)'"
            }
            if ($AllowedSids -notcontains $entrySid) {
                return "'$current': $($entry.IdentityReference) has '$($entry.FileSystemRights)'"
            }
        }

        if (-not $ChainMasks) { return $null }
        $parent = Split-Path -Parent $current
        if (-not $parent -or $parent -eq $current) { return $null }
        $current = $parent
    }
    return $null
}

# The PIDs of the running Core-Start instances, the launcher cmd.exe of each. The core and its
# restricted copy are children of that process, which exits only after they have.
function Get-GridcoinCoreStartPids {
    param([string]$TaskFolder = 'Gridcoin')
    $service = Get-GridcoinTaskService
    $startPath = "\$TaskFolder\Core-Start"
    $found = @()
    foreach ($running in $service.GetRunningTasks(1)) {
        if ([string]$running.Path -eq $startPath) { $found += [int]$running.EnginePID }
    }
    return $found
}

# Ends the core: it runs Core-Stop, then waits for the launcher of the running Core-Start instance to
# exit. True when no instance is left within TimeoutSec.
function Wait-GridcoinCoreStopped {
    param(
        [int]$TimeoutSec = 300,
        [string]$TaskFolder = 'Gridcoin'
    )
    $startPids = @(Get-GridcoinCoreStartPids -TaskFolder $TaskFolder)
    if ($startPids.Count -eq 0) { return $true }

    $service = Get-GridcoinTaskService
    $folder = $service.GetFolder("\$TaskFolder")
    $null = $folder.GetTask('Core-Stop').Run($null)

    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    foreach ($launcherPid in $startPids) {
        if ($launcherPid -gt 0) {
            $left = [int][Math]::Ceiling(($deadline - (Get-Date)).TotalSeconds)
            if ($left -lt 1) { $left = 1 }
            try {
                Wait-Process -Id $launcherPid -Timeout $left
            } catch {
                # the process was already gone, or the wait timed out; the check below decides
            }
            if (Get-Process -Id $launcherPid -ErrorAction SilentlyContinue) { return $false }
        } else {
            # No PID to wait on: poll the task until it leaves the running state (4).
            $startTask = $folder.GetTask('Core-Start')
            while ([int]$startTask.State -eq 4) {
                if ((Get-Date) -gt $deadline) { return $false }
                Start-Sleep -Seconds 2
            }
        }
    }
    return $true
}

# Writes one task into a folder by Task Scheduler's COM interface and registers it as S4U (logon type
# 2) for AccountSid. Nothing is stored for the account: S4U needs no password.
function Register-GridcoinTask {
    param(
        $TargetFolder,
        [string]$RegisteredName,
        $TaskDefinition,
        [string]$AccountSid
    )
    $sddl = 'D:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;FA;;;' + $AccountSid + ')'
    $createOrUpdate = 6
    $logonS4U = 2
    $null = $TargetFolder.RegisterTaskDefinition($RegisteredName, $TaskDefinition, $createOrUpdate, $AccountSid, $null, $logonS4U, $sddl)
}

function New-GridcoinTaskDefinition {
    param(
        $SchedulerService,
        [string]$Description,
        [string]$TimeLimit
    )
    $definition = $SchedulerService.NewTask(0)
    $definition.RegistrationInfo.Description = $Description
    $definition.Principal.RunLevel = 0
    $definition.Settings.Enabled = $true
    $definition.Settings.StartWhenAvailable = $true
    $definition.Settings.DisallowStartIfOnBatteries = $false
    $definition.Settings.StopIfGoingOnBatteries = $false
    $definition.Settings.ExecutionTimeLimit = $TimeLimit
    $definition.Settings.MultipleInstances = 2
    return $definition
}

# The Core-Start launcher of this installation, beside the scripts. There is one per installation,
# so one pair of tasks may use it (Get-GridcoinLauncherFolders).
function Get-GridcoinLauncherPath {
    return (Join-Path $script:GridcoinScriptDir 'core-autostart.cmd')
}

# The task folders directly under the root, other than ExceptFolder, whose Core-Start runs the
# launcher at LauncherPath.
function Get-GridcoinLauncherFolders {
    param(
        [Parameter(Mandatory = $true)][string]$LauncherPath,
        [string]$ExceptFolder = ''
    )
    $found = @()
    $service = Get-GridcoinTaskService
    foreach ($rootChild in @($service.GetFolder('\').GetFolders(0))) {
        $folderName = [string]$rootChild.Name
        if ($folderName -ieq $ExceptFolder) { continue }
        $startTask = $null
        try { $startTask = $rootChild.GetTask('Core-Start') } catch { $startTask = $null }
        if ($null -eq $startTask) { continue }
        foreach ($action in $startTask.Definition.Actions) {
            if ([int]$action.Type -ne 0) { continue }
            if (([string]$action.Arguments).IndexOf($LauncherPath, [System.StringComparison]::OrdinalIgnoreCase) -ge 0) {
                $found += $folderName
                break
            }
        }
    }
    return $found
}

# Registers \TaskFolder\Core-Start (boot trigger, through a generated launcher) and
# \TaskFolder\Core-Stop (on demand) for the current account, S4U. Returns the launcher's path.
# StartEnabled, BootTriggerEnabled and StopEnabled keep the state of earlier tasks (the installer's
# switch-over); a new registration has all three on.
function Register-GridcoinCoreTasks {
    param(
        [Parameter(Mandatory = $true)][string]$CorePath,
        [Parameter(Mandatory = $true)][string]$DataDir,
        [string]$TaskFolder = 'Gridcoin',
        [bool]$StartEnabled = $true,
        [bool]$BootTriggerEnabled = $true,
        [bool]$StopEnabled = $true
    )
    $userSid = Get-GridcoinCurrentSid

    # The data directory is passed only when it is not the default. With no -datadir the core
    # resolves and creates the default itself, with its owner-only DACL; with one it refuses a
    # missing directory.
    $defaultDataDir = Join-Path $env:APPDATA 'GridcoinResearch'
    $dataDirText = ''
    $stopText = 'stop'
    if ($DataDir -ne $defaultDataDir) {
        $dataDirText = ' -datadir="' + ($DataDir -replace '%', '%%') + '"'
        $stopText = '-datadir="' + $DataDir + '" stop'
    }

    # The launcher retries once, after two minutes, when the core exits non-zero (Task Scheduler's
    # own restart does not fire on a program's exit code). It lives beside the scripts, which the
    # caller has checked, and its stderr goes to %TEMP% so that the core creates and protects its own
    # data directory. cmd reads the file in the OEM codepage and expands % even inside quotes.
    $launcher = Get-GridcoinLauncherPath
    $coreText = $CorePath -replace '%', '%%'
    $errLog = '%TEMP%\gridcoin-core-start.err.log'
    $daemonLine = '"' + $coreText + '"' + $dataDirText + ' -multiprocess -autounlockblob 2>>"' + $errLog + '"'
    $launcherLines = @(
        '@echo off',
        'rem BEGIN GENERATED',
        'rem Gridcoin Core-Start launcher, written by Install-GridcoinCoreTask.ps1. Do not edit it.',
        'rem A clean run blocks until shutdown and exits 0. After a failure it waits two minutes and tries once more.',
        $daemonLine,
        'if %ERRORLEVEL% EQU 0 goto end',
        ('echo ==== attempt 1 failed exit %ERRORLEVEL% at %DATE% %TIME%, retrying in 2 min ==== >>"' + $errLog + '"'),
        '"%SystemRoot%\System32\PING.EXE" -n 121 127.0.0.1 >nul',
        $daemonLine,
        ':end',
        'rem END GENERATED'
    )
    # cmd reads the launcher in the OEM code page, and -Encoding Oem replaces a character it cannot
    # represent with '?', which would point the launcher at another path. Refuse such a path among
    # those written into it: the core's, and the data directory's when it is not the default. The
    # launcher's own path goes to Task Scheduler, in Unicode.
    $oemPage = [System.Globalization.CultureInfo]::CurrentCulture.TextInfo.OEMCodePage
    $oemEncoding = [System.Text.Encoding]::GetEncoding($oemPage)
    $inLauncher = @($CorePath)
    if ($dataDirText) { $inLauncher += $DataDir }
    foreach ($oemCheck in $inLauncher) {
        if ($oemEncoding.GetString($oemEncoding.GetBytes($oemCheck)) -cne $oemCheck) {
            throw ("The path '$oemCheck' has characters the console code page ($oemPage) cannot hold, and the " +
                   "launcher is written in that code page. Install where the path has only characters of the console code page.")
        }
    }
    Set-Content -LiteralPath $launcher -Encoding Oem -Value $launcherLines

    $service = Get-GridcoinTaskService
    $root = $service.GetFolder('\')
    $folder = $null
    try {
        $folder = $service.GetFolder("\$TaskFolder")
    } catch {
        $folderSddl = 'D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)'
        $folder = $root.CreateFolder($TaskFolder, $folderSddl)
    }

    $startDefinition = New-GridcoinTaskDefinition -SchedulerService $service -TimeLimit 'PT0S' `
        -Description 'Start the Gridcoin multiprocess core at boot (S4U, no stored password).'
    $startDefinition.Settings.Enabled = $StartEnabled
    $trigger = $startDefinition.Triggers.Create(8)
    $trigger.Enabled = $BootTriggerEnabled
    # The S4U logon of a domain account needs a domain controller, which is often not reachable in
    # the first seconds after boot: Task Scheduler then fails the logon and does not try again. A
    # minute's delay lets the network come up, and the restarts cover a slower one.
    $trigger.Delay = 'PT1M'
    $startDefinition.Settings.RestartInterval = 'PT1M'
    $startDefinition.Settings.RestartCount = 5
    $startAction = $startDefinition.Actions.Create(0)
    $startAction.Path = '%SystemRoot%\System32\cmd.exe'
    $startAction.Arguments = '/s /c ""' + $launcher + '""'
    Register-GridcoinTask -TargetFolder $folder -RegisteredName 'Core-Start' -TaskDefinition $startDefinition -AccountSid $userSid

    # Core-Stop has no trigger, on purpose: Task Scheduler starts no task once Windows' shutdown is in
    # progress, and the core stops itself on Windows' shutdown signal.
    $stopDefinition = New-GridcoinTaskDefinition -SchedulerService $service -TimeLimit 'PT10M' `
        -Description 'Graceful RPC stop of the Gridcoin core. Start this task to stop the core; never end Core-Start.'
    $stopDefinition.Settings.Enabled = $StopEnabled
    $stopAction = $stopDefinition.Actions.Create(0)
    $stopAction.Path = $CorePath
    $stopAction.Arguments = $stopText
    Register-GridcoinTask -TargetFolder $folder -RegisteredName 'Core-Stop' -TaskDefinition $stopDefinition -AccountSid $userSid

    foreach ($taskName in @('Core-Start', 'Core-Stop')) {
        $registered = $folder.GetTask($taskName)
        if ([int]$registered.Definition.Principal.LogonType -ne 2) {
            throw "\$TaskFolder\$taskName was registered with logon type $($registered.Definition.Principal.LogonType), not S4U (2)."
        }
    }
    return $launcher
}

# Unregisters the tasks of the given folder that run as the current account with logon
# type 1, the registration earlier versions made. Returns their names.
function Remove-GridcoinPasswordLogonTasks {
    param([string]$TaskFolder = 'Gridcoin')
    $removed = @()
    $service = Get-GridcoinTaskService
    $folder = $null
    try { $folder = $service.GetFolder("\$TaskFolder") } catch { return $removed }
    $userSid = Get-GridcoinCurrentSid
    foreach ($taskName in @('Core-Start', 'Core-Stop')) {
        $existing = $null
        try { $existing = $folder.GetTask($taskName) } catch { $existing = $null }
        if ($null -eq $existing) { continue }
        if ([int]$existing.Definition.Principal.LogonType -ne 1) { continue }
        if ((Get-GridcoinTaskPrincipalSid $existing) -ne $userSid) { continue }
        $folder.DeleteTask($taskName, 0)
        $removed += $taskName
    }
    return $removed
}

# Ends and unregisters the resident autounlock helper that earlier versions ran: the
# \Gridcoin\Autounlock task, and this user's powershell.exe processes that run its script. Returns
# the old DPAPI file's path when that file exists, else $null. The file is read from the task's
# arguments, else it is <datadir>\autounlock\passphrase.cred.
#
# With AllAccounts (the installer, which replaces the release for every account) the task and the
# helpers go whichever account they run as. The path returned is still this account's file only:
# another account's file is never read here.
function Remove-GridcoinLegacyAutounlock {
    param(
        [Parameter(Mandatory = $true)][string]$DataDir,
        [string]$TaskFolder = 'Gridcoin',
        [switch]$AllAccounts
    )
    $legacyPath = Join-Path $DataDir 'autounlock\passphrase.cred'
    $mySid = Get-GridcoinCurrentSid

    $service = Get-GridcoinTaskService
    $folder = $null
    try { $folder = $service.GetFolder("\$TaskFolder") } catch { $folder = $null }
    if ($null -ne $folder) {
        $oldTask = $null
        try { $oldTask = $folder.GetTask('Autounlock') } catch { $oldTask = $null }
        $taskIsMine = $false
        if ($null -ne $oldTask) { $taskIsMine = ((Get-GridcoinTaskPrincipalSid $oldTask) -eq $mySid) }
        if ($null -ne $oldTask -and -not $taskIsMine -and -not $AllAccounts) {
            Write-Host ("The \$TaskFolder\Autounlock task belongs to another account (" + (Get-GridcoinTaskPrincipalSid $oldTask) + "); left in place.")
            $oldTask = $null
        }
        if ($null -ne $oldTask) {
            if ($taskIsMine) {
                foreach ($oldAction in $oldTask.Definition.Actions) {
                    $actionText = [string]$oldAction.Arguments
                    if ($actionText -match '-CredPath\s+"([^"]+)"') { $legacyPath = $Matches[1] }
                }
            }
            try { $oldTask.Stop(0) } catch { Write-Verbose 'the Autounlock task was not running' }
            $folder.DeleteTask('Autounlock', 0)
        }
    }

    $helpers = @(Get-CimInstance -ClassName Win32_Process -Filter "Name = 'powershell.exe'")
    foreach ($helper in $helpers) {
        if ($helper.ProcessId -eq $PID) { continue }
        if (-not $helper.CommandLine) { continue }
        if ($helper.CommandLine -notlike '*Invoke-GridcoinAutounlock.ps1*') { continue }
        if (-not $AllAccounts) {
            $ownerSid = $null
            try { $ownerSid = (Invoke-CimMethod -InputObject $helper -MethodName GetOwnerSid).Sid } catch { $ownerSid = $null }
            if ($ownerSid -ne $mySid) { continue }
        }
        Stop-Process -Id $helper.ProcessId -Force
    }

    if (Test-Path -LiteralPath $legacyPath) { return $legacyPath }
    return $null
}

# A configuration value as the core reads it as a boolean (InterpretBool, src/util/system.cpp:84):
# an empty value is true; otherwise ParseInt32 (src/util/strencodings.cpp:330) must take the whole
# string as one integer, with an optional single leading + and an optional -, and the value is true
# when that integer is not zero. Anything else ("abc", "1abc", "1.5", a number too large for 32
# bits) is false. It is not atoi: trailing characters make the whole value false.
function ConvertTo-GridcoinConfBool {
    param([string]$Value)
    if ($Value.Length -eq 0) { return $true }
    $digits = $Value
    if ($digits.StartsWith('+-', [System.StringComparison]::Ordinal)) { return $false }
    if ($digits.StartsWith('+', [System.StringComparison]::Ordinal)) { $digits = $digits.Substring(1) }
    if ($digits -cnotmatch '^-?[0-9]+\z') { return $false }
    [int]$number = 0
    if (-not [int]::TryParse($digits, [System.Globalization.NumberStyles]::AllowLeadingSign,
            [System.Globalization.CultureInfo]::InvariantCulture, [ref]$number)) {
        return $false
    }
    return ($number -ne 0)
}

# The options of configuration file text, in order, as GetConfigOptions and InterpretOption read
# it (src/util/system.cpp:1094 and :118): everything from # to the end of the line is a comment; a
# line is trimmed of space, tab, CR and LF; a line [name] starts a section whose name prefixes the
# keys after it until the next section line (nothing returns to the top level); a line without an
# = (or one that starts with -) is a parse error in the core, which then does not start, and is
# skipped here; key and value are trimmed at the first =. The name (prefix and key) is split at its
# first . into a section and a key, and a key that starts with "no" is the negated form: the
# option is the boolean "false" when its value reads as true, and the boolean "true" (a double
# negative) when it reads as false. Keys are case-sensitive. Each option has Section, Key, IsBool,
# Bool and Str.
function Read-GridcoinConfOptions {
    param([string[]]$Lines)
    $trimChars = [char[]](' ', "`t", "`r", "`n")
    $options = @()
    $prefix = ''
    foreach ($rawLine in @($Lines)) {
        $text = [string]$rawLine
        $hashAt = $text.IndexOf([char]'#')
        if ($hashAt -ge 0) { $text = $text.Substring(0, $hashAt) }
        $text = $text.Trim($trimChars)
        if ($text.Length -eq 0) { continue }
        if ([int]$text[0] -eq 0x5B -and [int]$text[$text.Length - 1] -eq 0x5D) {
            $prefix = $text.Substring(1, $text.Length - 2) + '.'
            continue
        }
        if ([int]$text[0] -eq 0x2D) { continue }
        $equalsAt = $text.IndexOf([char]'=')
        if ($equalsAt -lt 0) { continue }
        $name = $prefix + $text.Substring(0, $equalsAt).Trim($trimChars)
        $optionValue = $text.Substring($equalsAt + 1).Trim($trimChars)
        $section = ''
        $key = $name
        $dotAt = $name.IndexOf([char]'.')
        if ($dotAt -ge 0) {
            $section = $name.Substring(0, $dotAt)
            $key = $name.Substring($dotAt + 1)
        }
        $isBool = $false
        $boolValue = $false
        if ($key.StartsWith('no', [System.StringComparison]::Ordinal)) {
            $key = $key.Substring(2)
            $isBool = $true
            $boolValue = -not (ConvertTo-GridcoinConfBool $optionValue)
        }
        $options += [pscustomobject]@{ Section = $section; Key = $key; IsBool = $isBool; Bool = $boolValue; Str = $optionValue }
    }
    return $options
}

# The network the options select, as the core decides -testnet (ArgsManager::GetChainName,
# src/util/system.cpp:1376, with GetSetting, src/util/settings.cpp:114, called with the chain name
# flag): only options of the top-level section count, so a testnet line after a [main] or [test]
# line (it is main.testnet or test.testnet) selects nothing; the LAST top-level testnet option
# wins, not the first as for other options; and when that last one is a negated form with a true
# reading (notestnet=1) it is skipped, which leaves the network main. A negated form with a false
# reading (notestnet=0) is a double negative and means testnet. Otherwise the value is read as a
# boolean (ConvertTo-GridcoinConfBool).
function Get-GridcoinNetworkFromOptions {
    param([object[]]$Options)
    $found = @()
    foreach ($option in @($Options)) {
        if ($null -eq $option) { continue }
        # Ordinal, as the core compares: -ceq compares by culture, which skips a byte order mark
        # that the core keeps at the start of a first key.
        if ($option.Section.Length -eq 0 -and [string]::Equals($option.Key, 'testnet', [System.StringComparison]::Ordinal)) {
            $found += $option
        }
    }
    if ($found.Count -eq 0) { return 'main' }
    $last = $found[$found.Count - 1]
    if ($last.IsBool) {
        if ($last.Bool) { return 'test' }
        return 'main'
    }
    if (ConvertTo-GridcoinConfBool $last.Str) { return 'test' }
    return 'main'
}

# 'test' or 'main' for the text of one gridcoinresearch.conf, without any file it includes.
function Get-GridcoinConfNetworkFromLines {
    param([string[]]$Lines)
    return (Get-GridcoinNetworkFromOptions -Options @(Read-GridcoinConfOptions -Lines $Lines))
}

# The lines of a configuration file the way the core reads it: bytes as UTF-8 without dropping a
# byte order mark (the core keeps it, so the key of a first line that starts with one is not the
# key it looks like), split at LF (CR is trimmed later).
function Read-GridcoinConfLines {
    param([Parameter(Mandatory = $true)][string]$Path)
    $confText = [System.Text.Encoding]::UTF8.GetString([System.IO.File]::ReadAllBytes($Path))
    return ($confText -split "`n")
}

# The network a data directory's core runs on, as the core decides it: gridcoinresearch.conf there,
# then the files its includeconf lines name. The core reads those after the file itself
# (ArgsManager::ReadConfigFiles, src/util/system.cpp:1265): the includeconf lines of the top level
# and of the section of the network the file itself selects, a noincludeconf line dropping the
# ones before it, relative names taken from the data directory, and an included file's own
# includeconf lines ignored. A file that cannot be opened stops the core, so it is skipped here;
# so is an include whose name is not a valid path. The main file that exists but cannot be opened
# is treated as absent, as the core does (ReadConfigFiles opens it with ReadConfigFileContents:
# a failed fopen is ConfigFileRead::NotOpened, which is fine unless -conf was given, and the core
# goes on without a file). Nothing here throws for a read problem, so a conf that cannot be read
# never costs the user the old passphrase file.
# The options of the included files come after those of the file, so they win for a repeated testnet.
function Get-GridcoinConfNetwork {
    param([Parameter(Mandatory = $true)][string]$DataDir)
    $confForNetwork = Join-Path $DataDir 'gridcoinresearch.conf'
    if (-not (Test-Path -LiteralPath $confForNetwork -PathType Leaf)) { return 'main' }
    $mainLines = $null
    try {
        $mainLines = Read-GridcoinConfLines -Path $confForNetwork
    } catch {
        return 'main'
    }
    $allOptions = @(Read-GridcoinConfOptions -Lines $mainLines)

    $ownNetwork = Get-GridcoinNetworkFromOptions -Options $allOptions
    $includeNames = @()
    foreach ($includeSection in @($ownNetwork, '')) {
        $listed = @()
        foreach ($option in $allOptions) {
            if ([string]::Equals($option.Section, $includeSection, [System.StringComparison]::Ordinal) -and
                [string]::Equals($option.Key, 'includeconf', [System.StringComparison]::Ordinal)) { $listed += $option }
        }
        $firstUsed = 0
        for ($index = 0; $index -lt $listed.Count; $index++) {
            if ($listed[$index].IsBool -and -not $listed[$index].Bool) { $firstUsed = $index + 1 }
        }
        for ($index = $firstUsed; $index -lt $listed.Count; $index++) {
            if (-not $listed[$index].IsBool) { $includeNames += $listed[$index].Str }
        }
    }
    foreach ($includeName in $includeNames) {
        try {
            $includePath = $includeName
            if (-not [System.IO.Path]::IsPathRooted($includeName)) { $includePath = Join-Path $DataDir $includeName }
            if (-not (Test-Path -LiteralPath $includePath -PathType Leaf)) { continue }
            $allOptions += @(Read-GridcoinConfOptions -Lines (Read-GridcoinConfLines -Path $includePath))
        } catch {
            # not a valid path, or not readable: skipped, as a missing file is
            continue
        }
    }
    return (Get-GridcoinNetworkFromOptions -Options $allOptions)
}

# Whether the account Sid is a direct member of the local Administrators group: task mode is for an
# administrator's own wallet. False when it is not or when it cannot be told, so that a standard
# account is never pointed at task mode; a member only through a domain group counts as not one.
function Test-GridcoinLocalAdministrator {
    param([string]$Sid)
    if (-not $Sid) { return $false }
    try {
        $adminsName = (New-Object System.Security.Principal.SecurityIdentifier('S-1-5-32-544')).Translate(
            [System.Security.Principal.NTAccount]).Value.Split('\')[-1]
        $adminsGroup = [ADSI]('WinNT://./' + $adminsName + ',group')
        foreach ($member in @($adminsGroup.psbase.Invoke('Members'))) {
            $sidBytes = $member.GetType().InvokeMember('objectSid', 'GetProperty', $null, $member, $null)
            if ((New-Object System.Security.Principal.SecurityIdentifier($sidBytes, 0)).Value -eq $Sid) { return $true }
        }
    } catch {
        return $false
    }
    return $false
}

# The account signed in to this process's session, as the owner of its explorer.exe, or $null when
# the session has none (a service or a remote shell).
function Get-GridcoinSessionUserSid {
    $sessionId = (Get-Process -Id $PID).SessionId
    foreach ($shell in @(Get-CimInstance -ClassName Win32_Process -Filter "Name = 'explorer.exe' AND SessionId = $sessionId")) {
        try {
            $shellOwner = (Invoke-CimMethod -InputObject $shell -MethodName GetOwnerSid).Sid
            if ($shellOwner) { return $shellOwner }
        } catch {
            Write-Verbose 'the owner of explorer.exe could not be read'
        }
    }
    return $null
}

# Deletes an earlier version's autounlock passphrase file and the old helper's two logs beside it.
# A file 5.5.2.0 wrote from an elevated console belongs to Administrators but grants only its
# account, so when the delete is refused and the owner is privileged, Administrators are first given
# the right to delete it. Returns what is left, each with why; empty when nothing is.
# The path comes from a task, which the account it runs as can rewrite, so only a file named
# passphrase.cred is deleted, as the default an earlier version used is. With -AccountSid, for
# another account's task, each file must be a plain file that account owns, and nothing is given
# the right to delete it.
function Remove-GridcoinLegacyPassphraseFile {
    param(
        [Parameter(Mandatory = $true)][string]$CredPath,
        [string]$AccountSid
    )
    $left = @()
    if ((Split-Path -Leaf $CredPath) -ne 'passphrase.cred') {
        return @("$CredPath (not deleted: an earlier version's passphrase file is named passphrase.cred)")
    }
    $oldDirectory = Split-Path -Parent $CredPath
    foreach ($oldFile in @($CredPath, (Join-Path $oldDirectory 'autounlock.log'), (Join-Path $oldDirectory 'autounlock.log.1'))) {
        if ($PSBoundParameters.ContainsKey('AccountSid')) {
            try {
                $item = Get-Item -LiteralPath $oldFile -Force -ErrorAction SilentlyContinue
                if ($null -eq $item) { continue }
                if ($item.PSIsContainer -or ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                    $left += "$oldFile (not deleted: it is not a plain file)"
                    continue
                }
                $owner = (Get-Acl -LiteralPath $oldFile).GetOwner([System.Security.Principal.SecurityIdentifier]).Value
                if (-not $AccountSid -or $owner -ne $AccountSid) {
                    $left += "$oldFile (not deleted: it belongs to $(Get-GridcoinAccountName $owner), not to the account whose task named it)"
                    continue
                }
                [System.IO.File]::Delete($oldFile)
            } catch {
                $left += "$oldFile ($($_.Exception.Message))"
            }
            continue
        }
        try {
            # No error for a file that does not exist; one for a directory that does not.
            [System.IO.File]::Delete($oldFile)
        } catch {
            $failure = $_.Exception
            if ($failure.InnerException) { $failure = $failure.InnerException }
            if ($failure -is [System.IO.DirectoryNotFoundException]) { continue }
            try {
                $oldAcl = Get-Acl -LiteralPath $oldFile
                $oldOwner = $oldAcl.GetOwner([System.Security.Principal.SecurityIdentifier]).Value
                if ((Get-GridcoinPrivilegedSids) -notcontains $oldOwner) { throw $failure.Message }
                $oldAcl.AddAccessRule((New-Object System.Security.AccessControl.FileSystemAccessRule(
                    (New-Object System.Security.Principal.SecurityIdentifier('S-1-5-32-544')), 'Delete', 'Allow')))
                Set-Acl -LiteralPath $oldFile -AclObject $oldAcl
                [System.IO.File]::Delete($oldFile)
            } catch {
                $left += "$oldFile ($($failure.Message))"
            }
        }
    }
    return $left
}

# Compiles C# for Add-Type in a directory that only SYSTEM and Administrators can write. Windows
# PowerShell compiles through the system's C# compiler, which writes the source and the assembly it
# then loads into the temporary directory; the account's own %TEMP% can be written by its processes
# that are not elevated. The directory is new, under Program Files, owned by Administrators with a
# protected list, checked like a helper before use, and removed afterwards. Elevated only.
function Add-GridcoinCompiledType {
    param(
        [Parameter(Mandatory = $true)][string]$TypeDefinition,
        [string[]]$ReferencedAssemblies = @()
    )
    $administrators = New-Object System.Security.Principal.SecurityIdentifier('S-1-5-32-544')
    $security = New-Object System.Security.AccessControl.DirectorySecurity
    $security.SetOwner($administrators)
    $security.SetAccessRuleProtection($true, $false)
    foreach ($sid in @('S-1-5-18', 'S-1-5-32-544')) {
        $security.AddAccessRule((New-Object System.Security.AccessControl.FileSystemAccessRule(
            (New-Object System.Security.Principal.SecurityIdentifier($sid)), 'FullControl', 'ContainerInherit, ObjectInherit',
            'None', 'Allow')))
    }
    $directory = Join-Path $env:ProgramFiles ('gridcoin-compile-' + [guid]::NewGuid().ToString('N'))
    [System.IO.Directory]::CreateDirectory($directory, $security) | Out-Null
    try {
        $vector = Get-GridcoinWriteVector -Path $directory -AllowedSids (Get-GridcoinPrivilegedSids)
        if ($vector) { throw "REFUSED: the directory to compile in can be written by an unprivileged account: $vector" }
        $saved = @($env:TMP, $env:TEMP)
        try {
            $env:TMP = $directory
            $env:TEMP = $directory
            Add-Type -TypeDefinition $TypeDefinition -ReferencedAssemblies $ReferencedAssemblies
        } finally {
            $env:TMP = $saved[0]
            $env:TEMP = $saved[1]
        }
    } finally {
        Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# The secret path of the autounlock migration lives in C#, so that no passphrase byte is ever bound
# to a PowerShell parameter or held in a PowerShell variable (module logging, event 4103, records
# bound parameters). PowerShell passes paths, the network name and SIDs only.
#
# Convert reads the old file with this user's DPAPI key, cleans the passphrase as the earlier
# helper did (CleanPassphrase), protects it with
# CryptProtectData and the flags the core's own writer uses (src/wallet/autounlock_dpapi.cpp:
# CRYPTPROTECT_UI_FORBIDDEN | CRYPTPROTECT_AUDIT | CRYPTPROTECT_LOCAL_MACHINE, the description
# "Gridcoin autounlock", the entropy "gridcoin-autounlock-v1:<network>"), builds the layout of
# src/wallet/autounlock_blob.h ("GRCAUB01", scope 1, name length, name, DPAPI blob), and writes it
# beside the target under a temporary name with a protected DACL, then moves it into place. The
# plaintext and the unmanaged copies are zeroed; the output blob is freed with LocalFree.
function Add-GridcoinDpapiType {
    if ('GridcoinDpapi' -as [type]) { return }
    $source = @'
using System;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.AccessControl;
using System.Security.Cryptography;
using System.Security.Principal;
using System.Text;

public static class GridcoinDpapi
{
    [StructLayout(LayoutKind.Sequential)]
    private struct DataBlob
    {
        public int cbData;
        public IntPtr pbData;
    }

    [DllImport("crypt32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern bool CryptProtectData(ref DataBlob dataIn, string description, ref DataBlob entropy,
        IntPtr reserved, IntPtr prompt, int flags, out DataBlob dataOut);

    [DllImport("kernel32.dll")]
    private static extern IntPtr LocalFree(IntPtr memory);

    [DllImport("advapi32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern bool ConvertStringSecurityDescriptorToSecurityDescriptorW(string sddl, int revision,
        out IntPtr descriptor, IntPtr size);

    [DllImport("advapi32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern bool ConvertSecurityDescriptorToStringSecurityDescriptorW(IntPtr descriptor, int revision,
        int information, out IntPtr sddl, IntPtr size);

    [DllImport("advapi32.dll", SetLastError = true)]
    private static extern bool GetSecurityDescriptorSacl(IntPtr descriptor, out bool present, out IntPtr sacl,
        out bool defaulted);

    [DllImport("advapi32.dll")]
    private static extern int SetSecurityInfo(SafeHandle handle, int objectType, int information, IntPtr owner,
        IntPtr group, IntPtr dacl, IntPtr sacl);

    [DllImport("advapi32.dll")]
    private static extern int GetSecurityInfo(SafeHandle handle, int objectType, int information, IntPtr owner,
        IntPtr group, IntPtr dacl, IntPtr sacl, out IntPtr descriptor);

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern bool MoveFileExW(string from, string to, int flags);

    private const int SE_FILE_OBJECT = 1;
    private const int LABEL_SECURITY_INFORMATION = 0x10;
    private const int MOVEFILE_REPLACE_EXISTING = 0x1;
    private const int MOVEFILE_WRITE_THROUGH = 0x8;
    // Medium integrity, with no read up and no write up, as the core labels the file it writes.
    private const string MEDIUM_NO_READ_UP = "S:(ML;;NRNW;;;ME)";

    private const int UI_FORBIDDEN = 0x1;
    private const int LOCAL_MACHINE = 0x4;
    private const int AUDIT = 0x10;
    private const int MAX_BYTES = 65536;

    private static DataBlob Copy(byte[] bytes)
    {
        DataBlob blob = new DataBlob();
        blob.cbData = bytes.Length;
        blob.pbData = Marshal.AllocHGlobal(bytes.Length);
        Marshal.Copy(bytes, 0, blob.pbData, bytes.Length);
        return blob;
    }

    private static void Wipe(DataBlob blob)
    {
        if (blob.pbData == IntPtr.Zero) return;
        Marshal.Copy(new byte[blob.cbData], 0, blob.pbData, blob.cbData);
        Marshal.FreeHGlobal(blob.pbData);
    }

    private static byte[] Protect(byte[] plain, byte[] entropy)
    {
        DataBlob input = Copy(plain);
        DataBlob extra = Copy(entropy);
        DataBlob output = new DataBlob();
        try
        {
            if (!CryptProtectData(ref input, "Gridcoin autounlock", ref extra, IntPtr.Zero, IntPtr.Zero,
                    UI_FORBIDDEN | AUDIT | LOCAL_MACHINE, out output))
            {
                throw new Win32Exception(Marshal.GetLastWin32Error());
            }
            byte[] result = new byte[output.cbData];
            Marshal.Copy(output.pbData, result, 0, output.cbData);
            return result;
        }
        finally
        {
            Wipe(input);
            Wipe(extra);
            if (output.pbData != IntPtr.Zero) LocalFree(output.pbData);
        }
    }

    // The passphrase bytes the way the 5.5.2.0 helper read them (Read-DpapiPassphrase): decoded as
    // UTF-8, one leading byte order mark (U+FEFF) dropped, trailing CR and LF characters removed
    // and nothing else, and an empty or whitespace-only result refused. The text is held only in
    // a char array and the result in a byte array, both cleared by their owners, so no string
    // ever holds the secret. The result is the UTF-8 of the cleaned text, the bytes the helper
    // sent to the core, so a passphrase that unlocked under the helper unlocks after conversion.
    private static byte[] CleanPassphrase(byte[] plain)
    {
        char[] chars = new char[Encoding.UTF8.GetMaxCharCount(plain.Length)];
        try
        {
            int count = Encoding.UTF8.GetChars(plain, 0, plain.Length, chars, 0);
            int start = 0;
            if (count > 0 && chars[0] == '\uFEFF') start = 1;
            int end = count;
            while (end > start && (chars[end - 1] == '\r' || chars[end - 1] == '\n')) end--;
            bool visible = false;
            for (int i = start; i < end; i++)
            {
                if (!char.IsWhiteSpace(chars[i]))
                {
                    visible = true;
                    break;
                }
            }
            if (!visible) throw new InvalidDataException("the old autounlock file holds an empty or whitespace-only passphrase");
            return Encoding.UTF8.GetBytes(chars, start, end - start);
        }
        finally
        {
            Array.Clear(chars, 0, chars.Length);
        }
    }

    public static void Convert(string oldPath, string newPath, string network, string[] sids)
    {
        if (network.Length < 1 || network.Length > 16) throw new ArgumentException("network name length");
        foreach (char letter in network)
        {
            if (letter < 'a' || letter > 'z') throw new ArgumentException("network name letters");
        }

        byte[] plain = null;
        byte[] secret = null;
        byte[] entropy = Encoding.ASCII.GetBytes("gridcoin-autounlock-v1:" + network);
        try
        {
            plain = ProtectedData.Unprotect(File.ReadAllBytes(oldPath), null, DataProtectionScope.CurrentUser);
            secret = CleanPassphrase(plain);
            byte[] dpapi = Protect(secret, entropy);

            byte[] name = Encoding.ASCII.GetBytes(network);
            byte[] magic = Encoding.ASCII.GetBytes("GRCAUB01");
            byte[] file = new byte[magic.Length + 2 + name.Length + dpapi.Length];
            Buffer.BlockCopy(magic, 0, file, 0, magic.Length);
            file[magic.Length] = 1;
            file[magic.Length + 1] = (byte)name.Length;
            Buffer.BlockCopy(name, 0, file, magic.Length + 2, name.Length);
            Buffer.BlockCopy(dpapi, 0, file, magic.Length + 2 + name.Length, dpapi.Length);
            if (file.Length > MAX_BYTES) throw new InvalidDataException("the encrypted passphrase is too large");

            WriteProtected(newPath, file, sids);
        }
        finally
        {
            if (plain != null) Array.Clear(plain, 0, plain.Length);
            if (secret != null) Array.Clear(secret, 0, secret.Length);
            Array.Clear(entropy, 0, entropy.Length);
        }
    }

    private static void WriteProtected(string path, byte[] file, string[] sids)
    {
        string directory = Path.GetDirectoryName(path);
        if (!Directory.Exists(directory))
        {
            string parent = Path.GetDirectoryName(directory);
            if (!Directory.Exists(parent)) Directory.CreateDirectory(parent);
            DirectorySecurity directorySecurity = new DirectorySecurity();
            directorySecurity.SetAccessRuleProtection(true, false);
            foreach (string sid in sids)
            {
                directorySecurity.AddAccessRule(new FileSystemAccessRule(new SecurityIdentifier(sid),
                    FileSystemRights.FullControl, InheritanceFlags.ContainerInherit | InheritanceFlags.ObjectInherit,
                    PropagationFlags.None, AccessControlType.Allow));
            }
            Directory.CreateDirectory(directory, directorySecurity);
        }

        FileSecurity fileSecurity = new FileSecurity();
        fileSecurity.SetAccessRuleProtection(true, false);
        foreach (string sid in sids)
        {
            fileSecurity.AddAccessRule(new FileSystemAccessRule(new SecurityIdentifier(sid),
                FileSystemRights.FullControl, AccessControlType.Allow));
        }

        string temporary = path + ".tmp";
        if (File.Exists(temporary)) File.Delete(temporary);
        try
        {
            // WRITE_OWNER and READ_CONTROL to set the label and read it back.
            using (FileStream stream = new FileStream(temporary, FileMode.CreateNew,
                FileSystemRights.Write | FileSystemRights.Delete | FileSystemRights.TakeOwnership |
                FileSystemRights.ReadPermissions, FileShare.None, 4096, FileOptions.WriteThrough, fileSecurity))
            {
                // Labelled before anything is written: a Low-integrity process of the account, which
                // the list admits, cannot read it.
                SetMediumNoReadUp(stream.SafeFileHandle);
                stream.Write(file, 0, file.Length);
                stream.Flush();
            }
            // Renamed over a file there, which keeps this file's own list; File.Replace would keep
            // the replaced file's.
            if (!MoveFileExW(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                throw new Win32Exception(Marshal.GetLastWin32Error());
            }
        }
        catch
        {
            if (File.Exists(temporary)) File.Delete(temporary);
            throw;
        }
    }

    private static void SetMediumNoReadUp(SafeHandle handle)
    {
        IntPtr descriptor;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(MEDIUM_NO_READ_UP, 1, out descriptor, IntPtr.Zero))
        {
            throw new Win32Exception(Marshal.GetLastWin32Error());
        }
        try
        {
            bool present;
            bool defaulted;
            IntPtr sacl;
            if (!GetSecurityDescriptorSacl(descriptor, out present, out sacl, out defaulted) || !present)
            {
                throw new Win32Exception(Marshal.GetLastWin32Error());
            }
            int set = SetSecurityInfo(handle, SE_FILE_OBJECT, LABEL_SECURITY_INFORMATION, IntPtr.Zero, IntPtr.Zero,
                IntPtr.Zero, sacl);
            if (set != 0) throw new Win32Exception(set);
        }
        finally
        {
            LocalFree(descriptor);
        }

        IntPtr read;
        int got = GetSecurityInfo(handle, SE_FILE_OBJECT, LABEL_SECURITY_INFORMATION, IntPtr.Zero, IntPtr.Zero,
            IntPtr.Zero, IntPtr.Zero, out read);
        if (got != 0) throw new Win32Exception(got);
        try
        {
            IntPtr text;
            if (!ConvertSecurityDescriptorToStringSecurityDescriptorW(read, 1, LABEL_SECURITY_INFORMATION, out text,
                IntPtr.Zero))
            {
                throw new Win32Exception(Marshal.GetLastWin32Error());
            }
            string label = Marshal.PtrToStringUni(text);
            LocalFree(text);
            if (label.IndexOf("ML;", StringComparison.Ordinal) < 0 || label.IndexOf("NR", StringComparison.Ordinal) < 0 ||
                label.IndexOf(";ME)", StringComparison.Ordinal) < 0)
            {
                throw new InvalidDataException("the autounlock file did not keep its Medium integrity label (" + label + ")");
            }
        }
        finally
        {
            LocalFree(read);
        }
    }
}
'@
    Add-GridcoinCompiledType -TypeDefinition $source -ReferencedAssemblies 'System.Security'
}

# Carries autounlock across the upgrade: converts the old file (decrypted with the current user's
# DPAPI key, which only this user's own session can do) into the file the core reads for Network,
# then deletes the old file and the helper's two logs. Takes paths only: the passphrase never
# leaves the C# type. Throws when the conversion fails, and then leaves the old file in place.
# True when a file was converted.
function Convert-GridcoinLegacyAutounlockBlob {
    param(
        [Parameter(Mandatory = $true)][string]$OldPath,
        [Parameter(Mandatory = $true)][string]$DataDir,
        [Parameter(Mandatory = $true)][string]$Network
    )
    if (-not (Test-Path -LiteralPath $OldPath)) { return $false }
    if ($Network -cnotmatch '^[a-z]{1,16}$') { throw "The network name '$Network' is not 1 to 16 letters a-z." }
    Add-GridcoinDpapiType

    $netDir = $DataDir
    if ($Network -eq 'test') { $netDir = Join-Path $DataDir 'testnet' }
    $newPath = Join-Path $netDir 'autounlock\passphrase.dpapi'
    $aclSids = [string[]]@((Get-GridcoinCurrentSid), 'S-1-5-18', 'S-1-5-32-544')

    [GridcoinDpapi]::Convert($OldPath, $newPath, $Network, $aclSids)

    Remove-Item -LiteralPath $OldPath -Force
    $oldDirectory = Split-Path -Parent $OldPath
    foreach ($logName in @('autounlock.log', 'autounlock.log.1')) {
        $logPath = Join-Path $oldDirectory $logName
        if (Test-Path -LiteralPath $logPath) { Remove-Item -LiteralPath $logPath -Force }
    }
    return $true
}
