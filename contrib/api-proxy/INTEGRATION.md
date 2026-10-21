# Consuming the Gridcoin Network Status API

This is the client-facing companion to [`README.md`](README.md). The
README covers running and deploying the proxy; this document covers
consuming the public instance from outside the project — third-party
block explorers, dashboards, stats sites and bots.

Everything below is observable behavior of the deployed service, not
aspiration. Where a value is a server default it is named as such, so
an operator running their own instance can see what changes.

## Base URL

```
https://api.gridcoin.us
```

No API key, no registration, `GET` only. Interactive schema browsers
are public:

| Path | What |
|---|---|
| `/docs` | Swagger UI |
| `/redoc` | ReDoc |
| `/openapi.json` | OpenAPI 3.x schema |

The schema is the authoritative description of request and response
shapes. Prefer generating a client from it over hand-transcribing the
table below.

Quick check that you are talking to a live instance:

```bash
curl -s https://api.gridcoin.us/health
# {"status":"ok","cache_age_seconds":167,"cache_ttl":300}
```

## Endpoints

Twelve `GET` endpoints. `/api/v1/network-status` is the live whitelist
and superblock snapshot; everything under `/api/v1/history/` is a daily
time series derived from the analytics database.

| Path | Rate limit | Cached | Returns |
|---|---|---|---|
| `/health` | — | — | Cache freshness; no network data |
| `/api/v1/network-status` | 30/min | 5 min | Superblock summary + per-project whitelist snapshot |
| `/api/v1/history/project-churn` | 30/min | 1 h | Daily project count in superblock, plus adds and drops |
| `/api/v1/history/cpid-churn` | 30/min | 1 h | Daily active CPID count, plus in and out counts |
| `/api/v1/history/project-active-cpids` | 30/min | 1 h | Daily active CPID count per project |
| `/api/v1/history/cpids` | 30/min | 1 h | Every CPID ever observed (autocomplete source) |
| `/api/v1/history/cpid-project-magnitude` | 30/min | 1 h | Per-project daily magnitude for one CPID; `?cpid=` required |
| `/api/v1/history/mrc-daily` | 30/min | 1 h | Daily MRC activity: counts and gross/net/fee GRC splits |
| `/api/v1/history/top-cpids` | 30/min | 1 h | CPID leaderboard; see parameters below |
| `/api/v1/history/block-version-activations` | 30/min | 1 h | Empirical activation date per block version |
| `/api/v1/history/projects` | 60/min | 1 h | Distinct project names ever seen, for UI filters |
| `/api/v1/history/releases` | 60/min | 6 h | Release tags and dates, classified `mandatory` / `leisure` / `unknown` |

History series begin **2020-09-09** and extend to the current UTC day,
so the daily endpoints return roughly 2,180 rows today and grow by one
per day. `/api/v1/history/cpids` is the large one — on the order of
5,000 rows and several hundred KB.

`/api/v1/history/releases` is sourced from the GitHub releases API
rather than the chain, and is served stale on upstream failure rather
than erroring.

### `top-cpids` parameters

| Parameter | Default | Notes |
|---|---|---|
| `project` | _(all)_ | Restrict the leaderboard to one project |
| `limit` | `100` | Silently clamped to 1-500 — no error on out-of-range |
| `order_by` | `lifetime_mag_sum` | One of `days_active`, `lifetime_mag_avg_active`, `lifetime_mag_avg_elapsed`, `lifetime_mag_sum`, `lifetime_rac_max`, `lifetime_tc_max`. Anything else is a 400 whose body lists the valid set |
| `period` | `all` | `all`, `week`, `month`, `quarter`, `year`. Rolling UTC windows, cached |
| `start_date`, `end_date` | — | Inclusive ISO `YYYY-MM-DD`, both required together, mutually exclusive with `period` |

Custom `start_date`/`end_date` ranges bypass the server cache and come
back with `Cache-Control: no-store`, to keep cache cardinality bounded.
Use `period` for anything you poll repeatedly.

## Response envelopes

The two families differ, which is easy to miss:

- Everything under `/api/v1/history/` wraps its rows in an object:
  `{"data": [ … ]}`.
- `/api/v1/network-status` returns a **bare object** with
  `timestamp`, `superblock` and `projects` at the top level. There is
  no `data` wrapper.

`network-status` `projects` is an **object keyed by project name**, not
an array:

```json
{
  "timestamp": "2026-10-21T01:43:43Z",
  "superblock": { "height": "4114208", "date": "10-20-2026 10:17:52",
                  "total_cpids": 1249, "active_beacons": 1103,
                  "total_magnitude": 114987, "total_projects": 15 },
  "projects": {
    "asteroids@home": {
      "status": "Active by Greylist Override",
      "display_name": "asteroids@home",
      "display_url": "https://asteroidsathome.net/boinc/",
      "stats_url": "https://asteroidsathome.net/boinc/stats/",
      "gdpr_controls": false,
      "in_superblock": true,
      "rac": 4674274, "average_rac": 5571,
      "total_credit": 15970522602, "cpid_count": 839,
      "zcd": 1, "was": 0.9724,
      "meets_greylist_criteria": false
    }
  }
}
```

## Access and limits

**CORS is an explicit origin allowlist.** A request from an origin that
is not on the list still returns `200`, but with no
`Access-Control-Allow-Origin` header — so the browser discards the
response and your console shows a CORS error, not an HTTP error. A
preflight `OPTIONS` from a non-allowlisted origin returns `400`.

This means **you cannot call this API directly from a browser on your
own domain until your origin is added.** Two options:

1. **Fetch server-side** (recommended). Call the API from your backend,
   cache the result, and serve it to your own clients from your own
   origin. Nothing is needed from us, you are insulated from our
   downtime, and you control your own cache policy.
2. **Browser-direct.** Open an issue on
   `gridcoin-community/Gridcoin-Research` asking for your origins to be
   added to `CORS_ORIGINS`, listing each one exactly — scheme, host and
   port, e.g. `https://example.org` and `https://www.example.org` are
   two distinct origins.

**Rate limits** are per remote IP, per the table above, and reset on a
one-minute window. Exceeding one returns `429`. Note that the limit
keys on the client IP, so browser-direct traffic is limited per visitor
while server-side traffic is limited per server — another reason the
server-side pattern scales better for a busy site.

**Caching.** Responses carry `Cache-Control: public, max-age=N`.
On `network-status`, `N` is the *remaining* life of the server-side
cache entry, counting down toward zero from 300 — not a fixed 300. Honor
it rather than polling faster; the underlying data only changes when a
new superblock arrives. There are no `ETag` or `Last-Modified` headers,
so conditional requests are not available.

**Compression.** Responses over 1024 bytes are gzipped when the client
sends `Accept-Encoding: gzip`. Worth enabling for the larger history
endpoints.

### Error contract

| Status | Body | When |
|---|---|---|
| `400` | `{"detail": "…"}` | Invalid parameter. Body explains, and for `order_by` lists the valid set |
| `429` | `{"error": "Rate limit exceeded. Try again later."}` | Rate limit exceeded. No `Retry-After` header — back off on your own clock |
| `502` | `{"detail": "…"}` | `history/releases` only, when GitHub is unreachable and no stale copy is available |
| `503` | `{"detail": "Analytics database is temporarily unavailable."}` | The nightly analytics refresh holds a brief exclusive write lock. Retry |

Note the inconsistency: `429` uses an `error` key while the others use
FastAPI's `detail`. Handle both.

The `503` window is short but real. The analytics ingest chain runs at
`00:00 UTC`; a client polling through that window should retry rather
than treat it as an outage.

## Data notes

These are the things that cost an integrator a day each. None of them
are visible in the OpenAPI schema.

**`status` has six possible values, and the one you expect is missing.**
The field is a passthrough of the wallet's `listprojects` RPC. The full
set, from `ProjectEntry::StatusToString` in
[`src/gridcoin/project.cpp`](../../src/gridcoin/project.cpp), is
`Active`, `Active by Greylist Override`, `Automatically Greylisted`,
`Manually Greylisted`, `Unknown` and `Deleted`. `Deleted` is filtered
out by the proxy and never reaches you. The trap: **as of this writing
no project carries the bare status `Active`** — every whitelisted
project is either `Active by Greylist Override` or
`Automatically Greylisted`. Code that tests `status == "Active"` to
build a whitelist renders an empty list today. Treat both `Active` and
`Active by Greylist Override` as whitelisted, and handle the other
values rather than assuming they cannot appear.

**`projects` can legitimately outnumber `superblock.total_projects`.**
At the time of writing the object holds 16 entries while
`total_projects` reports 15. That is correct, not a bug: the whitelist
is the set of projects the network knows about, while `total_projects`
counts those contributing to the current superblock. Do not use one as
a check on the other.

