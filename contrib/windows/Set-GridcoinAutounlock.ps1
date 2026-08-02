#Requires -Version 5.1
<#
.SYNOPSIS
    Enable (or remove) opt-in stake-only autounlock for the Windows multiprocess core.

.DESCRIPTION
    Prompts for the wallet passphrase (SecureString, entered twice), DPAPI-encrypts it
    (CurrentUser) to an owner-only blob, and registers the resident \Gridcoin\Autounlock
    scheduled task (run-as the wallet user) that runs Invoke-GridcoinAutounlock.ps1. The
    passphrase is NEVER on argv, in a log, or in the wallet. -Remove unregisters the task
    and deletes the blob.

    RUN THIS AS THE WALLET USER. DPAPI CurrentUser is keyed to the account that encrypts
    the blob, so it can only be decrypted by that same account -- which must therefore be
    the account the task runs as. This script refuses if -TaskUser resolves to a different
    account (an admin encrypting a blob a standard walletholder then cannot read). Elevation
    is fine: an elevated shell still runs as the same user SID.

    See contrib/windows/README.md and doc/multiprocess.md.

.NOTES
    Registering a run-as task with a stored login password typically needs an ELEVATED
    PowerShell and the "Log on as a batch job" right for the account.
#>
[CmdletBinding()]
param(
    [string]$DataDir = (Join-Path $env:APPDATA 'GridcoinResearch'),
    [string]$CredPath = (Join-Path $DataDir 'autounlock\passphrase.cred'),
    [string]$HelperPath = (Join-Path $PSScriptRoot 'Invoke-GridcoinAutounlock.ps1'),

    # The account the task runs as. Must be the current user (DPAPI CurrentUser constraint).
    [string]$TaskUser = "$env:USERDOMAIN\$env:USERNAME",

    # The TaskUser's LOGIN password (NOT the wallet passphrase). Prompted if omitted.
    [System.Security.SecureString]$TaskPassword,

    [int]$Interval = 20,
    [int]$Timeout = 99999999,
    [string]$TaskFolder = 'Gridcoin',
    [switch]$Remove
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# ---------------------------------------------------------------------------
# -Remove: unregister the task and delete the blob (idempotent).
# ---------------------------------------------------------------------------
if ($Remove) {
    # Resilient teardown: each step is independent so a failure in one still runs the rest.
    try {
        $existing = Get-ScheduledTask -TaskPath "\$TaskFolder\" -TaskName 'Autounlock' -ErrorAction SilentlyContinue
        if ($existing) {
            Unregister-ScheduledTask -TaskPath "\$TaskFolder\" -TaskName 'Autounlock' -Confirm:$false
            Write-Host "Removed \$TaskFolder\Autounlock"
        } else {
            Write-Host "\$TaskFolder\Autounlock not present (ok)"
        }
    } catch {
        Write-Host "Could not unregister \$TaskFolder\Autounlock ($($_.Exception.Message)); continuing." -ForegroundColor Yellow
    }
    if (Test-Path -LiteralPath $CredPath) {
        try { Remove-Item -LiteralPath $CredPath -Force; Write-Host "Deleted credential blob $CredPath" }
        catch { Write-Host "Could not delete $CredPath ($($_.Exception.Message))." -ForegroundColor Yellow }
    } else {
        Write-Host "credential blob $CredPath not present (ok)"
    }
    # Remove the autounlock dir's logs, then the dir itself if it is now empty.
    $credDir = Split-Path -Parent $CredPath
    foreach ($lg in @('autounlock.log', 'autounlock.log.1')) {
        $lp = Join-Path $credDir $lg
        if (Test-Path -LiteralPath $lp) { try { Remove-Item -LiteralPath $lp -Force } catch { } }
    }
    if ((Test-Path -LiteralPath $credDir) -and -not (Get-ChildItem -LiteralPath $credDir -Force)) {
        try { Remove-Item -LiteralPath $credDir -Force } catch { }
    }
    Write-Host "Autounlock removed. The core tasks (Core-Start / Core-Stop) are untouched."
    return
}

if (-not (Test-Path -LiteralPath $HelperPath)) {
    throw "Invoke-GridcoinAutounlock.ps1 not found at '$HelperPath'. Pass -HelperPath explicitly."
}
$HelperPath = (Resolve-Path -LiteralPath $HelperPath).Path

# DPAPI CurrentUser constraint: the blob must be encrypted by the account the task runs as.
# Refuse if -TaskUser resolves to a different SID than the current user.
$mySid = ([Security.Principal.WindowsIdentity]::GetCurrent()).User.Value
$taskSid = $null
try {
    $taskSid = (New-Object System.Security.Principal.NTAccount($TaskUser)).Translate([System.Security.Principal.SecurityIdentifier]).Value
} catch {
    $taskSid = $null
}
if ($taskSid -and ($taskSid -ne $mySid)) {
    throw ("-TaskUser '$TaskUser' is a different account than the one running this script. DPAPI " +
           "CurrentUser can only be decrypted by the account that encrypted it, so the autounlock task " +
           "must run as the SAME user that runs this setup. Re-run this AS '$TaskUser' (elevated if task " +
           "registration requires it).")
}
if (-not $taskSid) {
    Write-Host ("Note: could not resolve '$TaskUser' to a SID to confirm it matches the current user. The " +
                "DPAPI blob will only decrypt if the task runs as the same account that runs this setup.") -ForegroundColor Yellow
}

# ---------------------------------------------------------------------------
# Prompt the wallet passphrase (twice) and DPAPI-encrypt it.
# ---------------------------------------------------------------------------
$p1 = Read-Host -AsSecureString -Prompt 'Enter the WALLET passphrase to store for stake-only autounlock'
$p2 = Read-Host -AsSecureString -Prompt 'Re-enter the WALLET passphrase'

# Compare + encrypt via short-lived unmanaged buffers. The managed String materialised for
# UTF-8 encoding is unavoidable residue in PS 5.1 (equivalent to any -Password cmdlet); we
# minimise its lifetime and zero every buffer we control.
$b1 = [Runtime.InteropServices.Marshal]::SecureStringToGlobalAllocUnicode($p1)
$b2 = [Runtime.InteropServices.Marshal]::SecureStringToGlobalAllocUnicode($p2)
$bytes = $null
$blob = $null
try {
    $s1 = [Runtime.InteropServices.Marshal]::PtrToStringUni($b1)
    $s2 = [Runtime.InteropServices.Marshal]::PtrToStringUni($b2)
    if ($s1 -cne $s2) { throw 'the two passphrases do not match.' }
    if (-not $s1.Trim()) { throw 'the passphrase is empty.' }
    Add-Type -AssemblyName System.Security
    $bytes = [Text.Encoding]::UTF8.GetBytes($s1)          # UTF-8, no BOM (the helper tolerates a BOM anyway)
    $blob = [Security.Cryptography.ProtectedData]::Protect($bytes, $null, [Security.Cryptography.DataProtectionScope]::CurrentUser)
} finally {
    [Runtime.InteropServices.Marshal]::ZeroFreeGlobalAllocUnicode($b1)
    [Runtime.InteropServices.Marshal]::ZeroFreeGlobalAllocUnicode($b2)
    if ($bytes) { [Array]::Clear($bytes, 0, $bytes.Length) }
}

# ---------------------------------------------------------------------------
# Write the blob and lock it to the current user (defense in depth; DPAPI already
# user-binds it, but an owner-only ACL keeps other local users from even reading the blob).
# ---------------------------------------------------------------------------
$credDir = Split-Path -Parent $CredPath
if ($credDir -and -not (Test-Path -LiteralPath $credDir)) {
    New-Item -ItemType Directory -Path $credDir -Force | Out-Null
}
$meAccount = ([Security.Principal.WindowsIdentity]::GetCurrent()).User
# Lock the whole autounlock dir owner-only, with inheritance, so the (redacted) log the
# helper writes there is covered too -- not just the blob.
if ($credDir) {
    $dacl = New-Object System.Security.AccessControl.DirectorySecurity
    $dacl.SetAccessRuleProtection($true, $false)   # disable inheritance, drop inherited ACEs
    $inherit = [System.Security.AccessControl.InheritanceFlags]'ContainerInherit, ObjectInherit'
    $dacl.AddAccessRule((New-Object System.Security.AccessControl.FileSystemAccessRule(
        $meAccount, 'FullControl', $inherit, 'None', 'Allow')))
    Set-Acl -LiteralPath $credDir -AclObject $dacl
}
[IO.File]::WriteAllBytes($CredPath, $blob)
$acl = New-Object System.Security.AccessControl.FileSecurity
$acl.SetAccessRuleProtection($true, $false)   # disable inheritance, drop inherited ACEs
$acl.AddAccessRule((New-Object System.Security.AccessControl.FileSystemAccessRule($meAccount, 'FullControl', 'Allow')))
Set-Acl -LiteralPath $CredPath -AclObject $acl
Write-Host "Wrote DPAPI (CurrentUser) passphrase blob: $CredPath (owner-only)."

# ---------------------------------------------------------------------------
# Register the resident \Gridcoin\Autounlock task, run-as the wallet user.
# ---------------------------------------------------------------------------
if (-not $TaskPassword) {
    $cred = Get-Credential -UserName $TaskUser `
        -Message "Enter the Windows LOGIN password for '$TaskUser' (the account the autounlock task runs as). This is NOT the wallet passphrase."
    $TaskUser = $cred.UserName
    $TaskPassword = $cred.Password
}

# Re-validate AFTER the prompt: Get-Credential lets the operator edit the username, which
# would otherwise defeat the pre-prompt SID check and register the task under an account
# whose DPAPI store cannot decrypt the blob (autounlock would then fail on every start).
$finalSid = $null
try {
    $finalSid = (New-Object System.Security.Principal.NTAccount($TaskUser)).Translate([System.Security.Principal.SecurityIdentifier]).Value
} catch {
    $finalSid = $null
}
if ($finalSid -and ($finalSid -ne $mySid)) {
    throw ("The task run-as account '$TaskUser' is a different account than the one running this script. " +
           "DPAPI CurrentUser can only be decrypted by the account that encrypted the blob, so the task must " +
           "run as the SAME user that runs this setup. Re-run this AS '$TaskUser' (elevated if needed).")
}

$ddQuoted = '"' + $DataDir + '"'
$helperQuoted = '"' + $HelperPath + '"'
$credQuoted = '"' + $CredPath + '"'
$psArgs = "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File $helperQuoted -DataDir $ddQuoted -CredPath $credQuoted -Interval $Interval -Timeout $Timeout"

$action = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument $psArgs
$trigger = New-ScheduledTaskTrigger -AtStartup
# Resident: no execution-time limit; single instance. The modest crash-restart is a safety
# net only -- normal operation never exits (the loop logs transient/unrecoverable and keeps
# polling), so it will not hammer bad credentials.
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
    -StartWhenAvailable -ExecutionTimeLimit ([TimeSpan]::Zero) -MultipleInstances IgnoreNew `
    -RestartCount 3 -RestartInterval (New-TimeSpan -Minutes 1)

# Ensure \Gridcoin\ exists (Register -TaskPath does not reliably create it on all Windows versions).
try {
    $svc = New-Object -ComObject 'Schedule.Service'
    $svc.Connect()
    try { $null = $svc.GetFolder("\$TaskFolder") }
    catch { $null = $svc.GetFolder('\').CreateFolder($TaskFolder) }
} catch { }

# Register-ScheduledTask -Password needs the plaintext once. Minimise the BSTR lifetime; the
# managed String residue is unavoidable in PS 5.1 (equivalent to any -Password cmdlet).
$plainPw = $null
$bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($TaskPassword)
try {
    $plainPw = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr)
    Register-ScheduledTask -TaskPath "\$TaskFolder\" -TaskName 'Autounlock' -Force `
        -Action $action -Trigger $trigger -Settings $settings `
        -User $TaskUser -Password $plainPw -RunLevel Limited `
        -Description 'Stake-only wallet autounlock for the Gridcoin multiprocess core (DPAPI, run-as the wallet user).' | Out-Null
}
catch {
    throw "Failed to register \$TaskFolder\Autounlock: $($_.Exception.Message)`n" +
          "Registering a run-as task with a stored password usually needs an ELEVATED PowerShell " +
          "(Run as administrator) and the 'Log on as a batch job' right for '$TaskUser'. Re-run elevated; " +
          "see contrib/windows/README.md."
}
finally {
    $plainPw = $null
    [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr)
}

Write-Host "Registered \$TaskFolder\Autounlock (resident, run-as $TaskUser)." -ForegroundColor Green
Write-Host ""
Write-Host "  Start now :  Start-ScheduledTask -TaskPath '\$TaskFolder\' -TaskName 'Autounlock'"
Write-Host "  Log       :  $(Join-Path (Split-Path -Parent $CredPath) 'autounlock.log')"
Write-Host "  Remove    :  .\Set-GridcoinAutounlock.ps1 -Remove"
