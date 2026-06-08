# Gridcoin Network Status API Proxy

A thin FastAPI server that exposes read-only endpoints over the local
Gridcoin wallet's JSON-RPC interface and its Scraper data files.
Intended to run on the same host as a Gridcoin wallet running with the
`-scraper` option, so the website (and other read-only consumers) can
fetch live network data without each browser needing a direct
connection to the wallet.

## Endpoints

| Path | Source | Notes |
|---|---|---|
| `GET /health` | — | Cache freshness |
| `GET /api/v1/network-status` | Wallet RPC + Scraper `ConvergedStats.csv.gz` | Superblock + project snapshot; 5 min server-side cache |
| `GET /api/v1/history/cpid-churn` | `gridcoin_stats.duckdb` | Daily active / churn in-out CPID counts |
| `GET /api/v1/history/cpids` | `gridcoin_stats.duckdb` | Distinct CPIDs ever seen (autocomplete source) |
| `GET /api/v1/history/cpid-project-magnitude` | `gridcoin_stats.duckdb` | Per-project daily magnitude time series for one CPID; `?cpid=HEX` |
| `GET /api/v1/history/mrc-daily` | `gridcoin_stats.duckdb` | Daily MRC payment activity (count, fee-boosted count, gross/net/fee GRC splits) |
| `GET /api/v1/history/project-active-cpids` | `gridcoin_stats.duckdb` | Daily active CPID count per project |
| `GET /api/v1/history/project-churn` | `gridcoin_stats.duckdb` | Daily project count in superblock + adds/drops |
| `GET /api/v1/history/projects` | `gridcoin_stats.duckdb` | Distinct project names for UI filters |
| `GET /api/v1/history/block-version-activations` | `gridcoin_stats.duckdb` | Empirically-derived activation date per block version (predecessor-filtered) |
| `GET /api/v1/history/releases` | GitHub API | Release tags + dates classified `mandatory` / `leisure` / `unknown`; long-cached, served stale on upstream error |
| `GET /api/v1/history/top-cpids` | `gridcoin_stats.duckdb` (all-time) or `fact_stats` (windowed) | CPID leaderboard; `?project=`, `?limit=N`, `?order_by=COL`, `?period=all\|week\|month\|quarter\|year` (canned rolling windows, cached), `?start_date=YYYY-MM-DD&end_date=YYYY-MM-DD` (custom range, `Cache-Control: no-store`). All-time path reads precomputed summary; windowed paths aggregate on-demand from `fact_stats` |

All `/api/v1/*` endpoints are rate-limited per remote IP. `GET` only;
CORS is explicit-origin-list (see `CORS_ORIGINS` env var).

## Configuration

Environment variables (all optional):

| Var | Default | Purpose |
|---|---|---|
| `GRIDCOIN_CONF` | `~/.GridcoinResearch/gridcoinresearch.conf` | Wallet config (for `rpcuser` / `rpcpassword` / `rpcport`) |
| `SCRAPER_DATA_DIR` | `~/.GridcoinResearch/Scraper` | Scraper output directory |
| `STATS_DB_PATH` | `~/.GridcoinResearch/analytics/gridcoin_stats.duckdb` | Analytics DuckDB (optional — history endpoints 503 if absent) |
| `CORS_ORIGINS` | `https://gridcoin.us` | Comma-separated allowed browser origins |
| `LISTEN_HOST` | `127.0.0.1` | Bind address |
| `LISTEN_PORT` | `5000` | Bind port |
| `FORWARDED_ALLOW_IPS` | `127.0.0.1` | Trusted upstream IPs for `X-Forwarded-For` honoring (rate-limit key). Set to `*` only if every upstream that can reach `LISTEN_HOST` is fully trusted — otherwise spoofing the rate-limit key becomes trivial |
| `CACHE_TTL` | `300` | In-memory cache TTL for `/api/v1/network-status` (seconds) |
| `HISTORY_CACHE_TTL` | `3600` | `Cache-Control: max-age` for history endpoints |
| `GITHUB_REPO` | `gridcoin-community/Gridcoin-Research` | Source for the `/api/v1/history/releases` endpoint |
| `GITHUB_TOKEN` | _(unset)_ | Optional. With a personal access token the unauthenticated GitHub rate limit (60/h per source IP) lifts to 5000/h. Read-only `public_repo` scope is sufficient |
| `RELEASES_CACHE_TTL` | `21600` | `Cache-Control: max-age` for `/api/v1/history/releases` (default 6 h) |

## Running locally

```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
CORS_ORIGINS="https://gridcoin.us,http://localhost:4001" \
    .venv/bin/python server.py
```

## Concurrency model

- The wallet-RPC / scraper cache is refreshed on a background asyncio task every `CACHE_TTL` seconds.
- History endpoints open a fresh short-lived read-only DuckDB connection per request. The connection is closed immediately so the daily analytics refresh never contends with a long-lived reader. When the refresh writer briefly holds an exclusive lock, the endpoints return `503` and the client can retry.

## Deployment (production target: AWS scraper VPS)

The proxy runs behind nginx/caddy on the scraper host which terminates TLS for `api.gridcoin.us`. The analytics database is populated by the sibling systemd chain (`record-converged-stats` → `ingest-converged-stats` → `refresh-converged-stats`) documented alongside the analytics tooling.

### Reverse-proxy + rate-limit key

Per-IP rate limiting (`30/minute` on history endpoints, slowapi) keys
on `request.client.host`. With nginx/caddy on the same host as the
proxy, that's always `127.0.0.1` unless the proxy honors the
upstream's `X-Forwarded-For` header.

`server.py` runs uvicorn with `proxy_headers=True` and
`forwarded_allow_ips=$FORWARDED_ALLOW_IPS` (default `127.0.0.1`)
which rewrites the ASGI scope's `client` tuple from the trusted
upstream's XFF, so the per-IP key reflects the real browser. If you
launch via a different process supervisor, make sure the equivalent
`uvicorn --proxy-headers --forwarded-allow-ips=…` flags are passed.

The reverse-proxy needs to forward the client IP. For Caddy:

```caddyfile
api.gridcoin.us {
    reverse_proxy 127.0.0.1:5000 {
        header_up Host {host}
        header_up X-Real-IP {remote_host}
    }
}
```

Caddy adds `X-Forwarded-For` automatically; nginx requires explicit
`proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;`.
