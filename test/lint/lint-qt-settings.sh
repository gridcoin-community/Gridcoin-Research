#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# The node owns gridcoinsettings.json: it reads and writes the file and announces
# every change. In -multiprocess mode the GUI does not open it at all, so the
# GUI's own ArgsManager holds only the command line and the config file. A node
# setting read through gArgs in GUI code therefore works in the monolithic build
# and silently reads a stale or default value in -multiprocess mode.
#
# Rule: in src/qt, gArgs may be read only for
#   - the options the GUI registers itself (SetupUIArgs in src/qt/bitcoin.cpp),
#     parsed from that function so registering a GUI option is all it takes, and
#   - the process start-up options below, which the GUI process needs to start.
# Every other setting is a node setting and is read through interfaces::Node
# (getSettingBool/getSettingInt/getSettingStr, isSettingSet).
#
# The scan sees reads that name the option as a string literal on the same line
# (gArgs.GetBoolArg("-x", ...)). A name built at run time, or a call split across
# lines, is left to code review.

export LC_ALL=C

cd "$(git rev-parse --show-toplevel)" || exit 1

PROCESS_ARGS="-conf -datadir -debuglogfile -multiprocess -resetblockchaindata -testnet -version -wallet"

# The options SetupUIArgs registers.
GUI_ARGS=$(awk '/^static void SetupUIArgs\(/,/^}/' src/qt/bitcoin.cpp \
    | grep -oE 'argsman\.AddArg\("-[a-z0-9]+' | sed -E 's/.*"//' | sort -u | tr '\n' ' ')

if [ -z "$GUI_ARGS" ]; then
    echo "$(basename "${BASH_SOURCE[0]}"): could not read the GUI options from SetupUIArgs in src/qt/bitcoin.cpp"
    exit 1
fi

# GUI_ONLY_ARGS in src/init.cpp must list the same options: the node registers them as hidden arguments and
# ChangeSettings refuses to store them. A GUI option missing there is unknown to the node.
NODE_LIST=$(awk '/GUI_ONLY_ARGS\{/,/\};/' src/init.cpp | grep -oE '"-[a-z0-9]+"' | tr -d '"' | sort -u | tr '\n' ' ')
if [ "$NODE_LIST" != "$GUI_ARGS" ]; then
    echo "GUI_ONLY_ARGS in src/init.cpp does not match the options SetupUIArgs registers in src/qt/bitcoin.cpp:"
    echo "  GUI_ONLY_ARGS: $NODE_LIST"
    echo "  SetupUIArgs:   $GUI_ARGS"
    exit 1
fi

ALLOWED=" $PROCESS_ARGS $GUI_ARGS "

READ_RE='gArgs\.(GetArg|GetBoolArg|GetIntArg|GetArgs|GetSetting|GetSettingsList|IsArgSet|IsArgNegated)\("-[a-z0-9]+'

VIOLATIONS=$(git ls-files 'src/qt/*.cpp' 'src/qt/*.h' | sort -u | while read -r f; do
    grep -nE "$READ_RE" "$f" 2>/dev/null \
        | grep -vE '^[0-9]+:[[:space:]]*(//|\*|/\*)' \
        | while IFS=: read -r line text; do
            # Drop a trailing comment so a read it mentions is not counted.
            code=$(echo "$text" | sed -E 's://.*$::')
            for name in $(echo "$code" | grep -oE "$READ_RE" | sed -E 's/.*\("//'); do
                case "$ALLOWED" in
                    *" $name "*) ;;
                    *) echo "$f:$line: $name" ;;
                esac
            done
        done
done | sort -u)

if [ -n "$VIOLATIONS" ]; then
    echo "Node setting(s) read through gArgs in src/qt (file:line: setting):"
    echo "$VIOLATIONS"
    echo
    echo "In -multiprocess mode the GUI has no copy of the node's settings. Read a node"
    echo "setting through interfaces::Node (getSettingBool/getSettingInt/getSettingStr,"
    echo "isSettingSet). An option the GUI owns must be registered in SetupUIArgs"
    echo "(src/qt/bitcoin.cpp); a process start-up option belongs in PROCESS_ARGS in"
    echo "$(basename "${BASH_SOURCE[0]}")."
    exit 1
fi

exit 0
