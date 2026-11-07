#Requires -Version 5.1
<#
.SYNOPSIS
    Install a Gridcoin release only after its Authenticode signature verifies.

.DESCRIPTION
    Runs elevated and verifies before it touches anything. In order:

      1. Refuse unless elevated.
      2. Refuse unless the pinned signer file (release-signer.txt), this script and the files it
         loads are writable by privileged SIDs only (SYSTEM, Administrators, TrustedInstaller).
      3. Copy the installer and the SHA256SUMS list into a new directory named .update-<guid>
         beside this script, with a protected Administrators+SYSTEM ACL, so nothing changes
         them between the checks and the run. The directory is always removed.
      4. Check the Authenticode signature of the staged installer: status Valid, a chain to
         Microsoft's identity-verification code-signing root, the pinned Artifact Signing
         identity marker, the pinned subject name, and a timestamp. With no signer pinned
         this fails closed.
      5. The installer's name must be listed in SHA256SUMS and its SHA-256 must match.
      6. Its file version must exceed the installed DisplayVersion, unless -AllowDowngrade.
      7. Only then run the staged installer with /S, wait, and require exit 0 and a new
         DisplayVersion. Then print the installer's UPGRADE-NOTICE.txt, when it wrote one.

    Nothing is stopped before step 7 passes; the installer stops and restarts the cores.

.PARAMETER Installer
    The gridcoin-<version>-win64-setup.exe to install.

.PARAMETER Sums
    The SHA256SUMS list from the release.

.PARAMETER SignerFile
    The pinned release signer. Defaults to release-signer.txt beside this script.

.PARAMETER AllowDowngrade
    Install a version that is not newer than the installed one.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Installer,
    [Parameter(Mandatory = $true)][string]$Sums,
    [switch]$AllowDowngrade,
    [string]$SignerFile
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Windows PowerShell 5.1 leaves $PSScriptRoot empty in a parameter default of a script with
# [CmdletBinding()], so the default is set here.
if (-not $SignerFile) { $SignerFile = Join-Path $PSScriptRoot 'release-signer.txt' }

. (Join-Path $PSScriptRoot 'GridcoinCommon.ps1')
. (Join-Path $PSScriptRoot 'Test-GridcoinRelease.ps1')

$uninstallKey = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\GridcoinResearch (64-bit)'

function ConvertTo-FourPartVersion {
    param([Parameter(Mandatory = $true)][version]$Value)
    return New-Object System.Version($Value.Major, $Value.Minor, [Math]::Max($Value.Build, 0), [Math]::Max($Value.Revision, 0))
}

# A refusal is a terminating error, so the finally block below always runs before the exit.
function Stop-GridcoinUpdate {
    param([Parameter(Mandatory = $true)][string]$Message)
    throw (New-Object System.InvalidOperationException("REFUSED: $Message"))
}

