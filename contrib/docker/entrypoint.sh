#!/usr/bin/env bash

export LC_ALL=C

DATA_DIR="/home/gridcoin/.GridcoinResearch"
CONF_FILE="${DATA_DIR}/gridcoinresearch.conf"

# Whether $1 is one IPv4 subnet in CIDR form: four decimal octets of 0 to 255 without
# leading zeros, and a prefix length of 0 to 32. Nothing else, so the value goes into
# the configuration file as one line.
valid_ipv4_cidr() {
    local value=$1 parts part
    [[ $value =~ ^([0-9]{1,3})\.([0-9]{1,3})\.([0-9]{1,3})\.([0-9]{1,3})/([0-9]{1,2})$ ]] || return 1
    parts=("${BASH_REMATCH[@]:1:5}")
    for part in "${parts[@]:0:4}"; do
        [[ $part =~ ^(0|[1-9][0-9]*)$ ]] && ((part <= 255)) || return 1
    done
    [[ ${parts[4]} =~ ^(0|[1-9][0-9]?)$ ]] && ((parts[4] <= 32))
}

# Write the configuration of a first run to $1: generated RPC credentials, and an RPC
# listener on loopback only. With a subnet in $2 (checked by valid_ipv4_cidr) the
# listener is on every interface of the container instead, and that subnet may connect
# besides loopback. The file is owner-only from the start.
write_default_conf() {
    local conf=$1 subnet=$2 password
    password=$(openssl rand -hex 32)
    if [ ${#password} -ne 64 ]; then
        echo "could not generate an RPC password" >&2
        return 1
    fi
    (
        umask 077
        {
            echo "server=1"
            echo "rpcuser=gridcoinrpc"
            echo "rpcpassword=${password}"
            echo "# RPC is plain HTTP: the password and everything else cross the network"
            echo "# unencrypted. Loopback may always connect."
            echo "rpcallowip=127.0.0.1"
            if [ -n "${subnet}" ]; then
                echo "# GRC_RPC_ALLOW_SUBNET was set when this file was written: listen on"
                echo "# every interface of the container, and let that subnet connect."
                echo "rpcbind=0.0.0.0"
                echo "rpcallowip=${subnet}"
            else
                echo "# Listen on loopback only. To let a Docker network in, replace this line"
                echo "# with rpcbind=0.0.0.0 and add rpcallowip=<that network's subnet>."
                echo "rpcbind=127.0.0.1"
            fi
        } > "${conf}"
    )
}

main() {
    set -e

    # Ensure the data directory exists and is owned by gridcoin
    mkdir -p "${DATA_DIR}"
    chown -R gridcoin:gridcoin "${DATA_DIR}"

    # Bootstrap config if it doesn't exist
    if [ ! -f "${CONF_FILE}" ]; then
        local subnet=${GRC_RPC_ALLOW_SUBNET:-}
        if [ -n "${subnet}" ] && ! valid_ipv4_cidr "${subnet}"; then
            echo "GRC_RPC_ALLOW_SUBNET must be one IPv4 subnet in CIDR form, such as 172.17.0.0/16;" \
                 "${CONF_FILE} was not created" >&2
            exit 1
        fi
        echo "Creating default gridcoinresearch.conf..."
        write_default_conf "${CONF_FILE}" "${subnet}"
        chown gridcoin:gridcoin "${CONF_FILE}"
    elif [ -n "${GRC_RPC_ALLOW_SUBNET:-}" ]; then
        echo "GRC_RPC_ALLOW_SUBNET applies only when the configuration file is created;" \
             "${CONF_FILE} exists and was not changed" >&2
    fi

    # Force printtoconsole=1 for Docker log visibility
    if grep -q '^printtoconsole=' "${CONF_FILE}"; then
        sed -i 's/^printtoconsole=.*/printtoconsole=1/' "${CONF_FILE}"
    else
        echo "printtoconsole=1" >> "${CONF_FILE}"
    fi

    # If first arg starts with '-', prepend the daemon binary
    if [ "${1:0:1}" = '-' ]; then
        set -- gridcoinresearchd "$@"
    fi

    # Drop privileges for Gridcoin binaries
    if [ "$1" = 'gridcoinresearchd' ] || [ "$1" = 'gridcoinresearch' ]; then
        exec gosu gridcoin:gridcoin "$@"
    fi

    # Allow arbitrary commands (e.g. bash for debugging)
    exec "$@"
}

# contrib/docker/test_entrypoint.sh sources this file to test the functions above. Run
# as the image's entrypoint, it starts.
if [ "${BASH_SOURCE[0]}" = "$0" ]; then
    main "$@"
fi
