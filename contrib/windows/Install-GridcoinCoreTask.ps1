#Requires -Version 5.1
<#
.SYNOPSIS
    Register the Gridcoin multiprocess core as background Scheduled Tasks on Windows.

.DESCRIPTION
    Registers two discrete, single-purpose tasks under the \Gridcoin\ folder, both
    run-as the wallet user (never SYSTEM -- the IPC node.sock/ipc.cookie are
    owner-only NTFS-ACL'd and a SYSTEM core would lock the user's own GUI out):

      Core-Start : boot trigger -> a generated launcher (core-autostart.cmd) that runs
                   gridcoinresearchd.exe -datadir=<dd> -multiprocess and retries once
                   (after 2 min) on a non-zero exit -- the systemd Restart=on-failure analogue.
      Core-Stop  : gridcoinresearchd.exe -datadir=<dd> stop  (a GRACEFUL RPC stop
                   that flushes BDB/LevelDB). ON-DEMAND only -- used by manual stops
                   and by Update-GridcoinCore.ps1's upgrade engine.

    SEMANTICS GOTCHA: to stop the core you START the Core-Stop task. NEVER
    Stop-ScheduledTask the Core-Start task -- Task Scheduler's stop is a hard
    TerminateProcess (no flush = possible DB corruption).

    SHUTDOWN: Core-Stop carries NO OS-shutdown trigger. A scheduled task cannot make
    Windows wait, so an event-triggered stop is best-effort and gets cut short mid-flush
    (confirmed on-device). The reliable clean-shutdown primitive is a Group Policy
    Computer -> Shutdown script, which Windows runs synchronously and WAITS for (bounded
    by "Maximum wait time for Group Policy scripts", default 600s). Configure it with
    Set-GridcoinShutdownFlush.ps1 (opt-in). Having both a shutdown trigger AND the GP
    script would double-stop and race, so we ship only the GP-script path.

    It also adds an inbound Windows Firewall rule for the daemon (skip with
    -SkipFirewallRule): the headless core gets no interactive firewall prompt, and in
    multiprocess mode the core -- not the GUI -- does the P2P, so without a rule the node
    is outbound-only. Uninstall-GridcoinCoreTask.ps1 removes it.

    Autounlock (opt-in, DPAPI) is a SEPARATE task added by Set-GridcoinAutounlock.ps1
    (Plan 5); it is not registered here.

.NOTES
    Registering a run-as-user task with a stored password typically requires an
    ELEVATED PowerShell, and the account must hold "Log on as a batch job".