$stage = $null
$exitCode = 1
try {
    if (-not (Test-GridcoinElevated)) {
        Stop-GridcoinUpdate 'run this script from an elevated PowerShell (Run as administrator).'
    }

    $privileged = Get-GridcoinPrivilegedSids
    $guarded = @($SignerFile, $PSCommandPath, (Join-Path $PSScriptRoot 'GridcoinCommon.ps1'), (Join-Path $PSScriptRoot 'Test-GridcoinRelease.ps1'))
    foreach ($path in $guarded) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { Stop-GridcoinUpdate "'$path' does not exist." }
        $vector = Get-GridcoinWriteVector -Path $path -AllowedSids $privileged -ChainMasks
        if ($vector) { Stop-GridcoinUpdate "'$path' can be rewritten by an unprivileged account: $vector" }
    }
    foreach ($path in @($Installer, $Sums)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { Stop-GridcoinUpdate "'$path' does not exist." }
    }

    # Stage the inputs under this script's own directory, which only privileged accounts can
    # write (checked with the whole parent chain), in a directory that is created with its
    # protected Administrators+SYSTEM descriptor, so there is no moment when it is open.
    $vector = Get-GridcoinWriteVector -Path $PSScriptRoot -AllowedSids $privileged -ChainMasks
    if ($vector) { Stop-GridcoinUpdate "the script directory can be rewritten by an unprivileged account: $vector" }
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
    $stagePath = Join-Path $PSScriptRoot ('.update-' + [guid]::NewGuid().ToString('N'))
    $stage = $stagePath
    [void][System.IO.Directory]::CreateDirectory($stagePath, $acl)
    $stageVector = Get-GridcoinWriteVector -Path $stagePath -AllowedSids $privileged -ChainMasks
    if ($stageVector) { Stop-GridcoinUpdate "the staging directory is not protected: $stageVector" }

    $installerName = Split-Path -Leaf $Installer
    $stagedInstaller = Join-Path $stage $installerName
    $stagedSums = Join-Path $stage 'SHA256SUMS'
    $stagedSigner = Join-Path $stage 'release-signer.txt'
    Copy-Item -LiteralPath $Installer -Destination $stagedInstaller
    Copy-Item -LiteralPath $Sums -Destination $stagedSums
    Copy-Item -LiteralPath $SignerFile -Destination $stagedSigner

    try {
        Test-GridcoinReleaseSignature -Path $stagedInstaller -SignerFile $stagedSigner
    } catch {
        Stop-GridcoinUpdate "signature check failed: $($_.Exception.Message)"
    }
    Write-Host "$installerName carries a valid signature of the pinned release signer."

    try {
        $listed = Get-GridcoinReleaseHash -SumsPath $stagedSums -FileName $installerName
    } catch {
        Stop-GridcoinUpdate $_.Exception.Message
    }
    $actual = (Get-FileHash -LiteralPath $stagedInstaller -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -cne $listed) {
        Stop-GridcoinUpdate "the SHA-256 of $installerName is $actual, the checksum list says $listed."
    }
    Write-Host "$installerName matches the checksum list."

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
    $newVersion = ConvertTo-FourPartVersion $newVersion
    $installedVersion = ConvertTo-FourPartVersion $installedVersion
    if ($newVersion -le $installedVersion -and -not $AllowDowngrade) {
        Stop-GridcoinUpdate "the installer is $newVersion, not newer than the installed $installedVersion (-AllowDowngrade overrides)."
    }

    Write-Host "Installing $newVersion over $installedVersion ..."
    $installStarted = Get-Date
    $process = Start-Process -FilePath $stagedInstaller -ArgumentList '/S' -Wait -PassThru
    if ($process.ExitCode -ne 0) { Stop-GridcoinUpdate "the installer exited with code $($process.ExitCode)." }
    $afterText = (Get-ItemProperty -LiteralPath $uninstallKey).DisplayVersion
    $afterVersion = $null
    if (-not [version]::TryParse([string]$afterText, [ref]$afterVersion)) { Stop-GridcoinUpdate "DisplayVersion '$afterText' after the install cannot be read." }
    if ((ConvertTo-FourPartVersion $afterVersion) -ne $newVersion) {
        Stop-GridcoinUpdate "the installer exited 0 but DisplayVersion is '$afterText', not $newVersion."
    }
    Write-Host "Updated: DisplayVersion is now $afterText."

    # The silent install never shows the finish page, so the notes it would show are printed here:
    # the installer's UPGRADE-NOTICE.txt in this script's own folder, when this run wrote it.
    $noticePath = Join-Path $PSScriptRoot 'UPGRADE-NOTICE.txt'
    if ((Test-Path -LiteralPath $noticePath -PathType Leaf) -and
        (Get-Item -LiteralPath $noticePath).LastWriteTime -ge $installStarted) {
        try {
            Write-Host "Notes from the installer ($noticePath):"
            foreach ($noticeLine in @(Get-Content -LiteralPath $noticePath -Encoding UTF8)) { Write-Host $noticeLine }
        } catch {
            Write-Host "WARNING: the notes from the installer in $noticePath could not be printed: $($_.Exception.Message). The update itself succeeded."
        }
    }
    $exitCode = 0
} catch {
    $failure = $_.Exception.Message
    if ($failure.StartsWith('REFUSED: ')) {
        Write-Host "Update-GridcoinCore: $failure"
    } else {
        Write-Host "Update-GridcoinCore: FAILED: $failure"
    }
    $exitCode = 1
} finally {
    if ($stage -and (Test-Path -LiteralPath $stage)) {
        Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
    }
}
exit $exitCode
