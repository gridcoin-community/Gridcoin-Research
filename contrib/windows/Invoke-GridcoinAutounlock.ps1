#Requires -Version 5.1
<#
.SYNOPSIS
    Stake-only Gridcoin wallet autounlock helper (Windows-native).

.DESCRIPTION
    Native PowerShell port of contrib/wallettools/gridcoin_autounlock.py, which is the
    canonical reference for the security-critical behaviour. Reads RPC connection details
    from gridcoinresearch.conf (or gridcoin.conf), DPAPI-decrypts the wallet passphrase
    (CurrentUser), verifies that our own SID owns every LISTEN socket on the RPC port
    (fail closed), and then sends `walletpassphrase <pass> <timeout> $true` (stake-only)
    whenever it sees a fresh core instance. Python-3-free; runs on stock Windows.

    Security-critical behaviour (mirrors gridcoin_autounlock.py):
      * Listener-ownership gate. The RPC port is UNBOUND for minutes during core startup
        (LoadBlockIndex runs long before the RPC threads) and an unprivileged loopback
        port can be bound first by any local account. HTTP Basic authenticates the client
        to the server, not the reverse, so a naive readiness poll would hand
        rpcuser:rpcpassword (then the passphrase) to whatever answers. Before anything
        goes on the wire we require that every LISTEN socket on the port is owned by our
        own SID. A foreign owner, or a listener we cannot attribute, fails closed. On
        Windows the owner is resolved natively (Get-NetTCPConnection -> Win32_Process
        GetOwnerSid); this closes the same startup race the Python helper can only refuse
        on this platform. "Loopback" is decided by parsing the host as an IP address, never
        by string prefix (127.0.0.1.attacker.example is a DNS name that resolves off-box),
        and the gate is re-run immediately before the walletpassphrase call so no round trip
        separates the ownership check from the request that carries the secret.
      * Proxy-free request. $req.Proxy = $null so an environment/WinHTTP proxy can never
        divert the passphrase POST off-box in cleartext.
      * Redaction. The passphrase, the rpcpassword, and the Basic token are scrubbed from
        every log line, and a top-level catch keeps the resident loop alive without ever
        printing an unredacted error.

    Exit status (meaningful in -Once; the resident loop logs and keeps polling instead of
    exiting on these):
      0  unlocked, or nothing to do (already unlocked / not encrypted)
      1  transient; retrying may help (core not up, RPC not bound yet, transport)
      2  unrecoverable; retrying cannot help (wrong passphrase, HTTP 401, a foreign or
         unverifiable process holding the RPC port, a bad DPAPI blob)

    See doc/multiprocess.md and contrib/windows/README.md.
