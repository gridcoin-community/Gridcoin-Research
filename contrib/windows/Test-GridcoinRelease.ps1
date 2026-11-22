#Requires -Version 5.1
<#
.SYNOPSIS
    Check the Authenticode signature of a Gridcoin release file against the pinned release signer.

.DESCRIPTION
    Dot-source this file for the two functions, or run it with -InstallerPath to check one file
    and print the result (exit 0 on success, 1 on failure).

        Test-GridcoinReleaseSignature -Path -SignerFile   (throws on any failure)
        Get-GridcoinReleaseHash -SumsPath -FileName       (the listed SHA-256, or throws)

    The signature check requires all of:
      - a signer pinned in the signer file (release-signer.txt): "identity-eku=<OID>", the
        identity marker of the Artifact Signing certificate profile, and "subject-cn=<name>";
        with none, it refuses;
      - an embedded signature that Get-AuthenticodeSignature reports as Valid;
      - the pinned identity marker in the signer certificate's extended key usage;
      - the pinned name as the common name of the signer certificate's subject;
      - a timestamp;
      - a chain, built with the machine policy and online revocation checking, to Microsoft's
        identity-verification code-signing root, matched by its subject name. When no revocation
        server can be reached the chain is built again without the revocation check, and the
        message of any later refusal says so.
    The certificates of the signing service are short-lived, so the time validity of the chain is
    taken from the timestamped signature that Get-AuthenticodeSignature reports as Valid, and the
    chain build ignores it.

    Needs Windows PowerShell 5.1.

.PARAMETER InstallerPath
    The file to check.

.PARAMETER SignerPath
    The pinned signer file. Defaults to release-signer.txt beside this script.

.PARAMETER SumsPath
    With -InstallerPath: also print the SHA-256 that this SHA256SUMS list gives for the file's name.

.EXAMPLE
    .\Test-GridcoinRelease.ps1 -InstallerPath .\gridcoin-5.5.2.1-win64-setup.exe
