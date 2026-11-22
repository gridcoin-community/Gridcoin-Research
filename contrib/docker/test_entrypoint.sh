#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Tests of what entrypoint.sh writes into a new configuration file, without a container.
# The entrypoint is sourced, which defines its functions without starting it; main runs
# in a subshell on a temporary data directory, with chown stubbed out and "true" as the
# command it ends by running.

export LC_ALL=C

DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=contrib/docker/entrypoint.sh
. "${DIR}/entrypoint.sh"

failures=0
fail() {
    echo "FAIL: $*"
    failures=$((failures + 1))
}

for subnet in 172.17.0.0/16 10.0.0.0/8 192.168.1.0/24 1.2.3.4/32 0.0.0.0/0 255.255.255.255/32; do
    valid_ipv4_cidr "${subnet}" || fail "refused ${subnet}"
done

for subnet in "" 172.17.0.0 172.17.0.0/ 172.17.0.0/33 256.0.0.0/8 1.2.3/24 1.2.3.4.5/8 01.2.3.4/8 \
              1.2.3.4/08 "172.17.0.0/16 " " 172.17.0.0/16" $'172.17.0.0/16\nrpcallowip=0.0.0.0/0' \
              fe80::/10 "10.*.*.*" 172.17.0.0/-1 1.2.3.4/8/8 localhost/8; do
    if valid_ipv4_cidr "${subnet}"; then
        fail "accepted $(printf '%q' "${subnet}")"
    fi
done

if ! command -v openssl > /dev/null; then
    echo "Skipping the configuration file tests since openssl is not installed."
    if [ "${failures}" -ne 0 ]; then
        exit 1
    fi
    exit 0
fi

TMP=$(mktemp -d)
trap 'rm -rf "${TMP}"' EXIT

# The lines of file $1 that set key $2, one per line.
values() {
    grep "^$2=" "$1"
}

# Run main on a fresh data directory $1, with GRC_RPC_ALLOW_SUBNET=$2 when $2 is not "-".
# Prints the exit status.
run_main() {
    (
        DATA_DIR=$1
        CONF_FILE="$1/gridcoinresearch.conf"
        chown() { :; }
        if [ "$2" != "-" ]; then
            export GRC_RPC_ALLOW_SUBNET=$2
        else
            unset GRC_RPC_ALLOW_SUBNET
        fi
        main true > /dev/null 2>&1
    )
    echo $?
}

# Loopback only.
conf="${TMP}/loopback.conf"
write_default_conf "${conf}" "" || fail "write_default_conf failed without a subnet"
[ "$(values "${conf}" rpcbind)" = "rpcbind=127.0.0.1" ] || fail "loopback: rpcbind is not just 127.0.0.1"
[ "$(values "${conf}" rpcallowip)" = "rpcallowip=127.0.0.1" ] || fail "loopback: rpcallowip is not just 127.0.0.1"
values "${conf}" rpcpassword | grep -qE '^rpcpassword=[0-9a-f]{64}$' || fail "loopback: no generated rpcpassword"
[ "$(stat -c %a "${conf}")" = 600 ] || fail "loopback: the file is not mode 0600"

# One subnet.
conf="${TMP}/subnet.conf"
write_default_conf "${conf}" 172.17.0.0/16 || fail "write_default_conf failed with a subnet"
[ "$(values "${conf}" rpcbind)" = "rpcbind=0.0.0.0" ] || fail "subnet: rpcbind is not just 0.0.0.0"
[ "$(values "${conf}" rpcallowip)" = $'rpcallowip=127.0.0.1\nrpcallowip=172.17.0.0/16' ] ||
    fail "subnet: rpcallowip is not 127.0.0.1 and the subnet"
[ "$(stat -c %a "${conf}")" = 600 ] || fail "subnet: the file is not mode 0600"

# main refuses a value that is not a subnet, and creates nothing.
mkdir "${TMP}/bad"
[ "$(run_main "${TMP}/bad" "10.*.*.*")" = 1 ] || fail "main: a bad GRC_RPC_ALLOW_SUBNET did not exit 1"
[ ! -e "${TMP}/bad/gridcoinresearch.conf" ] || fail "main: a bad GRC_RPC_ALLOW_SUBNET created the file"

# main writes the subnet into a new file.
mkdir "${TMP}/new"
[ "$(run_main "${TMP}/new" 10.1.0.0/24)" = 0 ] || fail "main: a good GRC_RPC_ALLOW_SUBNET did not exit 0"
values "${TMP}/new/gridcoinresearch.conf" rpcallowip | grep -qx "rpcallowip=10.1.0.0/24" ||
    fail "main: the subnet is not in the new file"
values "${TMP}/new/gridcoinresearch.conf" printtoconsole | grep -qx "printtoconsole=1" ||
    fail "main: printtoconsole=1 is not in the new file"

# main leaves an existing file's RPC lines as they are.
mkdir "${TMP}/old"
printf 'rpcuser=u\nrpcpassword=p\n' > "${TMP}/old/gridcoinresearch.conf"
[ "$(run_main "${TMP}/old" 10.1.0.0/24)" = 0 ] || fail "main: an existing file did not exit 0"
[ -z "$(values "${TMP}/old/gridcoinresearch.conf" rpcallowip)" ] || fail "main: an existing file was given rpcallowip"
[ -z "$(values "${TMP}/old/gridcoinresearch.conf" rpcbind)" ] || fail "main: an existing file was given rpcbind"

if [ "${failures}" -ne 0 ]; then
    echo "${failures} entrypoint test(s) failed"
    exit 1
fi
echo "entrypoint tests passed"
