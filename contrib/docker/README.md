# Gridcoin Docker Images

Official Docker images for running Gridcoin as a headless daemon or with the Qt GUI.

## Images

| Image | Description |
|-------|-------------|
| `ghcr.io/gridcoin-community/gridcoinresearchd` | Headless daemon only |
| `ghcr.io/gridcoin-community/gridcoinresearch` | Daemon + Qt GUI |

Both images support `linux/amd64` and `linux/arm64`.

Both architectures use dynamically-linked binaries built with Ubuntu 24.04 system libraries.
The Docker container provides the hermetic runtime environment (consistent library versions)
instead of static linking.

## Quick Start

### Headless (Daemon)

```bash
docker pull ghcr.io/gridcoin-community/gridcoinresearchd:latest

docker run -d \
  --name gridcoin \
  -v ~/.GridcoinResearch:/home/gridcoin/.GridcoinResearch \
  -p 32749:32749 \
  ghcr.io/gridcoin-community/gridcoinresearchd:latest
```

The core's RPC port listens on loopback inside the container only. For RPC access from
the host, see [Exchange Deployment](#exchange-deployment).

### GUI

```bash
docker pull ghcr.io/gridcoin-community/gridcoinresearch:latest

# Allow X11 connections for the current local user only (run on host).
# Avoid broad commands like 'xhost +local:' which allow any local container
# to connect to your X server (keystroke capture, input injection).
xhost +SI:localuser:$(id -un)

docker run -d \
  --name gridcoin-gui \
  -e DISPLAY=$DISPLAY \
  -v /tmp/.X11-unix:/tmp/.X11-unix:ro \
  -v ~/.GridcoinResearch:/home/gridcoin/.GridcoinResearch \
  -p 32749:32749 \
  ghcr.io/gridcoin-community/gridcoinresearch:latest \
  gridcoinresearch
```

## Docker Compose

### Headless + BOINC

```bash
cd contrib/docker
cp .env.example .env
# Edit .env to set GRIDCOIN_DATA_DIR

docker compose up -d
```

This starts both the Gridcoin daemon and a BOINC client with a shared data volume,
allowing Gridcoin to read BOINC statistics for research reward eligibility.

### GUI + BOINC

```bash
cd contrib/docker
cp .env.example .env
# Edit .env to set GRIDCOIN_DATA_DIR and DISPLAY

docker compose -f docker-compose.gui.yml up -d
```

## Data Persistence

All blockchain data, wallet files, and configuration are stored in the data directory
mounted at `/home/gridcoin/.GridcoinResearch` inside the container. This is a standard
bind mount -- the same directory layout as a bare-metal installation.

**Back up your wallet:** The wallet file is at `<data-dir>/wallet.dat`. Back it up
regularly, especially before upgrading.

## Configuration

### Auto-Generated Config

On first run, if no `gridcoinresearch.conf` exists, the entrypoint creates one, mode
`0600`, with:
- `server=1` (RPC server enabled)
- `rpcuser=gridcoinrpc` and a random `rpcpassword` (generated via `openssl rand -hex 32`)
- `rpcbind=127.0.0.1` and `rpcallowip=127.0.0.1`: the RPC port listens on the container's
  loopback only, where nothing outside the container reaches it.
- With `GRC_RPC_ALLOW_SUBNET` set to one IPv4 subnet in CIDR form, such as
  `172.17.0.0/16`, instead `rpcbind=0.0.0.0`, `rpcallowip=127.0.0.1` and
  `rpcallowip=<that subnet>`: the port listens on every interface of the container, and
  only loopback and that subnet may connect. A value that is not such a subnet stops the
  container before anything is written.

On every start the entrypoint also sets `printtoconsole=1`, so the core's log goes to
`docker logs`.

The entrypoint writes the file only when there is none: `GRC_RPC_ALLOW_SUBNET` changes
nothing in an existing file, and the RPC lines of an existing file stay as they are. A
file written by an earlier image has `rpcallowip=` lines for `10.*.*.*`, `172.*.*.*` and
`192.168.*.*` and no `rpcbind=`. Without `rpcbind=`, any `rpcallowip=` line makes the
core listen on every interface, so replace those lines with the ones above.

RPC is plain HTTP: the password and every request cross the network unencrypted. Keep
the port on loopback or a Docker network, and tunnel it (SSH, for example) where it must
cross anything else.

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `GRIDCOIN_DATA_DIR` | `./.gridcoin` | Host path for persistent data |
| `GRC_RPC_ALLOW_SUBNET` | (none) | One IPv4 subnet in CIDR form that may reach the RPC port, written into a new configuration file (see above) |
| `P2P_PORT` | `32749` | Host P2P port mapping |
| `RPC_PORT` | `15715` | Host RPC port mapping |
| `DISPLAY` | (none) | X11 display for GUI |

### Custom Config

Edit `<data-dir>/gridcoinresearch.conf` directly, or pass arguments to the daemon:

```bash
docker run -d \
  --name gridcoin \
  -v ~/.GridcoinResearch:/home/gridcoin/.GridcoinResearch \
  ghcr.io/gridcoin-community/gridcoinresearchd:latest \
  -testnet -debug=net
```

## Exchange Deployment

For exchange integration, publish the RPC port on the host's loopback, and let the
Docker network's subnet reach the core when the configuration file is first created:

```bash
# The subnet of the network the container joins (here the default bridge)
docker network inspect bridge --format '{{(index .IPAM.Config 0).Subnet}}'

docker run -d \
  --name gridcoin \
  -v /data/gridcoin:/home/gridcoin/.GridcoinResearch \
  -e GRC_RPC_ALLOW_SUBNET=172.17.0.0/16 \
  -p 32749:32749 \
  -p 127.0.0.1:15715:15715 \
  ghcr.io/gridcoin-community/gridcoinresearchd:latest
```

A connection to the published port arrives at the container's address on the Docker
network, from that network (its gateway), not on the container's loopback: the core must
listen on every interface and allow that subnet, which is what the variable sets up.
Publishing on `127.0.0.1` keeps the port off the host's other interfaces.

The auto-generated config enables `server=1` with a random password. It is in
`gridcoinresearch.conf` in the data directory you mounted (`/data/gridcoin` above); copy it
into your RPC client's configuration from there.

Key RPC methods for exchanges:
- `getnewaddress` -- generate deposit addresses
- `sendtoaddress` -- process withdrawals
- `listtransactions` / `listsinceblock` -- track deposits
- `getblockchaininfo` -- monitor sync status
- `walletnotify` / `blocknotify` -- real-time transaction/block notifications

Transaction index is always enabled (no `-txindex` flag needed).

**Note:** Gridcoin uses plaintext `rpcuser`/`rpcpassword` (hashed `rpcauth` is not
supported), and RPC is plain HTTP. Keep the port on loopback and the Docker network as
above, and tunnel it where it must cross another network.

## GUI Display Server Notes

The GUI image includes the Qt XCB (X11) platform plugin.

- **X11 sessions:** Work directly with the X11 socket mount shown above
- **Wayland sessions:** Work via XWayland, which is enabled by default on GNOME, KDE
  Plasma, Sway, and other major Wayland compositors
- **Pure Wayland (no XWayland):** Not supported

## Building Locally

Build dynamically-linked binaries with system libraries, then create Docker images:

```bash
# 1. Install build dependencies (Ubuntu 24.04)
source ./install_dependencies.sh
install_deps native true true

# 2. Build
cmake -B build -DENABLE_GUI=ON -DUSE_QT6=ON -DENABLE_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 3. Stage binaries
cd contrib/docker
mkdir -p bin/amd64
cp ../../build/bin/gridcoinresearchd bin/amd64/
cp ../../build/bin/gridcoinresearch bin/amd64/

# 4. Build images, with your own UID and GID for the gridcoin user
docker build --build-arg UID=$(id -u) --build-arg GID=$(id -g) \
  -f Dockerfile.headless -t gridcoin-headless .
docker build --build-arg UID=$(id -u) --build-arg GID=$(id -g) \
  -f Dockerfile.gui -t gridcoin-gui .
```

## Ports

| Port | Protocol | Network | Description |
|------|----------|---------|-------------|
| 32749 | TCP | Mainnet | P2P |
| 15715 | TCP | Mainnet | RPC |
| 32748 | TCP | Testnet | P2P |
| 25715 | TCP | Testnet | RPC |

## Security Model

The images follow defense-in-depth principles:

- **Non-root execution:** The daemon runs as user `gridcoin` (UID/GID 1000) via `gosu`
- **Read-only filesystem:** `read_only: true` in docker-compose (writable data dir via volume)
- **Dropped capabilities:** `cap_drop: ALL` then `cap_add` restores only CHOWN,
  DAC_OVERRIDE, FOWNER, SETUID, SETGID (minimum for entrypoint bootstrap and `gosu`)
- **No privilege escalation:** `no-new-privileges` security option
- **Private tmp:** `tmpfs` mount for `/tmp`
- **Minimal base image:** `ubuntu:24.04` with only runtime dependencies installed
  (GUI adds Qt6 and X11 libraries)
- **Config permissions:** `gridcoinresearch.conf` created with mode `0600`
- **RPC on loopback:** a new configuration file has the RPC port listen on the
  container's loopback only, unless `GRC_RPC_ALLOW_SUBNET` names one subnet that may
  connect

## Troubleshooting

### Container won't start

Check logs:
```bash
docker logs gridcoin
```

### GUI shows "could not connect to display"

1. Ensure `DISPLAY` is set: `echo $DISPLAY`
2. Allow X11 access for the current user: `xhost +SI:localuser:$(id -un)`
3. Verify X11 socket exists: `ls /tmp/.X11-unix/`

### Sync is slow

This is normal for initial blockchain download. Monitor progress:
```bash
docker exec gridcoin gridcoinresearchd getblockchaininfo
```

### Permission denied on data directory

The published images run the daemon as UID/GID 1000, and at every start the entrypoint
gives the data directory to that user. If your host account has another UID or GID, build
the image with yours (see [Building Locally](#building-locally)), so that the files in
the data directory stay yours:
```bash
docker build --build-arg UID=$(id -u) --build-arg GID=$(id -g) \
  -f Dockerfile.headless -t gridcoin-headless .
```
