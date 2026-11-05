# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Generates the GUI's desktop file, GRC_DESKTOP_FILE, from
# contrib/gridcoinresearch.desktop.in with the launcher actions this build can run.
# Attaching to a running core needs the multiprocess GUI and a daemon to attach to,
# and connecting to the system service also needs its units (INSTALL_SYSTEMD_UNITS).
# The system service's names come from cmake/systemd_units.cmake (GRC_NET_ARG,
# GRC_STATE_NAME, GRC_ACCOUNT).
#
# Included from src/qt/CMakeLists.txt. It also runs in script mode, as
# cmake -D<option>=<value>... -DGRC_DESKTOP_FILE=<path> -P cmake/desktop_file.cmake,
# which the tests in contrib/init use.

set(GRC_DESKTOP_ACTIONS "Testnet;")
# Each group starts with a blank line, after the Testnet group the template ends with.
set(GRC_DESKTOP_ACTION_GROUPS "")
if(ENABLE_MULTIPROCESS AND ENABLE_DAEMON)
    set(_grc_gui_args "-multiprocess")
    if(NOT "${GRC_NET_ARG}" STREQUAL "")
        set(_grc_gui_args "${GRC_NET_ARG} -multiprocess")
    endif()
    string(APPEND GRC_DESKTOP_ACTIONS "Multiprocess;")
    string(APPEND GRC_DESKTOP_ACTION_GROUPS
        "\n\n[Desktop Action Multiprocess]\n"
        "Name=Connect to your own core\n"
        "Exec=gridcoinresearch ${_grc_gui_args}")
    if(INSTALL_SYSTEMD_UNITS)
        string(APPEND GRC_DESKTOP_ACTIONS "SystemService;")
        string(APPEND GRC_DESKTOP_ACTION_GROUPS
            "\n\n[Desktop Action SystemService]\n"
            "Name=Connect to the system service\n"
            "Exec=gridcoinresearch ${_grc_gui_args} -ipcdir=/run/${GRC_STATE_NAME} -ipcserviceuser=${GRC_ACCOUNT}")
    endif()
endif()

configure_file("${CMAKE_CURRENT_LIST_DIR}/../contrib/gridcoinresearch.desktop.in" "${GRC_DESKTOP_FILE}" @ONLY)