#>
[CmdletBinding()]
param(
    [string]$InstallerPath,
    [string]$SignerPath,
    [string]$SumsPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# An elevated run loads modules only from the system's own module directories.
$env:PSModulePath = "$PSHOME\Modules;$env:ProgramFiles\WindowsPowerShell\Modules"

# The subject of Microsoft's code-signing identity-verification root, matched by name.
$script:GridcoinMicrosoftRootSubject = '^CN=Microsoft Identity Verification Root Certificate Authority\b'

# {identity-eku, subject-cn} from a release-signer.txt; throws when nothing is pinned.
function Read-GridcoinReleaseSigner {
    param([Parameter(Mandatory = $true)][string]$SignerFile)
    if (-not (Test-Path -LiteralPath $SignerFile -PathType Leaf)) { throw "the signer file '$SignerFile' does not exist" }
    $found = @{}
    foreach ($rawLine in [System.IO.File]::ReadAllLines($SignerFile)) {
        $line = $rawLine.Trim()
        if ($line.Length -eq 0 -or $line.StartsWith('#')) { continue }
        if ($line -cnotmatch '^(identity-eku|subject-cn)=(.+)$') { throw "malformed line in $SignerFile" }
        $name = $Matches[1]
        $value = $Matches[2].Trim()
        if ($found.ContainsKey($name)) { throw "$name is pinned twice in $SignerFile" }
        $found[$name] = $value
    }
    if ($found.Count -eq 0) { throw "no release signer is pinned in $SignerFile" }
    if (-not $found.ContainsKey('identity-eku') -or -not $found.ContainsKey('subject-cn')) {
        throw "the release signer in $SignerFile needs both identity-eku and subject-cn"
    }
    if ($found['identity-eku'] -cnotmatch '^1\.3\.6\.1\.4\.1\.311\.97(\.[0-9]+)+$') {
        throw "identity-eku in $SignerFile is not an Artifact Signing identity OID (1.3.6.1.4.1.311.97.*)"
    }
    # Every Public Trust certificate carries this marker, so it identifies no one.
    if ($found['identity-eku'] -ceq '1.3.6.1.4.1.311.97.1.0') {
        throw "identity-eku in $SignerFile is the marker every Artifact Signing certificate carries, not a certificate profile's own"
    }
    return $found
}

function Test-GridcoinReleaseSignature {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$SignerFile
    )
    $pinned = Read-GridcoinReleaseSigner -SignerFile $SignerFile
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "the file '$Path' does not exist" }

    $sig = Get-AuthenticodeSignature -LiteralPath $Path
    if ($sig.Status -ne 'Valid') { throw "the Authenticode signature of $Path is $($sig.Status): $($sig.StatusMessage)" }
    if ($sig.SignatureType -ne 'Authenticode') { throw "$Path is not signed inside the file (signature type $($sig.SignatureType))" }
    $cert = $sig.SignerCertificate
    if ($null -eq $cert) { throw "$Path has no signer certificate" }

    $hasMarker = $false
    foreach ($ext in $cert.Extensions) {
        if ($ext.Oid.Value -ceq '2.5.29.37') {
            $eku = New-Object System.Security.Cryptography.X509Certificates.X509EnhancedKeyUsageExtension($ext, $false)
            foreach ($oid in $eku.EnhancedKeyUsages) {
                if ($oid.Value -ceq $pinned['identity-eku']) { $hasMarker = $true }
            }
        }
    }
    if (-not $hasMarker) { throw "the signer certificate of $Path does not carry the pinned identity marker $($pinned['identity-eku'])" }

    # The CN of the subject itself (GetNameInfo would fall back to other name parts).
    $commonName = ''
    if ($cert.Subject -cmatch '(?:^|, )CN=([^,]+)(?:,|$)') { $commonName = $Matches[1] }
    if ($commonName -cne $pinned['subject-cn']) { throw "the signer of $Path is '$commonName', not the pinned '$($pinned['subject-cn'])'" }

    if ($null -eq $sig.TimeStamperCertificate) { throw "the signature of $Path has no timestamp" }

    $chain = New-Object System.Security.Cryptography.X509Certificates.X509Chain($true)
    $chain.ChainPolicy.RevocationMode = [System.Security.Cryptography.X509Certificates.X509RevocationMode]::Online
    $chain.ChainPolicy.RevocationFlag = [System.Security.Cryptography.X509Certificates.X509RevocationFlag]::EntireChain
    $chain.ChainPolicy.VerificationFlags = [System.Security.Cryptography.X509Certificates.X509VerificationFlags]::IgnoreNotTimeValid
    $note = ''
    if (-not $chain.Build($cert)) {
        $revocationOnly = $true
        foreach ($status in $chain.ChainStatus) {
            if ($status.Status -ne 'RevocationStatusUnknown' -and $status.Status -ne 'OfflineRevocation') { $revocationOnly = $false }
        }
        if (-not $revocationOnly) { throw "the certificate chain of $Path is not valid: $($chain.ChainStatus[0].StatusInformation.Trim()) (an offline machine may lack the intermediate certificate)" }
        $chain.Reset()
        $chain.ChainPolicy.RevocationMode = [System.Security.Cryptography.X509Certificates.X509RevocationMode]::NoCheck
        $chain.ChainPolicy.VerificationFlags = [System.Security.Cryptography.X509Certificates.X509VerificationFlags]::IgnoreNotTimeValid
        if (-not $chain.Build($cert)) { throw "the certificate chain of $Path is not valid: $($chain.ChainStatus[0].StatusInformation.Trim())" }
        $note = ' (revocation could not be checked: no revocation server was reachable)'
    }
    $root = $chain.ChainElements[$chain.ChainElements.Count - 1].Certificate
    if ($root.Subject -cnotmatch $script:GridcoinMicrosoftRootSubject) {
        throw "the certificate chain of $Path ends at '$($root.Subject)', not at Microsoft's identity-verification root$note"
    }
    if ($note) { Write-Host "Test-GridcoinRelease: note$note." }
}

# The SHA-256 (lowercase hex) the list gives for FileName. Call only after the signature verified.
function Get-GridcoinReleaseHash {
    param(
        [Parameter(Mandatory = $true)][string]$SumsPath,
        [Parameter(Mandatory = $true)][string]$FileName
    )
    $text = [System.Text.Encoding]::UTF8.GetString([System.IO.File]::ReadAllBytes($SumsPath))
    $hit = $null
    foreach ($line in $text.Split("`n")) {
        if ($line.Length -eq 0) { continue }
        if ($line -cnotmatch '^([0-9a-f]{64})  ([^/\\\s][^/\\]*)$') { throw 'malformed line in the checksum list' }
        if ($Matches[2] -ceq $FileName) {
            if ($null -ne $hit) { throw "$FileName is listed twice" }
            $hit = $Matches[1]
        }
    }
    if ($null -eq $hit) { throw "$FileName is not listed in the checksum list" }
    return $hit
}

if ($InstallerPath -or $SignerPath -or $SumsPath) {
    if (-not $InstallerPath) {
        Write-Host 'Test-GridcoinRelease: -InstallerPath is needed.'
        exit 1
    }
    if (-not $SignerPath) { $SignerPath = Join-Path $PSScriptRoot 'release-signer.txt' }
    try {
        Test-GridcoinReleaseSignature -Path $InstallerPath -SignerFile $SignerPath
        Write-Host 'signature ok'
        if ($SumsPath) {
            Write-Host (Get-GridcoinReleaseHash -SumsPath $SumsPath -FileName (Split-Path -Leaf $InstallerPath))
        }
        exit 0
    } catch {
        Write-Host "Test-GridcoinRelease: REFUSED: $($_.Exception.Message)"
        exit 1
    }
}