#>
[CmdletBinding()]
param(
    # Data directory holding gridcoinresearch.conf (the wallet/config live here).
    [string]$DataDir = (Join-Path $env:APPDATA 'GridcoinResearch'),

    # Explicit config path (overrides the -DataDir lookup of gridcoinresearch.conf / gridcoin.conf).
    [string]$Conf,

    # DPAPI (CurrentUser) passphrase blob written by Set-GridcoinAutounlock.ps1.
    [string]$CredPath = (Join-Path $DataDir 'autounlock\passphrase.cred'),

    # Redacted log sink (a scheduled task's stdout is not captured anywhere). Defaults
    # alongside the blob; kept in the owner-only autounlock dir.
    [string]$LogPath = (Join-Path $DataDir 'autounlock\autounlock.log'),

    # Connection overrides (otherwise taken from the config; see resolve below).
    [string]$RpcConnect,
    [int]$RpcPort = 0,           # 0 => use conf rpcport, else DEFAULT_RPC_PORT
    [string]$RpcUser,

    [int]$Timeout = 99999999,    # walletpassphrase timeout (the node clamps > 100000000)
    [int]$Interval = 20,         # poll interval seconds
    [switch]$Once,               # poll once and exit (testing / one-shot)

    # Proceed even when the listener-ownership gate mechanism is unavailable on this host
    # (e.g. Get-NetTCPConnection missing). INSECURE unless the host is single-user and
    # trusted: without owner verification the passphrase could go to a process that
    # squatted the RPC port during core startup.
    [switch]$AllowUnverifiedListener
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$EXIT_OK = 0
$EXIT_TRANSIENT = 1
$EXIT_UNRECOVERABLE = 2

# Per-chain default RPC ports (CreateBaseChainParams, src/chainparamsbase.cpp). The daemon
# defaults to the port of the chain it was started on when rpcport is unset and a packaged
# gridcoinresearch.conf commonly omits rpcport -- so derive the default from the chain the
# config selects instead of assuming mainnet. Assuming mainnet is not just a connectivity
# bug: on a testnet node with no rpcport we would gate and POST to 15715, and if the same
# user also runs a mainnet node here the gate answers "ours" and the TESTNET passphrase is
# handed to the MAINNET wallet. An ambiguous chain is refused rather than guessed.
$DEFAULT_RPC_PORT = 15715   # mainnet (CBaseChainParams::MAIN)
$DEFAULT_RPC_PORTS = @{ 'main' = $DEFAULT_RPC_PORT; 'test' = 25715; 'regtest' = 35715 }

# Secrets shorter than this are NOT registered for redaction: Format-Redacted is a plain
# substring replace, so a short secret shreds unrelated text -- with rpcpassword=ass the
# security-critical "REFUSING TO SEND CREDENTIALS" warning came out as mangled nonsense.
# A secret that short cannot be meaningfully hidden in prose anyway.
$MIN_REDACTABLE_SECRET_LEN = 8

# RPC error codes we react to (src/rpc/protocol.h).
$RPC_INVALID_PARAMETER = -8
$RPC_WALLET_PASSPHRASE_INCORRECT = -14
$RPC_WALLET_WRONG_ENC_STATE = -15
$RPC_WALLET_ALREADY_UNLOCKED = -17

# ---------------------------------------------------------------------------
# Redaction (mirror register_secret / redact): scrub known secrets from any log line.
# ---------------------------------------------------------------------------
$script:Secrets = New-Object System.Collections.Generic.List[string]

function Register-Secret([string]$value) {
    if (-not $value) { return }
    if ($value.Length -lt $MIN_REDACTABLE_SECRET_LEN) {
        Write-Warn ("a secret shorter than {0} characters was NOT registered for log redaction: " +
            "redacting so short a string would shred unrelated text in every log line (the warnings " +
            "included). Use a longer rpcpassword/passphrase." -f $MIN_REDACTABLE_SECRET_LEN)
        return
    }
    [void]$script:Secrets.Add($value)
}

function Format-Redacted([string]$text) {
    $out = [string]$text
    foreach ($s in $script:Secrets) { if ($s) { $out = $out.Replace($s, '<REDACTED>') } }
    return $out
}

# Console (for -Once / interactive) AND an owner-only, size-capped log file (for the
# resident task, whose stdout goes nowhere). Both sinks receive redacted text only.
function Write-Line([string]$level, [string]$message) {
    $red = Format-Redacted $message
    $line = ('gridcoin_autounlock: ' + $(if ($level -eq 'WARNING') { 'WARNING: ' } else { '' }) + $red)
    if ($level -eq 'WARNING') { [Console]::Error.WriteLine($line) } else { [Console]::Out.WriteLine($line) }
    if ($LogPath) {
        try {
            $dir = Split-Path -Parent $LogPath
            if ($dir -and -not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
            # Single-rotation cap so a persistent refusal (logged every poll) cannot grow the file unbounded.
            if ((Test-Path -LiteralPath $LogPath) -and ((Get-Item -LiteralPath $LogPath).Length -gt 1MB)) {
                Move-Item -LiteralPath $LogPath -Destination ($LogPath + '.1') -Force
            }
            $stamp = (Get-Date).ToString('yyyy-MM-dd HH:mm:ss')
            Add-Content -LiteralPath $LogPath -Value ("$stamp $line") -Encoding UTF8
        } catch { }  # logging must never break the poll
    }
}

function Write-Log([string]$m)  { Write-Line 'INFO' $m }
function Write-Warn([string]$m) { Write-Line 'WARNING' $m }

# StrictMode-safe property read of a ConvertFrom-Json object (missing property -> $null,
# instead of the StrictMode "property cannot be found" terminating error).
function Get-Prop($obj, [string]$name) {
    if ($null -eq $obj) { return $null }
    $p = $obj.PSObject.Properties[$name]
    if ($p) { return $p.Value }
    return $null
}

# ---------------------------------------------------------------------------
# Config (mirror parse_conf / _load_conf / resolve_connection).
# ---------------------------------------------------------------------------
function ConvertFrom-GridcoinConf([string]$text) {
    # Strip from the first '#' (drops inline and full-line comments), key=value, and on a
    # repeated key the FIRST value wins -- matching the core (reverse_precedence,
    # src/util/settings.cpp). The core disallows '#' in rpcpassword, so this is safe.
    $conf = @{}
    foreach ($raw in ($text -split "`n")) {
        $line = ($raw -split '#', 2)[0].Trim()
        if (-not $line -or ($line -notmatch '=')) { continue }
        $idx = $line.IndexOf('=')
        $k = $line.Substring(0, $idx).Trim()
        $v = $line.Substring($idx + 1).Trim()
        if ($k -and -not $conf.ContainsKey($k)) { $conf[$k] = $v }   # first occurrence wins
    }
    return $conf
}

function Get-GridcoinConf {
    if ($Conf) { $candidates = @($Conf) }
    elseif ($DataDir) {
        # Core default is gridcoinresearch.conf; gridcoin.conf is a common packaging alias.
        $candidates = @((Join-Path $DataDir 'gridcoinresearch.conf'), (Join-Path $DataDir 'gridcoin.conf'))
    }
    else { return @{} }
    foreach ($p in $candidates) {
        if (Test-Path -LiteralPath $p) { return (ConvertFrom-GridcoinConf ([IO.File]::ReadAllText($p))) }
    }
    return @{}
}

# InterpretBool (src/util/system.cpp): an empty value is true; otherwise the integer value,
# with a non-integer parsing as 0 (false).
function Test-ConfBool($value) {
    if ($null -eq $value) { return $false }
    $text = ([string]$value).Trim()
    if (-not $text) { return $true }
    $n = 0
    if (-not [int]::TryParse($text, [ref]$n)) { return $false }
    return ($n -ne 0)
}

# The chain this config selects, mirroring ArgsManager::GetChainName(): the regtest/testnet
# booleans, else chain=, else 'main'. Returns $null when the config is ambiguous (more than
# one selector -- which the core rejects outright) or names a chain we have no default port
# for; the caller must then refuse to guess (see $DEFAULT_RPC_PORTS).
function Get-ConfChain($cfgTable) {
    # Config key -> the chain it selects. Order is irrelevant: more than one selector is
    # ambiguous either way.
    $chainFlags = @{ 'regtest' = 'regtest'; 'testnet' = 'test' }
    $selectors = New-Object System.Collections.Generic.List[string]
    foreach ($key in $chainFlags.Keys) {
        if ($cfgTable.ContainsKey($key) -and (Test-ConfBool $cfgTable[$key])) { [void]$selectors.Add($chainFlags[$key]) }
    }
    if ($cfgTable.ContainsKey('chain') -and $cfgTable['chain']) {
        [void]$selectors.Add(([string]$cfgTable['chain']).Trim().ToLowerInvariant())
    }
    if ($selectors.Count -gt 1) { return $null }
    if ($selectors.Count -eq 1) {
        if ($DEFAULT_RPC_PORTS.ContainsKey($selectors[0])) { return $selectors[0] }
        return $null
    }
    return 'main'
}

# ---------------------------------------------------------------------------
# Loopback classification (mirror is_loopback).
# ---------------------------------------------------------------------------
# Classify by PARSING the host as an IP address, never by string prefix. A DNS name like
# "127.0.0.1.attacker.example" starts with "127." but resolves off-box: a prefix test would
# call it loopback, run the ownership gate against our OWN port (which answers "ours"), and
# then POST the Basic-auth header and the passphrase to the remote host in cleartext.
# Anything that is not a loopback IP literal is remote -- with the single exception of the
# name "localhost", which RFC 6761 reserves and requires resolution to map to loopback.
# Treating that as remote would be worse than the prefix ever was: a remote target SKIPS the
# gate entirely, so the very common rpcconnect=localhost would lose its ownership check.
function Test-LoopbackHost([string]$hostName) {
    $h = ([string]$hostName).Trim().ToLowerInvariant()
    if ($h -eq 'localhost') { return $true }
    if ($h.StartsWith('[') -and $h.EndsWith(']') -and $h.Length -gt 2) { $h = $h.Substring(1, $h.Length - 2) }
    $addr = $null
    if (-not [System.Net.IPAddress]::TryParse($h, [ref]$addr)) { return $false }   # a DNS name
    return [System.Net.IPAddress]::IsLoopback($addr)
}

# ---------------------------------------------------------------------------
# Passphrase at rest: DPAPI CurrentUser (mirror read_passphrase's decode rules).
# ---------------------------------------------------------------------------
function Read-DpapiPassphrase([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { throw "credential blob not found: $path" }
    Add-Type -AssemblyName System.Security
    $blob = [IO.File]::ReadAllBytes($path)
    $bytes = [Security.Cryptography.ProtectedData]::Unprotect($blob, $null, 'CurrentUser')
    try {
        $pw = [Text.Encoding]::UTF8.GetString($bytes)
        if ($pw.Length -gt 0 -and [int][char]$pw[0] -eq 0xFEFF) { $pw = $pw.Substring(1) }  # drop a BOM
        $pw = $pw -replace '[\r\n]+$', ''                                                   # trailing CR/LF only
        if (-not $pw.Trim()) { throw 'decrypted passphrase is empty or whitespace-only' }
        return $pw
    } finally {
        [Array]::Clear($bytes, 0, $bytes.Length)
    }
}

# ---------------------------------------------------------------------------
# Native listener-ownership gate (mirror check_listener_ownership).
# Returns 'ours' | 'none' | 'foreign' | 'unreadable'.
# ---------------------------------------------------------------------------
function Get-ListenerOwnership([int]$port) {
    $mySid = ([Security.Principal.WindowsIdentity]::GetCurrent()).User.Value
    # Get-NetTCPConnection returns the full v4+v6 table; an empty/failed query means
    # "nothing we can see is listening" -> 'none'. We never send on 'none', so treating a
    # query miss as 'none' is safe. Per-listener owner attribution is where fail-closed
    # matters (below): a listener we cannot attribute to ourselves -> 'unreadable' -> refuse.
    try {
        $listeners = @(Get-NetTCPConnection -State Listen -LocalPort $port -ErrorAction SilentlyContinue)
    } catch {
        return 'none'
    }
    if ($listeners.Count -eq 0) { return 'none' }
    $sawForeign = $false
    foreach ($l in $listeners) {
        $ownerSid = $null
        try {
            $proc = Get-CimInstance Win32_Process -Filter "ProcessId=$($l.OwningProcess)" -ErrorAction Stop
            if ($proc) {
                $res = Invoke-CimMethod -InputObject $proc -MethodName GetOwnerSid -ErrorAction Stop
                if ((Get-Prop $res 'ReturnValue') -eq 0) { $ownerSid = [string](Get-Prop $res 'Sid') }
            }
        } catch {
            $ownerSid = $null
        }
        if ([string]::IsNullOrEmpty($ownerSid)) { return 'unreadable' }  # cannot attribute -> fail closed
        if ($ownerSid -ne $mySid) { $sawForeign = $true }
    }
    if ($sawForeign) { return 'foreign' }
    return 'ours'
}

# Run the gate. Returns an EXIT_* code to stop this poll (nothing sent), or $null to proceed.
function Invoke-Gate([int]$port, [bool]$loopback, [bool]$canGate) {
    if (-not $loopback) {
        return $null  # remote target: the ownership gate cannot apply (warned at startup); proceed
    }
    if (-not $canGate) {
        if ($AllowUnverifiedListener) { return $null }
        Write-Warn ("cannot verify the owner of the loopback RPC port {0} on this host; refusing to send " +
            "credentials. Use -AllowUnverifiedListener on a trusted single-user host to override." -f $port)
        return $EXIT_UNRECOVERABLE
    }
    switch (Get-ListenerOwnership $port) {
        'foreign' {
            Write-Warn ("REFUSING TO SEND CREDENTIALS: a process owned by another user is listening on the " +
                "RPC port {0}. Either the core runs as a different user than expected, or a local account has " +
                "bound the RPC port. Investigate before relying on autounlock." -f $port)
            return $EXIT_UNRECOVERABLE
        }
        'unreadable' {
            Write-Warn ("cannot attribute the owner of a listener on RPC port {0}; refusing to send credentials " +
                "to an unverified listener." -f $port)
            return $EXIT_UNRECOVERABLE
        }
        'none' { return $EXIT_TRANSIENT }   # nothing listening yet; the core is still coming up
        default { return $null }            # 'ours': proceed
    }
}

# ---------------------------------------------------------------------------
# Proxy-free JSON-RPC. Returns a discriminated result (no exceptions for RPC-level errors):
#   @{ Ok = $true;  Result = <object> }
#   @{ Ok = $false; Code = <jsonrpc code|$null>; Http = <status|$null>; Msg = <string> }
# ---------------------------------------------------------------------------
function Invoke-GridcoinRpc([string]$url, [string]$auth, [string]$method, [object[]]$params) {
    $body = (@{ jsonrpc = '1.0'; id = 'autounlock'; method = $method; params = $params } | ConvertTo-Json -Compress -Depth 6)
    $bytes = [Text.Encoding]::UTF8.GetBytes($body)
    try {
        $req = [System.Net.HttpWebRequest]::Create($url)
        $req.Proxy = $null                       # proxy-free: never divert the passphrase POST off-box
        $req.Method = 'POST'
        $req.ContentType = 'application/json'
        $req.Timeout = 10000
        $req.ReadWriteTimeout = 10000
        $req.KeepAlive = $false
        $req.Headers['Authorization'] = $auth
        $req.ContentLength = $bytes.Length
        $rs = $req.GetRequestStream()
        try { $rs.Write($bytes, 0, $bytes.Length) } finally { $rs.Close() }

        $resp = $req.GetResponse()
        try {
            $sr = New-Object IO.StreamReader($resp.GetResponseStream())
            try { $text = $sr.ReadToEnd() } finally { $sr.Close() }
        } finally { $resp.Close() }
    }
    catch [System.Net.WebException] {
        $r = $_.Exception.Response
        if ($null -ne $r) {
            # Always close the error response: HttpWebResponse holds the connection until closed,
            # and the resident loop can hit repeated HTTP errors -- unclosed responses would leak
            # handles and exhaust the per-endpoint connection pool. The returns below still run
            # this finally before the function returns.
            try {
                $status = [int]([System.Net.HttpWebResponse]$r).StatusCode
                if ($status -eq 401) {
                    return @{ Ok = $false; Code = $null; Http = 401; Msg = 'HTTP 401 Unauthorized: rpcuser/rpcpassword rejected by the daemon' }
                }
                # Gridcoin returns JSON-RPC errors with HTTP 500 and the error object in the body.
                $eo = $null
                try {
                    $er = New-Object IO.StreamReader($r.GetResponseStream())
                    try { $eb = $er.ReadToEnd() } finally { $er.Close() }
                    $eo = (Get-Prop ($eb | ConvertFrom-Json) 'error')
                } catch { $eo = $null }
                if ($eo) {
                    return @{ Ok = $false; Code = (Get-Prop $eo 'code'); Http = $status; Msg = [string](Get-Prop $eo 'message') }
                }
                return @{ Ok = $false; Code = $null; Http = $status; Msg = ("HTTP {0} error" -f $status) }
            } finally {
                $r.Close()
            }
        }
        # Transport-level (connection refused, timeout): no JSON-RPC code; retry may help.
        return @{ Ok = $false; Code = $null; Http = $null; Msg = [string]$_.Exception.Message }
    }
    catch {
        return @{ Ok = $false; Code = $null; Http = $null; Msg = [string]$_.Exception.Message }
    }

    $obj = $null
    try { $obj = $text | ConvertFrom-Json } catch { return @{ Ok = $false; Code = $null; Http = $null; Msg = 'RPC returned a non-JSON body' } }
    $err = Get-Prop $obj 'error'
    if ($err) {
        return @{ Ok = $false; Code = (Get-Prop $err 'code'); Http = $null; Msg = [string](Get-Prop $err 'message') }
    }
    return @{ Ok = $true; Result = (Get-Prop $obj 'result') }
}

# ---------------------------------------------------------------------------
# One poll (mirror run_once). Returns @{ Uptime = <int|$null>; Code = <EXIT_*> }.
# ---------------------------------------------------------------------------
function Invoke-Poll([string]$url, [string]$auth, [int]$port, [bool]$loopback, [bool]$canGate, [string]$passphrase, [int]$timeout, $prevUptime) {
    $gateCode = Invoke-Gate $port $loopback $canGate
    if ($null -ne $gateCode) { return @{ Uptime = $prevUptime; Code = $gateCode } }

    $info = Invoke-GridcoinRpc $url $auth 'getinfo' @()
    if (-not $info.Ok) {
        if ($info.Http -eq 401) {
            Write-Warn 'RPC rejected the credentials (HTTP 401): rpcuser/rpcpassword are wrong -- retrying cannot fix this'
            return @{ Uptime = $prevUptime; Code = $EXIT_UNRECOVERABLE }
        }
        return @{ Uptime = $prevUptime; Code = $EXIT_TRANSIENT }   # core down / RPC not bound yet
    }
    $u = Get-Prop $info.Result 'uptime'
    if ($null -eq $u) { return @{ Uptime = $prevUptime; Code = $EXIT_TRANSIENT } }
    try { $curUptime = [int]$u } catch { return @{ Uptime = $prevUptime; Code = $EXIT_TRANSIENT } }

    # should_unlock: first contact, or the core restarted (uptime went backwards).
    $needUnlock = ($null -eq $prevUptime) -or ($curUptime -lt $prevUptime)
    if (-not $needUnlock) { return @{ Uptime = $curUptime; Code = $EXIT_OK } }

    # Re-gate immediately before the only call that carries the passphrase. The gate at the
    # top of this poll ran BEFORE the getinfo round trip, and every request opens a fresh
    # connection (KeepAlive = $false) -- so the core could have exited in that window and a
    # foreign process won the bind race. Re-checking here closes that TOCTOU: the ownership
    # check and the passphrase POST are now adjacent, with no round trip between them. The
    # baseline stays at $prevUptime so the next poll retries the unlock.
    $gateCode = Invoke-Gate $port $loopback $canGate
    if ($null -ne $gateCode) { return @{ Uptime = $prevUptime; Code = $gateCode } }

    $r = Invoke-GridcoinRpc $url $auth 'walletpassphrase' @($passphrase, $timeout, $true)   # stake-only
    if ($r.Ok) {
        Write-Log 'wallet unlocked (stake-only)'
        return @{ Uptime = $curUptime; Code = $EXIT_OK }
    }
    if ($r.Http -eq 401) {
        Write-Warn 'RPC rejected the credentials (HTTP 401) -- retrying cannot fix this'
        return @{ Uptime = $curUptime; Code = $EXIT_UNRECOVERABLE }
    }
    $code = $r.Code
    if ($code -eq $RPC_WALLET_ALREADY_UNLOCKED) {
        # -17 is thrown before the stake-only flag is assigned, so the wallet is unlocked
        # but by another path and the staking-only restriction was NOT re-asserted -- if
        # that path was a full GUI unlock it is spendable. No RPC exposes the flag.
        Write-Warn 'wallet was already unlocked by another path (-17); the staking-only restriction was NOT re-asserted and may not be in effect'
        return @{ Uptime = $curUptime; Code = $EXIT_OK }
    }
    if ($code -eq $RPC_WALLET_WRONG_ENC_STATE) {
        Write-Warn 'wallet is not encrypted (-15); there is nothing to unlock'
        return @{ Uptime = $curUptime; Code = $EXIT_OK }
    }
    if ($code -eq $RPC_WALLET_PASSPHRASE_INCORRECT -or $code -eq $RPC_INVALID_PARAMETER) {
        Write-Warn ("walletpassphrase was rejected ({0}): {1} -- retrying cannot fix this" -f $code, $r.Msg)
        return @{ Uptime = $curUptime; Code = $EXIT_UNRECOVERABLE }
    }
    Write-Warn ("walletpassphrase failed: {0}" -f $r.Msg)
    return @{ Uptime = $curUptime; Code = $EXIT_TRANSIENT }
}

# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------
# Note: the local is $cfg, NOT $conf -- the script has a [string]$Conf parameter, and
# PowerShell variable names are case-insensitive, so assigning the hashtable to $conf would
# be coerced by that [string] type constraint into the string "System.Collections.Hashtable".
$cfg = Get-GridcoinConf
$rpcUser = if ($RpcUser) { $RpcUser } elseif ($cfg.ContainsKey('rpcuser')) { $cfg['rpcuser'] } else { $null }
$rpcPass = if ($cfg.ContainsKey('rpcpassword')) { $cfg['rpcpassword'] } else { $null }
$rpcHost = if ($RpcConnect) { $RpcConnect } elseif ($cfg.ContainsKey('rpcconnect')) { $cfg['rpcconnect'] } else { '127.0.0.1' }
if ($RpcPort -gt 0) {
    $port = $RpcPort
} elseif ($cfg.ContainsKey('rpcport')) {
    $parsedPort = 0
    if (-not [int]::TryParse([string]$cfg['rpcport'], [ref]$parsedPort)) {
        Write-Warn ("rpcport in the config is not an integer ('{0}')" -f $cfg['rpcport'])
        exit $EXIT_UNRECOVERABLE
    }
    $port = $parsedPort
} else {
    # No explicit port: take the default of the chain the config selects, never a blanket
    # mainnet guess (see $DEFAULT_RPC_PORTS).
    $chain = Get-ConfChain $cfg
    if ($null -eq $chain) {
        Write-Warn ('the config omits rpcport and does not select a chain unambiguously, so the RPC port ' +
            'cannot be guessed safely -- set rpcport in the config file (or pass -RpcPort). Guessing the ' +
            'mainnet port on a non-mainnet node would send this wallet''s passphrase to whichever wallet ' +
            'is listening there.')
        exit $EXIT_UNRECOVERABLE
    }
    $port = [int]$DEFAULT_RPC_PORTS[$chain]
}

if (-not $rpcUser -or -not $rpcPass) {
    Write-Warn 'rpcuser and rpcpassword must be set in the config file (or pass -RpcUser and set rpcpassword in the conf)'
    exit $EXIT_UNRECOVERABLE
}

# Decrypt the passphrase ONCE at startup (mirror main()). A bad blob or the wrong user
# context is unrecoverable -- do not enter the loop.
try {
    $passphrase = Read-DpapiPassphrase $CredPath
} catch {
    Write-Warn ("could not read the DPAPI passphrase blob: {0}" -f $_.Exception.Message)
    exit $EXIT_UNRECOVERABLE
}
Register-Secret $passphrase
Register-Secret $rpcPass
$token = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes(("{0}:{1}" -f $rpcUser, $rpcPass)))
Register-Secret $token
$auth = "Basic $token"

# Bracket an IPv6 literal host so the URL is valid: http://[::1]:port/, not http://::1:port/.
$urlHost = if ($rpcHost.Contains(':') -and -not $rpcHost.StartsWith('[')) { "[$rpcHost]" } else { $rpcHost }
$url = "http://{0}:{1}/" -f $urlHost, $port

# Gate applicability: a loopback target with the gate mechanism (Get-NetTCPConnection) present.
$loopback = Test-LoopbackHost $rpcHost
$canGate = $false
if ($loopback) {
    try { Get-Command Get-NetTCPConnection -ErrorAction Stop | Out-Null; $canGate = $true } catch { $canGate = $false }
    if (-not $canGate -and -not $AllowUnverifiedListener) {
        Write-Warn 'the listener-ownership gate is unavailable (Get-NetTCPConnection missing); credentials will NOT be sent until -AllowUnverifiedListener is set on a trusted single-user host'
    }
} else {
    Write-Warn ("RPC target {0} is not loopback; the listener-ownership gate cannot apply and credentials will be sent to the configured host." -f $rpcHost)
}

$prevUptime = $null
while ($true) {
    try {
        $result = Invoke-Poll $url $auth $port $loopback $canGate $passphrase $Timeout $prevUptime
        $prevUptime = $result.Uptime
        $code = $result.Code
    } catch {
        # The resident loop must never die and must never print an unredacted error.
        Write-Warn ("unexpected error during poll: {0}" -f (Format-Redacted ([string]$_.Exception.Message)))
        $code = $EXIT_TRANSIENT
    }
    if ($Once) { exit $code }
    Start-Sleep -Seconds $Interval
}
