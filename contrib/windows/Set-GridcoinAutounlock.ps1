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
    Run this at an ELEVATED console (Run as administrator). Two reasons: registering the
    resident run-as task with a stored login password needs elevation and the "Log on as a
    batch job" right for the account; and DPAPI CurrentUser encryption requires an
    interactive or batch logon -- it FAILS over a network logon (a key-based SSH / PSRemoting
    session), where the user's master key is not available. So enable autounlock from the
    machine's own console, not remotely.
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
# Argument quoting for the registered task action.
# ---------------------------------------------------------------------------
# CommandLineToArgvW -- which powershell.exe uses to split the command line the task
# hands it -- halves a run of backslashes that immediately precedes a closing quote and
# treats the quote as escaped. So a -DataDir ending in '\' (including a bare drive root,
# "D:\") swallows its closing quote and corrupts every argument after it: -CredPath,
# -Interval and -Timeout all shift. Double the trailing run so the quote still closes.
# Embedded double quotes are rejected outright instead (they have no legitimate use in
# these paths), matching the sibling scripts.
function Get-QuotedArg([string]$value) {
    return '"' + ($value -replace '(\\+)$', '$1$1') + '"'
}

# Absolute, trailing-separator-free, case-folded form for comparing two paths.
function Get-NormalizedPath([string]$p) {
    return ([IO.Path]::GetFullPath($p)).TrimEnd('\').ToLowerInvariant()
}

# ---------------------------------------------------------------------------
# SECURITY: writability of what the task will execute.
# ---------------------------------------------------------------------------
# The Autounlock task runs $HelperPath (via powershell.exe -File) AS THE WALLET USER, and
# that user's DPAPI store is exactly what decrypts the passphrase blob. So anyone who can
# replace the helper -- or any directory on the path to it -- has the passphrase handed to
# them by our own task. Fail CLOSED unless every component is writable only by privileged
# principals (SYSTEM, Administrators, TrustedInstaller) or by the wallet user itself, who
# already holds the passphrase and is therefore not an escalation.
#
# This mirrors Get-NonAdminWriteVector / Get-NonAdminWriteVectorForPath in
# Set-GridcoinShutdownFlush.ps1 (that script's script runs as SYSTEM, so it allows no user
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

# Returns a description of the first non-privileged write vector found on $p, or $null if
# it is safe. Checks THREE things, all of them ways to replace the script that a plain ACE
# scan misses:
#   1. an empty Access collection -- a NULL DACL grants everyone full control, so "no ACEs"
#      must read as UNSAFE, not as "no offender";
#   2. the OWNER -- an owner always holds implicit WRITE_DAC and can simply rewrite the
#      DACL, so a non-privileged owner defeats any ACE check;
#   3. Allow ACEs granting write/delete/ownership to a non-allowed principal.
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

# Walk the script AND every ancestor directory: write access to a CONTAINING directory is
# enough to delete and recreate the script, regardless of the script's own ACL, so checking
# the file alone is not sufficient.
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
    Write-Host ""
    # Two things -Remove does NOT do, both of which operators reasonably assume it does:
    Write-Host "NOTE: a RUNNING wallet stays UNLOCKED -- removing the task and the blob does not relock it." -ForegroundColor Yellow
    Write-Host "      Relock now with the core's own RPC, or stop the core:"
    Write-Host "          Start-ScheduledTask -TaskPath '\$TaskFolder\' -TaskName 'Core-Stop'"
    Write-Host "      (or, with the core running: & '<install>\daemon\gridcoinresearchd.exe' -datadir=`"$DataDir`" walletlock)"
    Write-Host "NOTE: deleting the blob is an ordinary file delete, NOT a secure erase. The DPAPI-encrypted" -ForegroundColor Yellow
    Write-Host "      bytes may survive in free space, backups, or Volume Shadow Copies. If the passphrase"
    Write-Host "      itself may be compromised, change the wallet passphrase -- do not rely on this delete."
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
# Validate the paths that get baked into the task's command line (see Get-QuotedArg).
# ---------------------------------------------------------------------------
if (-not $DataDir)          { throw "-DataDir must not be empty." }
if ($DataDir -match '"')    { throw "-DataDir must not contain a double-quote character (it is baked into the task's command line)." }
if ($CredPath -match '"')   { throw "-CredPath must not contain a double-quote character (it is baked into the task's command line)." }
if ($HelperPath -match '"') { throw "-HelperPath must not contain a double-quote character (it is baked into the task's command line)." }

# ---------------------------------------------------------------------------
# The blob's PARENT directory gets a protected, owner-only DACL below (so the helper's
# redacted log next to the blob is covered too, not just the blob). That is only safe for a
# directory dedicated to autounlock: with -CredPath "$DataDir\passphrase.cred" the parent IS
# the datadir, and we would strip inheritance from the whole wallet directory and lock every
# other principal (a backup agent, a second admin) out of wallet.dat. Refuse that.
# ---------------------------------------------------------------------------
$credDir = Split-Path -Parent $CredPath
if (-not $credDir) {
    throw "-CredPath must include a directory, e.g. '...\GridcoinResearch\autounlock\passphrase.cred'."
}
if ((Get-NormalizedPath $credDir) -eq (Get-NormalizedPath $DataDir)) {
    throw ("-CredPath '$CredPath' puts the blob directly in the datadir. This script applies a protected, " +
           "owner-only ACL to the blob's PARENT directory, so that would re-ACL the entire datadir (wallet.dat " +
           "included) and break inheritance on it. Use a dedicated subdirectory, e.g. " +
           "'$(Join-Path $DataDir 'autounlock\passphrase.cred')'.")
}
if ((Test-Path -LiteralPath $credDir) -and
    @(Get-ChildItem -LiteralPath $credDir -Force | Where-Object {
        $_.Name -notin @('passphrase.cred', 'autounlock.log', 'autounlock.log.1') -and $_.FullName -ne $CredPath
    }).Count -gt 0) {
    Write-Host ("WARNING: '$credDir' holds files other than the autounlock blob/log. It is about to be locked " +
                "owner-only with inheritance disabled, which will change access to everything in it. Use a " +
                "dedicated directory for -CredPath if that is not what you want.") -ForegroundColor Yellow
}

# ---------------------------------------------------------------------------
# SECURITY GATE: refuse to register a task that executes a replaceable script.
# ---------------------------------------------------------------------------
# Privileged principals plus the wallet user itself (the account this runs as, and the
# account the task runs as -- already verified to be the same SID above). Anyone else with
# write/delete/ownership anywhere on the path to the helper can swap it for their own script
# and have OUR task run it as the wallet user, whose DPAPI store then hands over the wallet
# passphrase. Fail closed before the operator is even asked for it.
$allowedSids = $script:PrivilegedSids + @($mySid)
$offender = Get-NonAdminWriteVectorForPath $HelperPath $allowedSids
if ($offender) {
    throw ("Refusing to register an autounlock task that runs '$HelperPath' -- $offender. Anyone who can " +
           "replace that script (or anything on the path to it) gets it executed AS the wallet user, whose " +
           "DPAPI store decrypts the stored wallet passphrase. Install these scripts under an admin-only " +
           "location (e.g. Program Files), with every parent directory admin-owned, and re-run from there " +
           "or pass -HelperPath pointing at such a copy.")
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
# $credDir was resolved and validated above (it must not be the datadir itself).
# ---------------------------------------------------------------------------
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

$ddQuoted = Get-QuotedArg $DataDir
$helperQuoted = Get-QuotedArg $HelperPath
$credQuoted = Get-QuotedArg $CredPath
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

# Verify the task actually registered. Register-ScheduledTask can emit a NON-terminating
# "Access is denied" and continue -- a boot-triggered run-as task needs an elevated shell --
# so confirm rather than report false success (the DPAPI blob was still written above).
if (-not (Get-ScheduledTask -TaskPath "\$TaskFolder\" -TaskName 'Autounlock' -ErrorAction SilentlyContinue)) {
    throw "Task \$TaskFolder\Autounlock was not registered (likely 'Access is denied'). This needs an " +
          "ELEVATED PowerShell (Run as administrator) and the 'Log on as a batch job' right for '$TaskUser'. " +
          "The DPAPI blob at $CredPath was written; re-run elevated to register the task, or -Remove to undo."
}

Write-Host "Registered \$TaskFolder\Autounlock (resident, run-as $TaskUser)." -ForegroundColor Green
Write-Host ""
Write-Host "  Start now :  Start-ScheduledTask -TaskPath '\$TaskFolder\' -TaskName 'Autounlock'"
Write-Host "  Log       :  $(Join-Path (Split-Path -Parent $CredPath) 'autounlock.log')"
Write-Host "  Remove    :  .\Set-GridcoinAutounlock.ps1 -Remove"
