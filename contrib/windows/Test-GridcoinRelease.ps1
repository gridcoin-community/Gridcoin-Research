#Requires -Version 5.1
<#
.SYNOPSIS
    Verify a signed Gridcoin SHA256SUMS release list (detached ECDSA P-256, pinned key).

.DESCRIPTION
    Dot-source this file for the two functions, or run it with -SumsPath, -SigPath and
    -KeysPath to verify a list and print the result (exit 0 on success, 1 on failure).

        Test-GridcoinReleaseSignature -SumsPath -SigPath -KeysPath   (throws on any failure)
        Get-GridcoinReleaseHash -SumsPath -FileName                  (the listed SHA-256, or throws)

    Formats are fixed by contrib/release/sign-sha256sums.py. SHA256SUMS.sig is one line,
    "gridcoin-sha256sums-sig-v1 <keyid> <base64 of r||s, 64 bytes>", an ECDSA P-256 signature over
    the SHA-256 of the exact bytes of SHA256SUMS. The keyid is the lowercase hex of the first
    8 bytes of SHA-256(X||Y). release-keys.txt has "#" comments and "<keyid> <base64 of X||Y> <label>"
    lines. With no key pinned the check fails closed.

    Needs Windows PowerShell 5.1 with .NET Framework 4.7.2 or later.

.EXAMPLE
    .\Test-GridcoinRelease.ps1 -SumsPath SHA256SUMS -SigPath SHA256SUMS.sig -KeysPath release-keys.txt -FileName gridcoin-5.5.2.1-win64-setup.exe
#>
[CmdletBinding()]
param(
    [string]$SumsPath,
    [string]$SigPath,
    [string]$KeysPath,
    [string]$FileName
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-GridcoinReleaseKeyId {
    param([Parameter(Mandatory = $true)][byte[]]$PublicXY)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $digest = $sha.ComputeHash($PublicXY)
    } finally {
        $sha.Dispose()
    }
    $text = New-Object System.Text.StringBuilder
    for ($i = 0; $i -lt 8; $i++) { [void]$text.Append($digest[$i].ToString('x2')) }
    return $text.ToString()
}

# {keyid = X||Y bytes} from a release-keys.txt; throws when none is pinned.
function Read-GridcoinReleaseKeys {
    param([Parameter(Mandatory = $true)][string]$KeysPath)
    if (-not (Test-Path -LiteralPath $KeysPath -PathType Leaf)) { throw "the release keys file '$KeysPath' does not exist" }
    $found = @{}
    foreach ($rawLine in [System.IO.File]::ReadAllLines($KeysPath)) {
        $line = $rawLine.Trim()
        if ($line.Length -eq 0 -or $line.StartsWith('#')) { continue }
        $fields = $line -split '\s+', 3
        if ($fields.Count -lt 2 -or $fields[0] -notmatch '^[0-9a-f]{16}$' -or $fields[1] -notmatch '^[A-Za-z0-9+/]{86}==$') {
            throw "malformed key line in $KeysPath"
        }
        $xy = [Convert]::FromBase64String($fields[1])
        if ($xy.Length -ne 64) { throw "malformed key line in $KeysPath" }
        if ((Get-GridcoinReleaseKeyId -PublicXY $xy) -ne $fields[0]) { throw "key line $($fields[0]) in $KeysPath does not match its key id" }
        $found[$fields[0]] = $xy
    }
    if ($found.Count -eq 0) { throw "no release key is pinned in $KeysPath" }
    return $found
}

function Test-GridcoinReleaseSignature {
    param(
        [Parameter(Mandatory = $true)][string]$SumsPath,
        [Parameter(Mandatory = $true)][string]$SigPath,
        [Parameter(Mandatory = $true)][string]$KeysPath
    )
    $pinned = Read-GridcoinReleaseKeys -KeysPath $KeysPath

    $sigBytes = [System.IO.File]::ReadAllBytes($SigPath)
    foreach ($b in $sigBytes) { if ($b -gt 127) { throw 'the signature file is not ASCII' } }
    $sigText = [System.Text.Encoding]::ASCII.GetString($sigBytes)
    if (-not $sigText.EndsWith("`n") -or $sigText.IndexOf("`n") -ne ($sigText.Length - 1)) {
        throw 'the signature file must be exactly one LF-terminated line'
    }
    $parts = $sigText.Substring(0, $sigText.Length - 1).Split(' ')
    if ($parts.Count -ne 3 -or $parts[0] -ne 'gridcoin-sha256sums-sig-v1') { throw 'the signature line is malformed' }
    if ($parts[1] -notmatch '^[0-9a-f]{16}$') { throw 'the signature key id is malformed' }
    if ($parts[2] -notmatch '^[A-Za-z0-9+/]{86}==$') { throw 'the signature is not 64 bytes of base64' }
    $signature = [Convert]::FromBase64String($parts[2])
    if ($signature.Length -ne 64) { throw 'the signature is not 64 bytes' }
    $keyId = $parts[1]
    if (-not $pinned.ContainsKey($keyId)) { throw "the signature key $keyId is not pinned in $KeysPath" }
    $pubXY = [byte[]]$pinned[$keyId]

    # ECCPUBLICBLOB: magic 0x31534345 (ECS1, P-256 public), the coordinate length, X, Y
    $blob = New-Object System.Collections.Generic.List[byte]
    $blob.AddRange([BitConverter]::GetBytes([uint32]0x31534345))
    $blob.AddRange([BitConverter]::GetBytes([uint32]32))
    $blob.AddRange($pubXY)

    $sumsBytes = [System.IO.File]::ReadAllBytes($SumsPath)
    $cngKey = $null
    $verifier = $null
    try {
        try {
            $cngKey = [System.Security.Cryptography.CngKey]::Import($blob.ToArray(), [System.Security.Cryptography.CngKeyBlobFormat]::EccPublicBlob)
        } catch {
            throw "the pinned key $keyId is not a valid P-256 public key"
        }
        $verifier = New-Object System.Security.Cryptography.ECDsaCng($cngKey)
        $good = $verifier.VerifyData($sumsBytes, $signature, [System.Security.Cryptography.HashAlgorithmName]::SHA256)
    } finally {
        if ($null -ne $verifier) { $verifier.Dispose() }
        if ($null -ne $cngKey) { $cngKey.Dispose() }
    }
    if (-not $good) { throw "the signature does not match $SumsPath" }
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
        if ($line -notmatch '^([0-9a-f]{64})  ([^/\\\s][^/\\]*)$') { throw 'malformed line in the checksum list' }
        if ($Matches[2] -ceq $FileName) {
            if ($null -ne $hit) { throw "$FileName is listed twice" }
            $hit = $Matches[1]
        }
    }
    if ($null -eq $hit) { throw "$FileName is not listed in the checksum list" }
    return $hit
}

if ($SumsPath -or $SigPath -or $KeysPath) {
    if (-not ($SumsPath -and $SigPath -and $KeysPath)) {
        Write-Host 'Test-GridcoinRelease: -SumsPath, -SigPath and -KeysPath are all needed.'
        exit 1
    }
    try {
        Test-GridcoinReleaseSignature -SumsPath $SumsPath -SigPath $SigPath -KeysPath $KeysPath
        if ($FileName) {
            Write-Host (Get-GridcoinReleaseHash -SumsPath $SumsPath -FileName $FileName)
        } else {
            Write-Host 'signature ok'
        }
        exit 0
    } catch {
        Write-Host "Test-GridcoinRelease: REFUSED: $($_.Exception.Message)"
        exit 1
    }
}
