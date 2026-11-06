#Requires -Version 5.1
<#
.SYNOPSIS
    Install a Gridcoin release only after its signed checksum list verifies.

.DESCRIPTION
    Runs elevated and verifies before it touches anything. In order:

      1. Refuse unless elevated.
      2. Refuse unless the pinned keys file, this script and the files it loads are writable by
         privileged SIDs only (SYSTEM, Administrators, TrustedInstaller).
      3. Copy the installer, the SHA256SUMS list and its .sig into a new directory under
         $env:TEMP with a protected Administrators+SYSTEM ACL, so nothing changes them between
         the checks and the run.
      4. Verify the detached ECDSA P-256 signature of SHA256SUMS against the pinned key
         (release-keys.txt). With no key pinned this fails closed.
      5. The installer's name must be listed and its SHA-256 must match.
      6. Its file version must exceed the installed DisplayVersion, unless -AllowDowngrade.
      7. Only then run the staged installer with /S, wait, and require exit 0 and a new
         DisplayVersion.

    Nothing is stopped before step 7 passes; the installer stops and restarts the cores.

.PARAMETER Installer
    The gridcoin-<version>-win64-setup.exe to install.

.PARAMETER Sums
    The SHA256SUMS list from the release.

.PARAMETER Signature
    The SHA256SUMS.sig file from the release.

.PARAMETER KeysFile
    The pinned release keys. Defaults to release-keys.txt beside this script.

.PARAMETER AllowDowngrade
    Install a version that is not newer than the installed one.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Installer,
    [Parameter(Mandatory = $true)][string]$Sums,
    [Parameter(Mandatory = $true)][string]$Signature,
    [string]$KeysFile = (Join-Path $PSScriptRoot 'release-keys.txt'),
    [switch]$AllowDowngrade
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'GridcoinCommon.ps1')
. (Join-Path $PSScriptRoot 'Test-GridcoinRelease.ps1')

$uninstallKey = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\GridcoinResearch (64-bit)'

function Stop-GridcoinUpdate {
    param([Parameter(Mandatory = $true)][string]$Message)
    Write-Host "Update-GridcoinCore: REFUSED: $Message"
    exit 1
}

try {
    if (-not (Test-GridcoinElevated)) {
        Stop-GridcoinUpdate 'run this script from an elevated PowerShell (Run as administrator).'
    }

    $privileged = Get-GridcoinPrivilegedSids
    $guarded = @($KeysFile, $PSCommandPath, (Join-Path $PSScriptRoot 'GridcoinCommon.ps1'), (Join-Path $PSScriptRoot 'Test-GridcoinRelease.ps1'))
    foreach ($path in $guarded) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { Stop-GridcoinUpdate "'$path' does not exist." }
        $vector = Get-GridcoinWriteVector -Path $path -AllowedSids $privileged -ChainMasks
        if ($vector) { Stop-GridcoinUpdate "'$path' can be rewritten by an unprivileged account: $vector" }
    }
    foreach ($path in @($Installer, $Sums, $Signature)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { Stop-GridcoinUpdate "'$path' does not exist." }
    }

    # Stage the inputs where only Administrators and SYSTEM can write.
    $stage = Join-Path $env:TEMP ('GridcoinUpdate-' + [guid]::NewGuid().ToString('N'))
    [void](New-Item -ItemType Directory -Path $stage)
    $inherit = [System.Security.AccessControl.InheritanceFlags]'ContainerInherit, ObjectInherit'
    $noProp = [System.Security.AccessControl.PropagationFlags]::None
    $allow = [System.Security.AccessControl.AccessControlType]::Allow
    $acl = New-Object System.Security.AccessControl.DirectorySecurity
    $acl.SetAccessRuleProtection($true, $false)
    $acl.SetOwner((New-Object System.Security.Principal.SecurityIdentifier('S-1-5-32-544')))
    foreach ($sid in @('S-1-5-18', 'S-1-5-32-544')) {
        $who = New-Object System.Security.Principal.SecurityIdentifier($sid)
        $acl.AddAccessRule((New-Object System.Security.AccessControl.FileSystemAccessRule($who, 'FullControl', $inherit, $noProp, $allow)))
    }
    Set-Acl -LiteralPath $stage -AclObject $acl
    $stageVector = Get-GridcoinWriteVector -Path $stage -AllowedSids $privileged
    if ($stageVector) { Stop-GridcoinUpdate "the staging directory is not protected: $stageVector" }

    $installerName = Split-Path -Leaf $Installer
    $stagedInstaller = Join-Path $stage $installerName
    $stagedSums = Join-Path $stage 'SHA256SUMS'
    $stagedSig = Join-Path $stage 'SHA256SUMS.sig'
    $stagedKeys = Join-Path $stage 'release-keys.txt'
    Copy-Item -LiteralPath $Installer -Destination $stagedInstaller
    Copy-Item -LiteralPath $Sums -Destination $stagedSums
    Copy-Item -LiteralPath $Signature -Destination $stagedSig
    Copy-Item -LiteralPath $KeysFile -Destination $stagedKeys

    try {
        Test-GridcoinReleaseSignature -SumsPath $stagedSums -SigPath $stagedSig -KeysPath $stagedKeys
    } catch {
        Stop-GridcoinUpdate "signature check failed: $($_.Exception.Message)"
    }
    Write-Host 'The signature of the checksum list verifies against the pinned key.'

    try {
        $listed = Get-GridcoinReleaseHash -SumsPath $stagedSums -FileName $installerName
    } catch {
        Stop-GridcoinUpdate $_.Exception.Message
    }
    $actual = (Get-FileHash -LiteralPath $stagedInstaller -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $listed) {
        Stop-GridcoinUpdate "the SHA-256 of $installerName is $actual, the signed list says $listed."
    }
    Write-Host "$installerName matches the signed list."

    $newText = (Get-Item -LiteralPath $stagedInstaller).VersionInfo.FileVersion
    $installedText = $null
    if (Test-Path -LiteralPath $uninstallKey) {
        $installedText = (Get-ItemProperty -LiteralPath $uninstallKey).DisplayVersion
    }
    if (-not $installedText) { Stop-GridcoinUpdate 'no installed Gridcoin was found; run the installer itself for a first install.' }
    $newVersion = $null
    $installedVersion = $null
    if (-not [version]::TryParse([string]$newText, [ref]$newVersion)) { Stop-GridcoinUpdate "the installer's file version '$newText' cannot be read." }
    if (-not [version]::TryParse([string]$installedText, [ref]$installedVersion)) { Stop-GridcoinUpdate "the installed version '$installedText' cannot be read." }
    if ($newVersion -le $installedVersion -and -not $AllowDowngrade) {
        Stop-GridcoinUpdate "the installer is $newVersion, not newer than the installed $installedVersion (-AllowDowngrade overrides)."
    }

    Write-Host "Installing $newVersion over $installedVersion ..."
    $process = Start-Process -FilePath $stagedInstaller -ArgumentList '/S' -Wait -PassThru
    if ($process.ExitCode -ne 0) { Stop-GridcoinUpdate "the installer exited with code $($process.ExitCode)." }
    $afterText = (Get-ItemProperty -LiteralPath $uninstallKey).DisplayVersion
    if ($afterText -eq $installedText) { Stop-GridcoinUpdate "the installer exited 0 but DisplayVersion is still $installedText." }
    Write-Host "Updated: DisplayVersion is now $afterText."
    exit 0
} catch {
    Write-Host "Update-GridcoinCore: FAILED: $($_.Exception.Message)"
    exit 1
}
