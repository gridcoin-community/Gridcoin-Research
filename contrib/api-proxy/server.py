#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""
Gridcoin Network Status API Proxy

A thin FastAPI server that proxies read-only data from a local Gridcoin
wallet's JSON-RPC interface and scraper data files, serving it as a
public REST API for the Gridcoin website.

Configuration via environment variables:
    GRIDCOIN_CONF       Path to gridcoinresearch.conf (default: ~/.GridcoinResearch/gridcoinresearch.conf)
    SCRAPER_DATA_DIR    Path to Scraper data directory (default: ~/.GridcoinResearch/Scraper)
    CORS_ORIGINS        Comma-separated allowed origins (default: https://gridcoin.us)
    LISTEN_HOST         Bind address (default: 127.0.0.1)
    LISTEN_PORT         Bind port (default: 5000)
    CACHE_TTL           Cache refresh interval in seconds (default: 300)
"""

import asyncio
import csv
import datetime as _dt
import gzip
import json
import logging
import os
import threading
import time
from collections import defaultdict
from contextlib import asynccontextmanager
from pathlib import Path

import duckdb
import httpx
from fastapi import FastAPI, HTTPException, Request, Response
from fastapi.middleware.cors import CORSMiddleware
from fastapi.middleware.gzip import GZipMiddleware
from slowapi import Limiter
from slowapi.errors import RateLimitExceeded
from slowapi.util import get_remote_address
from starlette.responses import JSONResponse

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

GRIDCOIN_CONF = os.getenv(
    "GRIDCOIN_CONF",
    str(Path.home() / ".GridcoinResearch" / "gridcoinresearch.conf"),
)
SCRAPER_DATA_DIR = os.getenv(
    "SCRAPER_DATA_DIR",
    str(Path.home() / ".GridcoinResearch" / "Scraper"),
)
CORS_ORIGINS = [o.strip() for o in os.getenv("CORS_ORIGINS", "https://gridcoin.us").split(",") if o.strip()]
LISTEN_HOST = os.getenv("LISTEN_HOST", "127.0.0.1")
LISTEN_PORT = int(os.getenv("LISTEN_PORT", "5000"))
# Trusted reverse-proxy source IPs for X-Forwarded-* honoring. Comma-
# separated, default loopback-only since the typical deployment puts a
# Caddy/nginx terminator on the same host. Set to "*" only if you fully
# trust whatever upstream is rewriting client headers — otherwise a
# rate-limit key spoof becomes trivial.
FORWARDED_ALLOW_IPS = os.getenv("FORWARDED_ALLOW_IPS", "127.0.0.1")
CACHE_TTL = int(os.getenv("CACHE_TTL", "300"))
STATS_DB_PATH = os.getenv(
    "STATS_DB_PATH",
    str(Path.home() / ".GridcoinResearch" / "analytics" / "gridcoin_stats.duckdb"),
)
# Cache-Control max-age for history endpoints. The data only changes once a
# day when the refresh service runs, so long caches are fine.
HISTORY_CACHE_TTL = int(os.getenv("HISTORY_CACHE_TTL", "3600"))
# GitHub releases metadata source for the release-annotation overlay on the
# analytics charts. Cached aggressively; releases are infrequent and the
# unauth'd GitHub rate limit is tight (60 req/hour per IP).
GITHUB_REPO = os.getenv("GITHUB_REPO", "gridcoin-community/Gridcoin-Research")
GITHUB_TOKEN = os.getenv("GITHUB_TOKEN")  # optional; lifts rate limit to 5000/hr
RELEASES_CACHE_TTL = int(os.getenv("RELEASES_CACHE_TTL", "21600"))  # 6 hours

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(message)s",
)
log = logging.getLogger("gridcoin-api-proxy")

# ---------------------------------------------------------------------------
# Wallet RPC client
# ---------------------------------------------------------------------------


def read_wallet_config(conf_path: str) -> dict:
    """Parse rpcuser, rpcpassword, rpcport from the wallet config file.

    Raises FileNotFoundError if the file is missing and ValueError if
    rpcuser or rpcpassword are absent. The module's startup hook
    catches these and surfaces them via FastAPI's lifespan, so an
    interactive `import server` (e.g. for tests, REPL, doc tooling)
    does not abort the process the way a bare `sys.exit()` would.
    """
    config = {"rpcport": 15715}
    with open(conf_path, "r", encoding="utf8") as f:
        for line in f:
            line = line.strip()
            if line.startswith("#") or "=" not in line:
                continue
            key, _, value = line.partition("=")
            key = key.strip()
            value = value.strip()
            if key == "rpcuser":
                config["rpcuser"] = value
            elif key == "rpcpassword":
                config["rpcpassword"] = value
            elif key == "rpcport":
                config["rpcport"] = int(value)

    if "rpcuser" not in config or "rpcpassword" not in config:
        raise ValueError(
            f"rpcuser/rpcpassword not found in {conf_path}"
        )

    return config


async def rpc_call(client: httpx.AsyncClient, url: str, auth: tuple,
                   method: str, params: list | None = None) -> dict:
    """Make a JSON-RPC call to the wallet."""
    payload = {
        "jsonrpc": "1.0",
        "id": method,
        "method": method,
        "params": params or [],
    }
    resp = await client.post(
        url,
        content=json.dumps(payload),
        headers={"Content-Type": "application/json"},
        auth=auth,
        timeout=60.0,
    )
    resp.raise_for_status()
    result = resp.json()
    if result.get("error"):
        raise RuntimeError(f"RPC error in {method}: {result['error']}")
    return result["result"]


# ---------------------------------------------------------------------------
# Data collection
# ---------------------------------------------------------------------------


def parse_converged_stats(scraper_dir: str) -> dict[str, int]:
    """Parse ConvergedStats.csv.gz and return per-project CPID counts."""
    stats_path = Path(scraper_dir) / "ConvergedStats.csv.gz"
    counts: dict[str, int] = defaultdict(int)

    try:
        with gzip.open(stats_path, "rt") as f:
            reader = csv.reader(f)
            for row in reader:
                if len(row) >= 2 and row[0] == "byCPIDbyProject":
                    counts[row[1]] += 1
    except FileNotFoundError:
        log.warning("ConvergedStats.csv.gz not found at %s", stats_path)
    except Exception as e:
        log.warning("Error parsing ConvergedStats.csv.gz: %s", e)

    return dict(counts)


async def collect_network_status(rpc_url: str, rpc_auth: tuple,
                                  scraper_dir: str) -> dict:
    """Fetch all data sources and merge into the API response."""
    async with httpx.AsyncClient() as client:
        # Fetch all RPC data concurrently.
        list_projects_task = rpc_call(client, rpc_url, rpc_auth,
                                      "listprojects", [True])
        greylist_task = rpc_call(client, rpc_url, rpc_auth,
                                 "getautogreylist", [True])
        superblock_task = rpc_call(client, rpc_url, rpc_auth,
                                   "superblocks", [1, True])

        list_projects, greylist_data, superblock_data = await asyncio.gather(
            list_projects_task, greylist_task, superblock_task
        )

    # Parse scraper stats (synchronous but fast — ~300KB gzipped).
    cpid_counts = parse_converged_stats(scraper_dir)

    # Extract superblock data.
    sb = superblock_data[0] if superblock_data else {}
    sb_projects = sb.get("contract_contents", {}).get("projects", {})

    # Build greylist lookup: project name → greylist metrics.
    # Note: the RPC output has a trailing colon in the key name ("project:").
    greylist_lookup = {}
    for entry in greylist_data.get("auto_greylist_projects", []):
        name = entry.get("project:", "").rstrip(":")
        if not name:
            name = entry.get("project", "")
        greylist_lookup[name] = entry

    # Merge everything into the response.
    projects = {}
    for name, proj in list_projects.items():
        status = proj.get("status", "Unknown")

        # Skip deleted projects.
        if status == "Deleted":
            continue

        project_entry = {
            "status": status,
            "gdpr_controls": proj.get("gdpr_controls", False),
            "display_name": proj.get("display_name", name),
            "display_url": proj.get("display_url", ""),
            "stats_url": proj.get("stats_url", ""),
        }

        # A project can be Active on the whitelist but excluded from the
        # current superblock if its stats have gone stale (no scraper
        # manifest parts updated in 48+ hours). Flag this explicitly so the
        # frontend can distinguish "active and earning" from "active but
        # temporarily excluded."
        in_superblock = name in sb_projects
        project_entry["in_superblock"] = in_superblock

        if in_superblock:
            sb_proj = sb_projects[name]
            project_entry["rac"] = sb_proj.get("rac", 0)
            project_entry["average_rac"] = sb_proj.get("average_rac", 0)
            project_entry["total_credit"] = sb_proj.get("total_credit", 0)

        # Add CPID count from scraper data.
        if name in cpid_counts:
            project_entry["cpid_count"] = cpid_counts[name]

        # Add greylist metrics.
        if name in greylist_lookup:
            gl = greylist_lookup[name]
            project_entry["zcd"] = gl.get("zcd", 0)
            project_entry["was"] = round(gl.get("was", 0), 4)
            project_entry["meets_greylist_criteria"] = gl.get(
                "meets_greylist_criteria", False)

        projects[name] = project_entry

    return {
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "superblock": {
            "height": sb.get("height", 0),
            "date": sb.get("date", ""),
            "total_cpids": sb.get("total_cpids", 0),
            "active_beacons": sb.get("active_beacons", 0),
            "total_magnitude": sb.get("total_magnitude", 0),
            "total_projects": sb.get("total_projects", 0),
        },
        "projects": projects,
    }


# ---------------------------------------------------------------------------
# Cache
# ---------------------------------------------------------------------------

_cache: dict = {}
_cache_lock = asyncio.Lock()


_refresh_inflight: asyncio.Task | None = None


async def _do_refresh(rpc_url: str, rpc_auth: tuple, scraper_dir: str) -> dict:
    """Actually fetch and update the cache. Caller is the single-flight winner."""
    global _cache
    try:
        data = await collect_network_status(rpc_url, rpc_auth, scraper_dir)
        data["_fetched_at"] = time.time()
        async with _cache_lock:
            _cache = data
        log.info("Cache refreshed: %d projects, superblock %s",
                 len(data.get("projects", {})),
                 data.get("superblock", {}).get("height", "?"))
        return data
    except Exception as e:
        log.error("Failed to refresh cache: %s", e)
        if _cache:
            log.info("Serving stale cache.")
            return _cache
        raise


async def get_cached_status(rpc_url: str, rpc_auth: tuple,
                             scraper_dir: str) -> dict:
    """Return cached network status, refreshing if stale.

    Refreshes are single-flighted: when the entry expires, the first
    request creates an asyncio.Task to do the wallet RPC + scraper
    read. Subsequent requests that arrive while that task is in flight
    await the same task instead of triggering parallel RPCs against
    the wallet. Without this, a burst of requests right after TTL
    expiry would multiply wallet load and head-of-line block each
    other for the same data.
    """
    global _refresh_inflight

    async with _cache_lock:
        now = time.time()
        if _cache and now - _cache.get("_fetched_at", 0) < CACHE_TTL:
            return _cache
        # Stale or missing — coalesce concurrent refreshes onto a
        # single task. The task itself updates _cache and clears the
        # in-flight slot before returning.
        if _refresh_inflight is None or _refresh_inflight.done():
            _refresh_inflight = asyncio.create_task(
                _do_refresh(rpc_url, rpc_auth, scraper_dir)
            )
        task = _refresh_inflight

    try:
        return await task
    finally:
        # First waiter to finish clears the slot so the next stale
        # period starts a fresh task (the `done()` check on entry
        # handles the case where someone else already reset it).
        async with _cache_lock:
            if _refresh_inflight is task:
                _refresh_inflight = None


# ---------------------------------------------------------------------------
# Background refresh
# ---------------------------------------------------------------------------

_refresh_task: asyncio.Task | None = None


async def background_refresh(rpc_url: str, rpc_auth: tuple,
                              scraper_dir: str):
    """Periodically refresh the cache in the background."""
    while True:
        await asyncio.sleep(CACHE_TTL)
        try:
            await get_cached_status(rpc_url, rpc_auth, scraper_dir)
        except Exception as e:
            log.error("Background refresh failed: %s", e)


# ---------------------------------------------------------------------------
# FastAPI application
# ---------------------------------------------------------------------------

# Resolved at startup inside lifespan() so an unreadable wallet
# config raises through FastAPI/Uvicorn rather than exiting the
# process at import time.
RPC_URL: str | None = None
RPC_AUTH: tuple | None = None

limiter = Limiter(key_func=get_remote_address)


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Start background cache refresh on startup."""
    global _refresh_task, RPC_URL, RPC_AUTH

    # Wallet config — read here rather than at module import so a
    # missing/bad config raises cleanly through Uvicorn's startup
    # path instead of a bare sys.exit() during `import server`.
    wallet_conf = read_wallet_config(GRIDCOIN_CONF)
    RPC_URL = f"http://127.0.0.1:{wallet_conf['rpcport']}/"
    RPC_AUTH = (wallet_conf["rpcuser"], wallet_conf["rpcpassword"])

    # Initial cache population.
    log.info("Populating initial cache...")
    try:
        await get_cached_status(RPC_URL, RPC_AUTH, SCRAPER_DATA_DIR)
    except Exception as e:
        log.error("Initial cache population failed: %s", e)

    # Start background refresh.
    _refresh_task = asyncio.create_task(
        background_refresh(RPC_URL, RPC_AUTH, SCRAPER_DATA_DIR)
    )
    log.info("Background refresh started (interval: %ds)", CACHE_TTL)

    yield

    # Shutdown.
    if _refresh_task:
        _refresh_task.cancel()
        try:
            await _refresh_task
        except asyncio.CancelledError:
            pass
    log.info("Shutdown complete.")


app = FastAPI(
    title="Gridcoin Network Status API",
    version="1.0.0",
    lifespan=lifespan,
)

app.state.limiter = limiter

app.add_middleware(
    CORSMiddleware,
    allow_origins=CORS_ORIGINS,
    allow_methods=["GET"],
    allow_headers=["*"],
)

# Compress responses over ~1 KB. The history endpoints return JSON
# that compresses ~5x; the network-status payload ~3-4x.
app.add_middleware(GZipMiddleware, minimum_size=1024)


@app.exception_handler(RateLimitExceeded)
async def rate_limit_handler(request: Request, exc: RateLimitExceeded):
    # Positional args required by FastAPI's exception-handler signature;
    # the response body is generic, so neither is referenced.
    _ = request, exc
    return JSONResponse(
        status_code=429,
        content={"error": "Rate limit exceeded. Try again later."},
    )


@app.get("/api/v1/network-status")
@limiter.limit("30/minute")
async def network_status(request: Request):
    """Return current Gridcoin network status including project data."""
    _ = request  # required by @limiter.limit; body reads only from cache
    data = await get_cached_status(RPC_URL, RPC_AUTH, SCRAPER_DATA_DIR)

    # Derive remaining freshness from the cached `_fetched_at`. When the
    # refresh failed and we're serving stale data (age >= CACHE_TTL),
    # tell clients/intermediaries not to cache it — otherwise they'd
    # treat already-stale data as fresh for another full CACHE_TTL.
    age = time.time() - data.get("_fetched_at", 0)
    if age >= CACHE_TTL:
        cache_control = "no-store"
    else:
        cache_control = f"public, max-age={max(0, int(CACHE_TTL - age))}"

    # Strip internal cache metadata.
    response_data = {k: v for k, v in data.items() if not k.startswith("_")}

    return JSONResponse(
        content=response_data,
        headers={"Cache-Control": cache_control},
    )


# ---------------------------------------------------------------------------
# Historical analytics endpoints (served from DuckDB summary tables)
# ---------------------------------------------------------------------------
#
# The analytics DB is rebuilt once a day at 00:00 UTC by the
# refresh-converged-stats.service systemd user unit. During that ~3 s write
# window the file is locked and reads will fail; we open a fresh short-lived
# read-only connection per request so no locks persist in the proxy process,
# and we translate lock errors into a retryable 503 response.


def _history_query(sql: str, params: tuple = ()) -> list[dict]:
    """Run a read-only query against the analytics DB and return a list of
    row dicts. Connection is opened and closed per call so the proxy never
    holds a reader lock that would block the nightly refresh. Date values
    are stringified to ISO-8601 for plain-JSON serialisation."""
    try:
        con = duckdb.connect(STATS_DB_PATH, read_only=True)
    except duckdb.IOException as e:
        # File locked by the refresh writer, or DB absent.
        raise HTTPException(
            status_code=503,
            detail="Analytics database is temporarily unavailable.",
        ) from e
    try:
        cursor = con.execute(sql, params)
        cols = [d[0] for d in cursor.description]
        rows = cursor.fetchall()
    finally:
        con.close()
    import datetime as _dt
    out = []
    for r in rows:
        d = {}
        for c, v in zip(cols, r):
            if isinstance(v, _dt.date):
                v = v.isoformat()
            d[c] = v
        out.append(d)
    return out


# Response cache for the history endpoints.
#
# Keyed by (endpoint_name, *param_values). The stored value is the
# already-JSON-encoded response body, so cache hits skip both the DuckDB
# query AND serialization (the 29k-row per-project payload alone takes
# ~100-300 ms to json.dumps on the t2.medium target host).
#
# Entries are invalidated when either:
#   * the cache TTL has elapsed, or
#   * the analytics DB file's mtime has advanced since we cached (i.e.
#     the daily refresh has run).
_history_cache: dict[tuple, tuple[float, bytes]] = {}
_history_cache_lock = threading.Lock()


def _analytics_db_mtime() -> float:
    try:
        return Path(STATS_DB_PATH).stat().st_mtime
    except FileNotFoundError:
        return 0.0


def _cached_history_response(cache_key: tuple, sql: str, params: tuple = ()) -> Response:
    """Serve a cached JSON body if fresh, otherwise run the query, cache,
    and return. Always sends `Cache-Control: public, max-age=N` so
    browsers / intermediaries can keep a copy for `HISTORY_CACHE_TTL` s."""
    db_mtime = _analytics_db_mtime()
    now = time.time()

    with _history_cache_lock:
        entry = _history_cache.get(cache_key)
    if entry is not None:
        cached_at, body = entry
        if cached_at >= db_mtime and (now - cached_at) <= HISTORY_CACHE_TTL:
            return Response(
                content=body,
                media_type="application/json",
                headers={"Cache-Control": f"public, max-age={HISTORY_CACHE_TTL}"},
            )

    data = _history_query(sql, params)
    body = json.dumps({"data": data}).encode("utf-8")

    with _history_cache_lock:
        _history_cache[cache_key] = (now, body)

    return Response(
        content=body,
        media_type="application/json",
        headers={"Cache-Control": f"public, max-age={HISTORY_CACHE_TTL}"},
    )


@app.get("/api/v1/history/cpid-churn")
@limiter.limit("30/minute")
async def history_cpid_churn(request: Request):
    """Daily time series of CPID activity — active count plus churn in/out."""
    _ = request  # required by @limiter.limit
    return _cached_history_response(
        cache_key=("cpid-churn",),
        sql="""
            SELECT obs_date,
                   active_cpids,
                   new_cpids,
                   returning_cpids,
                   churn_in,
                   churn_out,
                   departing_cpids
            FROM summary_cpid_churn
            ORDER BY obs_date
        """,
    )


@app.get("/api/v1/history/mrc-daily")
@limiter.limit("30/minute")
async def history_mrc_daily(request: Request):
    """Daily time series of MRC (Manual Research Claim) payment activity.

    One row per UTC day; days with zero MRC activity are present in the
    series with all-zero metrics (ingestion writes a row for every day
    that has a known [first_height, next_day.first_height-1] bracket).
    Sourced from the mrc_daily table populated by ingest_mrc.py.
    """
    _ = request  # required by @limiter.limit
    return _cached_history_response(
        cache_key=("mrc-daily",),
        sql="""
            SELECT obs_date,
                   mrcs_paid,
                   mrcs_fee_boosted,
                   gross_research,
                   net_to_researcher,
                   foundation_fees,
                   staker_fees,
                   calc_min_fees,
                   fee_boost
            FROM mrc_daily
            ORDER BY obs_date
        """,
    )


@app.get("/api/v1/history/block-version-activations")
@limiter.limit("30/minute")
async def history_block_version_activations(request: Request):
    """Dates each new block version first appeared in the chain, derived
    empirically from the block_version recorded alongside each day's
    first_height in daily_block_boundaries.

    Filtered to versions whose predecessor we also observed — without
    that filter we'd return the start of our data window as an
    "activation", which would be misleading (the data starts at v12
    activation, but v12 itself activated earlier).
    """
    _ = request  # required by @limiter.limit
    return _cached_history_response(
        cache_key=("block-version-activations",),
        sql="""
            WITH version_first_seen AS (
                SELECT block_version AS version,
                       MIN(obs_date) AS activation_date
                FROM daily_block_boundaries
                WHERE block_version IS NOT NULL
                GROUP BY block_version
            )
            SELECT version, activation_date
            FROM version_first_seen vfs
            WHERE EXISTS (
                SELECT 1 FROM version_first_seen vp
                WHERE vp.version = vfs.version - 1
            )
            ORDER BY version
        """,
    )


@app.get("/api/v1/history/project-active-cpids")
@limiter.limit("30/minute")
async def history_project_active_cpids(request: Request):
    """Daily time series of active (magnitude-positive) CPID counts per
    project. One row per (obs_date, project)."""
    _ = request  # required by @limiter.limit
    return _cached_history_response(
        cache_key=("project-active-cpids",),
        sql="""
            SELECT obs_date, project, active_cpids, contributing_cpids,
                   total_rac, total_mag
            FROM summary_project_active_cpids
            ORDER BY obs_date, project
        """,
    )


@app.get("/api/v1/history/project-churn")
@limiter.limit("30/minute")
async def history_project_churn(request: Request):
    """Daily time series of projects in the superblock — total count
    plus day-over-day in/out counts (NULL on gap-adjacent days)."""
    _ = request  # required by @limiter.limit
    return _cached_history_response(
        cache_key=("project-churn",),
        sql="""
            SELECT obs_date, total_projects, projects_in, projects_out
            FROM summary_project_churn
            ORDER BY obs_date
        """,
    )


@app.get("/api/v1/history/projects")
@limiter.limit("60/minute")
async def history_projects(request: Request):
    """List of all projects ever seen in the history, with first/last
    seen dates. Used to populate the per-project filter dropdown."""
    _ = request  # required by @limiter.limit
    return _cached_history_response(
        cache_key=("projects",),
        sql="""
            SELECT project                 AS name,
                   min(obs_date)           AS first_seen,
                   max(obs_date)           AS last_seen,
                   count(DISTINCT obs_date) AS days_observed
            FROM fact_stats
            WHERE stats_type = 'byProject' AND project IS NOT NULL
            GROUP BY project
            ORDER BY project
        """,
    )


@app.get("/api/v1/history/cpids")
@limiter.limit("30/minute")
async def history_cpids(request: Request):
    """List of all CPIDs ever seen in the history, with compact rollup
    fields. Used by the sand-chart page to populate a CPID autocomplete
    input. Sorted by cpid hex for stable browser datalist ordering."""
    _ = request  # required by @limiter.limit
    return _cached_history_response(
        cache_key=("cpids",),
        sql="""
            SELECT cpid,
                   first_seen,
                   last_seen,
                   days_active,
                   lifetime_mag_sum
            FROM summary_cpid_lifetime
            ORDER BY cpid
        """,
    )


@app.get("/api/v1/history/cpid-project-magnitude")
@limiter.limit("30/minute")
async def history_cpid_project_magnitude(request: Request, cpid: str):
    """Per-day magnitude breakdown of a single CPID across projects.
    Drives the stacked-area (sand) chart that visualises how a
    researcher's magnitude was distributed across projects over time.

    The `cpid` query parameter is validated to 32 lowercase hex chars to
    keep the cache key tight and eliminate an SQL-injection surface."""
    _ = request
    if not (len(cpid) == 32 and all(c in "0123456789abcdef" for c in cpid)):
        raise HTTPException(
            status_code=400,
            detail="cpid must be exactly 32 lowercase hex characters.",
        )
    return _cached_history_response(
        cache_key=("cpid-project-magnitude", cpid),
        sql="""
            SELECT obs_date, project, mag
            FROM fact_stats
            WHERE stats_type = 'byCPIDbyProject'
              AND cpid = ?
              AND project IS NOT NULL
            ORDER BY obs_date, project
        """,
        params=(cpid,),
    )


_TOP_CPIDS_PERIODS = {
    "all":     None,    # use the precomputed summary table
    "week":    7,
    "month":   30,
    "quarter": 90,
    "year":    365,
}

_TOP_CPIDS_ALLOWED_ORDERS = {
    "lifetime_mag_avg_active",
    "lifetime_mag_avg_elapsed",
    "lifetime_mag_sum",
    "lifetime_rac_max",
    "lifetime_tc_max",
    "days_active",
}


def _top_cpids_resolve_window(
    period: str | None,
    start_date: str | None,
    end_date: str | None,
) -> tuple[_dt.date, _dt.date, bool] | None:
    """Resolve the requested window, if any.

    Returns (start, end, is_canned) or None for "all time" (precomputed
    summary path). `is_canned` is True for period= queries (cacheable)
    and False for explicit start_date/end_date (not cacheable — the
    caller bypasses the in-process cache to bound cardinality).

    Raises HTTPException 400 on invalid combinations.
    """
    if start_date or end_date:
        if period and period != "all":
            raise HTTPException(
                status_code=400,
                detail="period and start_date/end_date are mutually exclusive",
            )
        if not (start_date and end_date):
            raise HTTPException(status_code=400, detail="start_date and end_date must both be provided")
        try:
            start = _dt.date.fromisoformat(start_date)
            end = _dt.date.fromisoformat(end_date)
        except ValueError as e:
            raise HTTPException(status_code=400, detail=f"invalid ISO date: {e}")
        if end < start:
            raise HTTPException(status_code=400, detail="end_date must be >= start_date")
        return (start, end, False)

    period = period or "all"
    if period not in _TOP_CPIDS_PERIODS:
        raise HTTPException(
            status_code=400,
            detail=f"period must be one of {sorted(_TOP_CPIDS_PERIODS)}",
        )
    days = _TOP_CPIDS_PERIODS[period]
    if days is None:
        return None  # all-time — use precomputed summary
    end = _dt.datetime.now(_dt.timezone.utc).date()
    start = end - _dt.timedelta(days=days - 1)  # inclusive on both ends
    return (start, end, True)


def _top_cpids_range_response(
    project: str | None,
    limit: int,
    order_by: str,
    start: _dt.date,
    end: _dt.date,
    is_canned: bool,
) -> Response:
    """Run the on-demand range aggregation against fact_stats and
    return a JSON response. Cached only for canned periods."""
    days_elapsed = (end - start).days + 1

    if project:
        cache_key = ("top-cpids-range-by-project", project, limit, order_by, start.isoformat(), end.isoformat())
        sql = f"""
            SELECT cpid,
                   ? AS project,
                   MIN(obs_date)                     AS first_seen,
                   MAX(obs_date)                     AS last_seen,
                   COUNT(*) FILTER (WHERE mag > 0)   AS days_active,
                   COUNT(*)                          AS days_observed,
                   ?                                 AS days_elapsed,
                   COALESCE(SUM(mag), 0)             AS lifetime_mag_sum,
                   AVG(mag) FILTER (WHERE mag > 0)   AS lifetime_mag_avg_active,
                   COALESCE(SUM(mag), 0) / ?         AS lifetime_mag_avg_elapsed,
                   MAX(rac)                          AS lifetime_rac_max,
                   MAX(tc)                           AS lifetime_tc_max
            FROM fact_stats
            WHERE stats_type = 'byCPIDbyProject'
              AND project = ?
              AND obs_date BETWEEN ? AND ?
            GROUP BY cpid
            ORDER BY {order_by} DESC NULLS LAST
            LIMIT ?
        """
        params = (project, days_elapsed, days_elapsed, project, start, end, limit)
    else:
        cache_key = ("top-cpids-range", limit, order_by, start.isoformat(), end.isoformat())
        sql = f"""
            SELECT cpid,
                   MIN(obs_date)                     AS first_seen,
                   MAX(obs_date)                     AS last_seen,
                   COUNT(*) FILTER (WHERE mag > 0)   AS days_active,
                   COUNT(*)                          AS days_observed,
                   ?                                 AS days_elapsed,
                   COALESCE(SUM(mag), 0)             AS lifetime_mag_sum,
                   AVG(mag) FILTER (WHERE mag > 0)   AS lifetime_mag_avg_active,
                   COALESCE(SUM(mag), 0) / ?         AS lifetime_mag_avg_elapsed,
                   MAX(rac)                          AS lifetime_rac_max,
                   MAX(tc)                           AS lifetime_tc_max
            FROM fact_stats
            WHERE stats_type = 'byCPID'
              AND obs_date BETWEEN ? AND ?
            GROUP BY cpid
            ORDER BY {order_by} DESC NULLS LAST
            LIMIT ?
        """
        params = (days_elapsed, days_elapsed, start, end, limit)

    if is_canned:
        return _cached_history_response(cache_key=cache_key, sql=sql, params=tuple(params))

    # Custom range: live query, no in-process cache, no public caching.
    data = _history_query(sql, tuple(params))
    body = json.dumps({"data": data}).encode("utf-8")
    return Response(
        content=body,
        media_type="application/json",
        headers={"Cache-Control": "no-store"},
    )


@app.get("/api/v1/history/top-cpids")
@limiter.limit("30/minute")
async def history_top_cpids(
    request: Request,
    project: str | None = None,
    limit: int = 100,
    order_by: str = "lifetime_mag_sum",
    period: str | None = None,
    start_date: str | None = None,
    end_date: str | None = None,
):
    """Top CPIDs ranked over a configurable date window.

    Default behavior (`period` omitted or `period=all`) reads the
    precomputed `summary_cpid_lifetime` / `summary_cpid_project_lifetime`
    tables — same data as before this endpoint grew range support.

    For any other `period` (week, month, quarter, year) the endpoint
    runs an on-demand aggregation against the `fact_stats` table over
    the resolved [today-N+1 .. today] UTC window. Canned periods are
    cached.

    Custom ranges via `start_date` and `end_date` (both ISO YYYY-MM-DD,
    inclusive) bypass the in-process cache and return
    `Cache-Control: no-store` to keep cache cardinality bounded.
    `period` and `start_date`/`end_date` are mutually exclusive.

    `order_by` must be one of the allowed column names (prevents SQL
    injection in the LIMIT clause).
    """
    _ = request  # required by @limiter.limit
    limit = max(1, min(500, limit))
    if order_by not in _TOP_CPIDS_ALLOWED_ORDERS:
        raise HTTPException(
            status_code=400,
            detail=f"order_by must be one of {sorted(_TOP_CPIDS_ALLOWED_ORDERS)}",
        )

    window = _top_cpids_resolve_window(period, start_date, end_date)
    if window is not None:
        start, end, is_canned = window
        return _top_cpids_range_response(project, limit, order_by, start, end, is_canned)

    # All-time — precomputed summary tables.
    if project:
        return _cached_history_response(
            cache_key=("top-cpids-by-project", project, limit, order_by),
            sql=f"""
                SELECT cpid, project, first_seen, last_seen,
                       days_active, days_observed, days_elapsed,
                       lifetime_mag_sum, lifetime_mag_avg_active,
                       lifetime_mag_avg_elapsed, lifetime_rac_max, lifetime_tc_max
                FROM summary_cpid_project_lifetime
                WHERE project = ?
                ORDER BY {order_by} DESC NULLS LAST
                LIMIT ?
            """,
            params=(project, limit),
        )
    return _cached_history_response(
        cache_key=("top-cpids", limit, order_by),
        sql=f"""
            SELECT cpid, first_seen, last_seen,
                   days_active, days_observed, days_elapsed,
                   lifetime_mag_sum, lifetime_mag_avg_active,
                   lifetime_mag_avg_elapsed, lifetime_rac_max, lifetime_tc_max
            FROM summary_cpid_lifetime
            ORDER BY {order_by} DESC NULLS LAST
            LIMIT ?
        """,
        params=(limit,),
    )


# ---------------------------------------------------------------------------
# GitHub releases (for chart-annotation overlay)
# ---------------------------------------------------------------------------
#
# Separate from the analytics DB history endpoints because the source is the
# GitHub API, not DuckDB. Cache body is keyed on a constant (no parameters);
# TTL is long because releases publish infrequently. On upstream error we
# serve stale cache if available, only 502 if we've never fetched.

_releases_cache: dict[str, tuple[float, bytes]] = {}
_releases_cache_lock = threading.Lock()


def _classify_release(tag: str, name: str) -> str:
    """Return 'mandatory', 'leisure', or 'unknown' based on tag/name suffix.

    Gridcoin release naming convention (verified against 102 releases back to
    2016): mandatory releases carry '-mandatory' in the tag or release name;
    casual releases carry '-leisure'. Anything else (older releases pre-dating
    the convention, oddly-named entries) falls through to 'unknown'.
    """
    combined = f"{tag or ''} {name or ''}".lower()
    if "mandatory" in combined:
        return "mandatory"
    if "leisure" in combined:
        return "leisure"
    return "unknown"


async def _fetch_releases_from_github() -> list[dict]:
    """Pull all releases from the GitHub repo with pagination, drop
    prereleases (testnet tags), and return the slim shape the front-end
    needs for chart annotations. Sorted oldest-first."""
    headers = {
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
    }
    if GITHUB_TOKEN:
        headers["Authorization"] = f"Bearer {GITHUB_TOKEN}"
    url = f"https://api.github.com/repos/{GITHUB_REPO}/releases"
    raw: list[dict] = []
    async with httpx.AsyncClient(timeout=10.0) as client:
        page = 1
        while True:
            resp = await client.get(url, headers=headers,
                                    params={"per_page": 100, "page": page})
            resp.raise_for_status()
            batch = resp.json()
            if not batch:
                break
            raw.extend(batch)
            if len(batch) < 100:
                break
            page += 1

    slim: list[dict] = []
    for rel in raw:
        if rel.get("prerelease"):
            continue
        tag = rel.get("tag_name", "") or ""
        name = rel.get("name", "") or ""
        slim.append({
            "tag": tag,
            "name": name,
            "published_at": rel.get("published_at"),
            "type": _classify_release(tag, name),
            "html_url": rel.get("html_url"),
        })
    slim.sort(key=lambda x: x.get("published_at") or "")
    return slim


@app.get("/api/v1/history/releases")
@limiter.limit("60/minute")
async def history_releases(request: Request):
    """Published (non-prerelease) Gridcoin-Research releases, annotated with
    mandatory/leisure/unknown type derived from the tag+name suffix. Used by
    the analytics page to overlay release markers on the historical charts."""
    _ = request  # required by @limiter.limit
    now = time.time()
    cache_key = "releases"

    with _releases_cache_lock:
        entry = _releases_cache.get(cache_key)
    if entry is not None:
        cached_at, body = entry
        if (now - cached_at) <= RELEASES_CACHE_TTL:
            return Response(
                content=body,
                media_type="application/json",
                headers={"Cache-Control": f"public, max-age={RELEASES_CACHE_TTL}"},
            )

    try:
        data = await _fetch_releases_from_github()
    except httpx.HTTPError as e:
        log.warning("Failed to fetch releases from GitHub: %s", e)
        # Serve stale cache rather than 502 if we have anything at all.
        with _releases_cache_lock:
            entry = _releases_cache.get(cache_key)
        if entry is not None:
            _, stale_body = entry
            return Response(
                content=stale_body,
                media_type="application/json",
                headers={"Cache-Control": "public, max-age=60"},
            )
        raise HTTPException(
            status_code=502,
            detail="Unable to fetch release metadata from GitHub.",
        ) from e

    body = json.dumps({"data": data}).encode("utf-8")
    with _releases_cache_lock:
        _releases_cache[cache_key] = (now, body)
    return Response(
        content=body,
        media_type="application/json",
        headers={"Cache-Control": f"public, max-age={RELEASES_CACHE_TTL}"},
    )


@app.get("/health")
async def health():
    """Health check endpoint."""
    has_cache = bool(_cache) and "_fetched_at" in _cache
    age = time.time() - _cache.get("_fetched_at", 0) if has_cache else None
    return {
        "status": "ok" if has_cache else "no_data",
        "cache_age_seconds": round(age) if age is not None else None,
        "cache_ttl": CACHE_TTL,
    }


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    import uvicorn
    # proxy_headers + forwarded_allow_ips make uvicorn rewrite the ASGI
    # scope's `client` tuple based on the trusted upstream's
    # X-Forwarded-For header, so `request.client.host` (and therefore
    # slowapi's per-IP rate-limit key) reflects the real client rather
    # than the loopback peer Caddy/nginx presents to us.
    uvicorn.run(
        "server:app",
        host=LISTEN_HOST,
        port=LISTEN_PORT,
        log_level="info",
        proxy_headers=True,
        forwarded_allow_ips=FORWARDED_ALLOW_IPS,
    )