#>
[CmdletBinding()]
param(
    # Path to gridcoinresearchd.exe. Default: resolved below across both known layouts --
    # the NSIS installer's ...\GridcoinResearch\daemon\gridcoinresearchd.exe (scripts in
    # ...\GridcoinResearch\windows\), and a build tree where the exe sits one level up.
    [string]$CorePath,

    # The datadir the core owns (node.sock lives in it). Baked into both the task
    # and the GUI shortcut so the GUI attaches to the datadir the core is serving.
    # REQUIRED when -TaskUser is not the current user (see the guard below).
    [string]$DataDir,

    # The account the tasks run as. Default: the invoking interactive user.
    # DOMAIN\user or .\user form. "Install as admin, run as walletholder" is
    # supported by passing -TaskUser explicitly (then -DataDir is also required).
    [string]$TaskUser = "$env:USERDOMAIN\$env:USERNAME",

    # The TaskUser's *login* password (NOT the wallet passphrase). Required so the
    # tasks can run at boot with no interactive logon (LogonType=Password). Prompted
    # securely if omitted.
    [System.Security.SecureString]$TaskPassword,

    [string]$TaskFolder = 'Gridcoin',

    # Skip creating the inbound Windows Firewall rule for the daemon. By default this script
    # adds one: the headless core (gridcoinresearchd.exe) never triggers the interactive
    # firewall prompt the GUI gets on first run, so without a rule Windows silently blocks
    # inbound P2P and the node cannot accept incoming peers (outbound still works). In
    # multiprocess mode the *core* does the P2P, not the GUI, so the GUI's own rule does not
    # cover it. Pass -SkipFirewallRule if you manage the firewall separately.
    [switch]$SkipFirewallRule,

    # Firewall profiles the inbound rule applies to. Public is excluded by default (a P2P
    # listener open on untrusted networks is a deliberate choice); add 'Public' to include it.
    [ValidateSet('Domain', 'Private', 'Public')]
    [string[]]$FirewallProfile = @('Domain', 'Private')
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Resolve the default core path across both layouts (installer daemon\ subfolder first,
# then a build tree with the exe one level up). An explicit -CorePath is honored as-is.
if (-not $CorePath) {
    $CorePath = @(
        (Join-Path $PSScriptRoot '..\daemon\gridcoinresearchd.exe'),   # NSIS installer layout
        (Join-Path $PSScriptRoot '..\gridcoinresearchd.exe')           # exe alongside the scripts' parent
    ) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (-not $CorePath) { $CorePath = (Join-Path $PSScriptRoot '..\daemon\gridcoinresearchd.exe') }
}
if (-not (Test-Path -LiteralPath $CorePath)) {
    throw "gridcoinresearchd.exe not found at '$CorePath'. Pass -CorePath explicitly (installer layout: " +
          "...\GridcoinResearch\daemon\gridcoinresearchd.exe, with these scripts in ...\GridcoinResearch\windows\)."
}
$CorePath = (Resolve-Path -LiteralPath $CorePath).Path

# The default datadir (%APPDATA%) is evaluated in the INSTALLER's context. If a
# different TaskUser was given (install-as-admin, run-as-walletholder), that path
# points at the installer's profile -- which the TaskUser usually cannot read, so
# the core would abort or silently build a second, unused datadir. Require an
# explicit -DataDir in that case.
$currentUser = "$env:USERDOMAIN\$env:USERNAME"
if (-not $PSBoundParameters.ContainsKey('DataDir')) {
    if ($PSBoundParameters.ContainsKey('TaskUser') -and ($TaskUser -ne $currentUser)) {
        throw "You specified -TaskUser '$TaskUser' (not the current user '$currentUser'), so the " +
              "default datadir (this user's %APPDATA%) would be wrong. Pass -DataDir explicitly, e.g. " +
              "-DataDir 'C:\Users\<walletholder>\AppData\Roaming\GridcoinResearch'."
    }
    $DataDir = Join-Path $env:APPDATA 'GridcoinResearch'
}
if ($DataDir -match '"') {
    throw "-DataDir must not contain a double-quote character."
}
# A trailing backslash escapes the closing quote under CommandLineToArgvW (which both
# powershell.exe and the daemon use to split their command lines), so -datadir="C:\dd\"
# swallows the quote and corrupts every argument that follows it. It is also baked into the
# generated .cmd, where cmd.exe applies yet another quoting pass. Reject it rather than try
# to escape it correctly for both parsers at once.
if ($DataDir -match '\\$') {
    throw "-DataDir must not end with a backslash ('$DataDir'): a trailing backslash escapes the closing " +
          "quote when the command line is parsed, corrupting the arguments after it. Drop the trailing " +
          "backslash (and use a subdirectory rather than a bare drive root)."
}

if (-not $TaskPassword) {
    $cred = Get-Credential -UserName $TaskUser `
        -Message "Enter the Windows password for '$TaskUser' (the account the Gridcoin core will run as). This is NOT the wallet passphrase."
    $TaskUser = $cred.UserName
    $TaskPassword = $cred.Password
}

# ---------------------------------------------------------------------------
# SECURITY: Core-Start executes the generated core-autostart.cmd (in this script's own
# directory) as the wallet user, at boot, with no interactive session to notice. Anyone who
# can replace that .cmd -- or anything on the path to it -- gets arbitrary code run as the
# wallet user, which owns the wallet and (when autounlock is enabled) the DPAPI store that
# decrypts the wallet passphrase. Fail CLOSED unless every component is writable only by
# privileged principals or by the wallet user itself (who already owns the wallet, so that
# is not an escalation).
#
# Mirrors Get-NonAdminWriteVector / Get-NonAdminWriteVectorForPath in
# Set-GridcoinShutdownFlush.ps1 (whose script runs as SYSTEM and therefore allows no user
# SID at all). Test ONLY the write/delete/own-change bits, NOT the composite
# Modify/FullControl rights, whose bit patterns overlap ReadAndExecute/Synchronize (masking
# those would wrongly flag the read-only Program Files "Users: ReadAndExecute, Synchronize"
# ACE).
$script:PrivilegedSids = @(
    'S-1-5-18',                                                        # SYSTEM
    'S-1-5-32-544',                                                    # Administrators
    'S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464'   # TrustedInstaller
)
#
# The mask differs per object type, which matters: on a FILE, appending to it is enough to
# change what runs, so AppendData counts. On a DIRECTORY, AppendData/CreateDirectories only
# permits creating a NEW entry -- it cannot replace an existing child -- and every default
# Windows volume root grants exactly that to Users ("BUILTIN\Users:(CI)(AD)",
# "Authenticated Users:(AD)" on C:\), so folding it in would refuse every install on every
# machine. What matters on a container is CreateFiles/Delete/DeleteSubdirectoriesAndFiles/
# ChangePermissions/TakeOwnership.
$script:FileWriteMask = [int]([System.Security.AccessControl.FileSystemRights]'Write, Delete, DeleteSubdirectoriesAndFiles, ChangePermissions, TakeOwnership')
$script:DirWriteMask = [int]([System.Security.AccessControl.FileSystemRights]'CreateFiles, WriteExtendedAttributes, WriteAttributes, Delete, DeleteSubdirectoriesAndFiles, ChangePermissions, TakeOwnership')

function Resolve-AceSid($idr) {
    if ($null -eq $idr) { return $null }
    if ($idr -is [System.Security.Principal.SecurityIdentifier]) { return $idr.Value }
    try { return $idr.Translate([System.Security.Principal.SecurityIdentifier]).Value } catch { }
    # Get-Acl surfaces some identities as a bare string (notably $acl.Owner), which has no
    # Translate(). Resolve those through NTAccount -- otherwise EVERY path, including
    # admin-owned ones, reports an unresolvable owner and this refuses on every machine.
    try { return ([System.Security.Principal.NTAccount]([string]$idr)).Translate([System.Security.Principal.SecurityIdentifier]).Value } catch { return $null }
}

# The first non-privileged write vector on $p, or $null if safe. Checks three things, all
# ways to replace the launcher that a plain ACE scan misses: an empty Access collection (a
# NULL DACL grants everyone full control, so "no ACEs" must read as UNSAFE); the OWNER (who
# holds implicit WRITE_DAC and can rewrite the DACL at will); and Allow ACEs granting
# write/delete/ownership to a principal outside $allowedSids.
function Get-NonAdminWriteVector([string]$p, [string[]]$allowedSids) {
    try { $acl = Get-Acl -LiteralPath $p } catch { return "the ACL of '$p' could not be read" }

    $access = @($acl.Access)
    if ($access.Count -eq 0) { return "'$p' has a NULL/empty DACL (everyone has full control)" }

    # Ask for the owner AS A SID: $acl.Owner is a string, so translating it directly
    # is unreliable (see Resolve-AceSid).
    $ownerSid = $null
    try { $ownerSid = $acl.GetOwner([System.Security.Principal.SecurityIdentifier]).Value } catch { }
    if ($null -eq $ownerSid) { $ownerSid = Resolve-AceSid $acl.Owner }
    if ($null -eq $ownerSid) { return "'$p' has an unresolvable owner ($($acl.Owner))" }
    if ($allowedSids -notcontains $ownerSid) {
        return "'$p' is owned by $($acl.Owner), who can rewrite its DACL at will"
    }

    $mask = if (Test-Path -LiteralPath $p -PathType Container) { $script:DirWriteMask } else { $script:FileWriteMask }
    foreach ($ace in $access) {
        if ($ace.AccessControlType -ne [System.Security.AccessControl.AccessControlType]::Allow) { continue }
        # An inherit-only ACE grants nothing on THIS object; it is a template for children,
        # where it shows up as a real (inherited) ACE -- and this walk checks those children
        # in their own right. Not skipping it would flag every volume root, whose default
        # "Authenticated Users:(OI)(CI)(IO)(M)" ACE is exactly that.
        if (([int]$ace.PropagationFlags -band [int][System.Security.AccessControl.PropagationFlags]::InheritOnly) -ne 0) { continue }
        if (([int]$ace.FileSystemRights -band $mask) -eq 0) { continue }
        $sid = Resolve-AceSid $ace.IdentityReference
        if ($null -eq $sid) { return "'$p': $($ace.IdentityReference) (unresolvable SID) has '$($ace.FileSystemRights)'" }
        if ($allowedSids -notcontains $sid) { return "'$p': $($ace.IdentityReference) has '$($ace.FileSystemRights)'" }
    }
    return $null
}

# Walk the launcher AND every ancestor directory: write access to a CONTAINING directory is
# enough to delete and recreate the launcher whatever its own ACL says.
function Get-NonAdminWriteVectorForPath([string]$p, [string[]]$allowedSids) {
    $current = $p
    while ($current) {
        $vector = Get-NonAdminWriteVector $current $allowedSids
        if ($vector) { return $vector }
        $parent = Split-Path -Parent $current
        if (-not $parent -or $parent -eq $current) { break }
        $current = $parent
    }
    return $null
}

$taskSid = $null
try {
    $taskSid = (New-Object System.Security.Principal.NTAccount($TaskUser)).Translate([System.Security.Principal.SecurityIdentifier]).Value
} catch {
    $taskSid = $null   # unresolvable: allow only the privileged principals (stricter)
}
$allowedSids = $script:PrivilegedSids
if ($taskSid) { $allowedSids = $allowedSids + @($taskSid) }

# On a first install the launcher does not exist yet, so walk its directory instead (a
# missing file would otherwise read as "ACL could not be read").
$guardTarget = Join-Path $PSScriptRoot 'core-autostart.cmd'
if (-not (Test-Path -LiteralPath $guardTarget)) { $guardTarget = $PSScriptRoot }
$offender = Get-NonAdminWriteVectorForPath $guardTarget $allowedSids
if ($offender) {
    throw "Refusing to register a boot task that runs '$(Join-Path $PSScriptRoot 'core-autostart.cmd')' -- " +
          "$offender. Anyone who can replace that launcher gets code executed as '$TaskUser' at every boot, " +
          "with the wallet (and, if autounlock is enabled, the DPAPI store holding the wallet passphrase) at " +
          "its disposal. Install these scripts under an admin-only location (e.g. Program Files), with every " +
          "parent directory admin-owned, and re-run from there."
}

# Register-ScheduledTask -Password requires the plaintext. NOTE: PtrToStringBSTR
# materializes a managed System.String, whose contents live on the heap until GC
# and cannot be zeroed in PS 5.1 (Register-ScheduledTask has no SecureString form);
# we minimize the BSTR lifetime (ZeroFreeBSTR) but the String residue is
# unavoidable and equivalent to any -Password cmdlet.
$plainPw = $null
$bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($TaskPassword)
try {
    $plainPw = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr)

    $ddQuoted = '"' + $DataDir + '"'

    # Core-Start runs through a generated launcher that retries ONCE (after a 2-minute wait) if
    # the daemon exits non-zero -- covering the rare early-boot start failure seen on-device (the
    # task fires but the daemon exits before it can even log). A clean start blocks until shutdown
    # then exits 0 (no retry); the graceful GP-shutdown stop also exits 0. Each attempt's stderr is
    # captured to core-start.err.log so any recurrence is diagnosable. The launcher lives in the
    # install dir (not the datadir) so it is reachable at boot even before the user profile loads --
    # and the 2-minute wait lets a not-yet-ready datadir/profile settle before the retry. (Task
    # Scheduler's own restart-on-failure does NOT fire on a program's non-zero exit -- confirmed
    # on-device -- so the retry must live here.) If the launcher is ever deleted, re-run this
    # script to regenerate it. A crash or Core-Stop that lands inside the 2-minute wait is bounded
    # and harmless: a graceful stop exits 0 (no retry) and OS shutdown tears down the waiting cmd.exe.
    $launcher = Join-Path $PSScriptRoot 'core-autostart.cmd'
    # cmd reads a batch file in the OEM codepage and treats % as expansion even inside quotes, so
    # write the file as OEM (preserves a non-ASCII datadir under the user profile, which -Encoding
    # Ascii would replace with '?') and escape any literal % as %%.
    $coreB   = $CorePath -replace '%', '%%'
    $ddB     = $DataDir  -replace '%', '%%'
    # Capture the launcher's stderr OUTSIDE the data directory, in %TEMP%.
    #
    # It used to go to <datadir>\core-start.err.log, which forced the launcher to
    # 'md' the data directory first -- cmd evaluates a 2>> redirect before running
    # the command, so a missing directory meant the daemon never launched at all.
    # That pre-creation is now actively harmful: the daemon applies an owner-only
    # DACL to the data directory ONLY when it creates the directory itself
    # (util::CreateOwnerOnlyDirectory, create-only by design so a deliberately
    # widened datadir is never re-tightened). A launcher that made the directory
    # first left it with default inherited permissions and the daemon then --
    # correctly -- left it alone. %TEMP% always exists and is writable by the task's
    # user, so no pre-creation is needed and the daemon gets to create and protect
    # its own data directory.
    $errLogB = '%TEMP%\gridcoin-core-start.err.log'
    $daemonCmd = '"' + $coreB + '" -datadir="' + $ddB + '" -multiprocess 2>>"' + $errLogB + '"'
    Set-Content -LiteralPath $launcher -Encoding Oem -Value @(
        '@echo off',
        'rem Gridcoin Core-Start launcher (generated by Install-GridcoinCoreTask.ps1). Starts the',
        'rem multiprocess core; on a non-zero exit, waits 2 minutes and retries once. A clean run',
        'rem blocks until shutdown (exit 0 = no retry). Do not hand-edit -- re-run the installer.',
        $daemonCmd,
        'if %ERRORLEVEL% EQU 0 goto end',
        ('echo ==== attempt 1 failed exit %ERRORLEVEL% at %DATE% %TIME%, retrying in 2 min ==== >>"' + $errLogB + '"'),
        'ping -n 121 127.0.0.1 >nul',
        $daemonCmd,
        ':end'
    )

    $startAction = New-ScheduledTaskAction -Execute "$env:SystemRoot\System32\cmd.exe" -Argument ('/c "' + $launcher + '"')
    $stopAction  = New-ScheduledTaskAction -Execute $CorePath -Argument "-datadir=$ddQuoted stop"

    # Core-Start: long-running headless daemon, no execution-time limit. The start/crash
    # retry lives in the launcher above (1 retry, 2-min wait), NOT here: Task Scheduler's
    # restart-on-failure does not trigger on a program's non-zero exit (confirmed on-device).
    $startSettings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
        -StartWhenAvailable -ExecutionTimeLimit ([TimeSpan]::Zero) -MultipleInstances IgnoreNew

    # Core-Stop: a short-lived action; no restart (a failed stop must not loop).
    $stopSettings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
        -StartWhenAvailable -ExecutionTimeLimit (New-TimeSpan -Minutes 10) -MultipleInstances IgnoreNew

    $startTrigger = New-ScheduledTaskTrigger -AtStartup

    # Ensure the task folder exists first: Register-ScheduledTask -TaskPath does not
    # reliably create a missing folder on all Windows versions (best-effort -- if
    # this fails, Register may still create it or surface the clear error below).
    try {
        $svc = New-Object -ComObject 'Schedule.Service'
        $svc.Connect()
        try { $null = $svc.GetFolder("\$TaskFolder") }
        catch { $null = $svc.GetFolder('\').CreateFolder($TaskFolder) }
    } catch { }

    Register-ScheduledTask -TaskPath "\$TaskFolder\" -TaskName 'Core-Start' -Force `
        -Action $startAction -Trigger $startTrigger -Settings $startSettings `
        -User $TaskUser -Password $plainPw -RunLevel Limited `
        -Description 'Start the Gridcoin multiprocess core at boot (run-as the wallet user).' | Out-Null

    # Core-Stop: on-demand only, NO trigger. It carries no OS-shutdown (EventID-13)
    # trigger on purpose -- a scheduled task can't make Windows wait, so that path is
    # best-effort and gets cut short mid-flush. The reliable OS-shutdown flush is a Group
    # Policy Computer -> Shutdown script (Set-GridcoinShutdownFlush.ps1); shipping both
    # would double-stop and race. Core-Stop remains for manual stops and Update-GridcoinCore.
    Register-ScheduledTask -TaskPath "\$TaskFolder\" -TaskName 'Core-Stop' -Force `
        -Action $stopAction -Settings $stopSettings `
        -User $TaskUser -Password $plainPw -RunLevel Limited `
        -Description 'Graceful RPC stop of the Gridcoin core (on-demand; used by manual stops and Update-GridcoinCore). Start this task to stop the core; never hard-kill Core-Start. For a clean OS-shutdown flush use Set-GridcoinShutdownFlush.ps1.' | Out-Null
}
catch {
    throw "Failed to register the Gridcoin tasks: $($_.Exception.Message)`n" +
          "Registering a run-as-user task with a stored password usually needs an ELEVATED " +
          "PowerShell (Run as administrator), and '$TaskUser' must hold the 'Log on as a batch job' " +
          "right. Re-run elevated; see contrib/windows/README.md."
}
finally {
    $plainPw = $null
    [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr)
}

# Verify both tasks actually registered. Register-ScheduledTask can emit a NON-terminating
# "Access is denied" (0x80070005) -- a boot-triggered task needs an elevated shell, whereas
# the on-demand Core-Stop registers without it -- and continue, so confirm rather than trust
# that we reached here. Otherwise a half-registered install reports false success.
foreach ($name in 'Core-Start', 'Core-Stop') {
    if (-not (Get-ScheduledTask -TaskPath "\$TaskFolder\" -TaskName $name -ErrorAction SilentlyContinue)) {
        throw "Task \$TaskFolder\$name was not registered (likely 'Access is denied'). A boot-triggered " +
              "run-as task needs an ELEVATED PowerShell (Run as administrator) and the 'Log on as a batch " +
              "job' right for '$TaskUser'. Re-run elevated; see contrib/windows/README.md."
    }
}

# Inbound firewall rule for the daemon. The headless core never gets the interactive prompt
# the GUI does, and in multiprocess mode the core (not the GUI) does the P2P -- so without
# this rule Windows silently blocks inbound and the node is outbound-only. Program-based (like
# the GUI's own auto-created rule) so it is port-independent (mainnet/testnet). Non-fatal: the
# tasks are already registered, so a firewall failure warns rather than aborting.
$fwName = 'Gridcoin multiprocess core (gridcoinresearchd)'
if ($SkipFirewallRule) {
    Write-Host "Skipped the inbound firewall rule (-SkipFirewallRule). The core will be outbound-only until you add one for '$CorePath'." -ForegroundColor Yellow
} else {
    try {
        Get-NetFirewallRule -DisplayName $fwName -ErrorAction SilentlyContinue | Remove-NetFirewallRule -ErrorAction SilentlyContinue
        New-NetFirewallRule -DisplayName $fwName -Direction Inbound -Action Allow -Program $CorePath `
            -Protocol TCP -Profile $FirewallProfile -Enabled True `
            -Description 'Allow inbound P2P connections to the Gridcoin multiprocess core (gridcoinresearchd.exe). Created at task setup because the headless core gets no interactive firewall prompt.' | Out-Null
        Write-Host "Added inbound firewall rule '$fwName' for $CorePath (profiles: $($FirewallProfile -join ', '))." -ForegroundColor Green
    } catch {
        Write-Host ("WARNING: could not add the inbound firewall rule ($($_.Exception.Message)). The core will be " +
                    "outbound-only until you add an inbound rule for '$CorePath' (needs an elevated shell).") -ForegroundColor Yellow
    }
}

Write-Host "Registered \$TaskFolder\Core-Start (boot, via a retry launcher) and \$TaskFolder\Core-Stop (on-demand), run-as $TaskUser." -ForegroundColor Green
Write-Host ""
Write-Host "  Start the core now :  Start-ScheduledTask -TaskPath '\$TaskFolder\' -TaskName 'Core-Start'"
Write-Host "  Stop the core      :  Start-ScheduledTask -TaskPath '\$TaskFolder\' -TaskName 'Core-Stop'   (graceful; do NOT Stop-ScheduledTask Core-Start)"
Write-Host "  Attach the GUI     :  .\Start-GridcoinGui.ps1 -DataDir '$DataDir'"
Write-Host ""
Write-Host "  For a clean database flush on OS shutdown, also run (once):" -ForegroundColor Yellow
Write-Host "     .\Set-GridcoinShutdownFlush.ps1 -DataDir '$DataDir'   (configures a Group Policy shutdown script)" -ForegroundColor Yellow