**Fields are absent, not null, when `in_superblock` is `false`.**
A project not in the current superblock has no statistics to report, so
`rac`, `average_rac`, `total_credit` and `cpid_count` are **omitted
from the object entirely**. Today `SiDock@home` is in this state. Use
key-presence checks or defaults; `entry["rac"]` will raise.

**`meets_greylist_criteria` is a prediction, not the current state.**
It reports whether a project *currently satisfies the rule that would
greylist it*, evaluated per
[`src/gridcoin/project.h`](../../src/gridcoin/project.h): at least 7
superblocks from baseline, and either `zcd > 7` or `was < 0.1`. A
project can meet the criteria without yet being greylisted. `zcd` is
zero-credit days across the last 20 superblocks; `was` is a work
availability score over the last 40, computed from credit change across
the last 7.

**`superblock.height` is a string, its siblings are numbers.** In the
same object, `height` arrives quoted (`"4114208"`) while
`total_cpids`, `active_beacons`, `total_magnitude` and
`total_projects` are JSON numbers. Coerce `height` before comparing or
sorting on it; a strict schema validator will flag it.

**Two date formats are in play.** `superblock.date` is
`MM-DD-YYYY HH:MM:SS`, a legacy wallet format. Everywhere else —
`timestamp`, and `obs_date` in every history series — is ISO 8601.
Parsing `superblock.date` with a stock ISO parser fails.

**Churn series carry nulls at both edges.** In `project-churn` and
`cpid-churn`, the first row has a null "in" count and the last row a
null "out" count, because the deltas are computed against a neighbor
that does not exist. Charting code that sums or averages these without
filtering produces a wrong first and last data point.

**Pool CPIDs appear on the leaderboards untagged.** `top-cpids` ranks
individual CPIDs, and mining pools hold CPIDs like any other
participant — grcpool's `7d0d73fe026d66fd4ab8d5d8da32a611` currently
ranks third all-time. It represents thousands of people, not one
cruncher. If you present a leaderboard as individuals, filter or label
the known pool CPIDs; the list lives in
[`src/gridcoin/pool.cpp`](../../src/gridcoin/pool.cpp).

## Operational reality

Be realistic about what you are depending on. The public instance runs
on a single small AWS EC2 instance that also runs the mainnet and
testnet scrapers and the analytics database. There is no CDN in front
of it, no redundancy behind it and no SLA. It is maintained on a
best-effort basis by the Gridcoin developers.

That is the strongest argument for the server-side pattern: cache our
responses on your infrastructure and your site stays up when ours does
not.

## Stability

`/api/v1` is intended to be additive: new endpoints and new fields may
appear, but existing field names and types are not renamed or removed
in place. A genuinely breaking change would land under a new path
prefix. This is intent rather than a contractual guarantee — see
*Operational reality* above.

If you are running an integration worth notifying, say so on the issue
tracker and we will make an effort to give notice before anything
breaking ships.

## Reference implementation

The Gridcoin website consumes this API in the browser, and its client
code is a working reference for every endpoint here — pagination,
caching, error handling and chart shaping included. See
[`gridcoin-community/Gridcoin-Site`](https://github.com/gridcoin-community/Gridcoin-Site),
`assets/js/`:

| File | Covers |
|---|---|
| `analytics-api.js` | Base URL resolution and shared fetch helpers |
| `fetch-whitelist.js` | `network-status` → whitelist table |
| `fetch-top-cpids.js` | `history/top-cpids`, including the window parameters |
| `fetch-cpid-churn.js`, `fetch-project-churn.js` | Churn series, including the edge nulls |
| `fetch-project-active-cpids.js` | `history/project-active-cpids` |
| `fetch-mrc-daily.js` | `history/mrc-daily` |
| `chart-helpers.js` | Shared charting utilities |

The rendered result is at <https://gridcoin.us/guides/whitelist.htm>
and <https://gridcoin.us/guides/analytics.htm>.

## Problems, requests and attribution

Open an issue on
[`gridcoin-community/Gridcoin-Research`](https://github.com/gridcoin-community/Gridcoin-Research/issues)
for a bug, a CORS origin request, or a field or endpoint you need that
does not exist yet. Adding a field to an existing response is usually
easy.

Attribution is not required. A link back to <https://gridcoin.us> is
appreciated.
