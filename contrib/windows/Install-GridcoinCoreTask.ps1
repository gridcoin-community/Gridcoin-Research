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

if (-not $TaskPassword) {
    $cred = Get-Credential -UserName $TaskUser `
        -Message "Enter the Windows password for '$TaskUser' (the account the Gridcoin core will run as). This is NOT the wallet passphrase."
    $TaskUser = $cred.UserName
    $TaskPassword = $cred.Password
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
    $errLogB = (Join-Path $DataDir 'core-start.err.log') -replace '%', '%%'
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
