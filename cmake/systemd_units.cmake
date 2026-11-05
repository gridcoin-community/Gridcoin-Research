# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Generates the systemd units, the sysusers.d entry and the tmpfiles.d entries from
# the templates in contrib/init for one network, and installs them with the
# autounlock helper and gridcoin-service. Included from the daemon install block in
# src/CMakeLists.txt on every UNIX but macOS, after GNUInstallDirs; it installs
# nothing of systemd's except on Linux.
#
# The templates name the network only through the GRC_* variables set below, so
# a mainnet and a testnet package come from the same files.

set(GRC_UNIT_NETWORK "main" CACHE STRING "Network the generated systemd units serve (main or test)")
set_property(CACHE GRC_UNIT_NETWORK PROPERTY STRINGS main test)
# systemd is Linux only, so the default is ON there and OFF elsewhere (the BSDs).
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_grc_systemd_default ON)
else()
    set(_grc_systemd_default OFF)
endif()
option(INSTALL_SYSTEMD_UNITS "Install the systemd units, sysusers.d and tmpfiles.d entries (Linux only)"
       ${_grc_systemd_default})
if(INSTALL_SYSTEMD_UNITS AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(WARNING "INSTALL_SYSTEMD_UNITS is ignored on ${CMAKE_SYSTEM_NAME}: systemd is Linux only")
    set(INSTALL_SYSTEMD_UNITS OFF)
endif()
option(INSTALL_SYSTEMD_AUTOUNLOCK "Install the autounlock helper and its units (systemd 251 or later)" ON)
# gridcoin-service is told whether the autounlock units are installed, so that it
# never operates on units this build left out.
if(INSTALL_SYSTEMD_AUTOUNLOCK)
    set(GRC_AUTOUNLOCK_UNITS "yes")
else()
    set(GRC_AUTOUNLOCK_UNITS "no")
endif()
# The helper's account is created only where the helper units are installed: the
# sysusers.d template puts this in front of the account's line.
if(INSTALL_SYSTEMD_AUTOUNLOCK)
    set(GRC_AUTOUNLOCK_SYSUSERS_OFF "")
else()
    set(GRC_AUTOUNLOCK_SYSUSERS_OFF "#")
endif()
set(GRC_SYSTEMD_UNITDIR "lib/systemd/system" CACHE STRING "Unit directory, relative to CMAKE_INSTALL_PREFIX")
set(GRC_SYSUSERSDIR "lib/sysusers.d" CACHE STRING "sysusers.d directory, relative to CMAKE_INSTALL_PREFIX")
set(GRC_TMPFILESDIR "lib/tmpfiles.d" CACHE STRING "tmpfiles.d directory, relative to CMAKE_INSTALL_PREFIX")
set(GRC_UNIT_PYTHON3 "/usr/bin/python3" CACHE STRING "Python interpreter the helper unit runs")

# The helper's directory under libexec, for each network. gridcoin-service takes
# both from here, since it serves either network.
set(GRC_LIBEXEC_SUBDIR_MAIN "gridcoin")
set(GRC_LIBEXEC_SUBDIR_TEST "gridcointestnet")

# The data directory under share for each network, and the Refuse class pattern
# file installed there (refused-setting-keys.txt). gridcoin-service takes both
# networks' paths from here. Each network has its own directory, named like its
# helper's under libexec, so that the two networks' packages, which can be
# installed together, do not ship the same file.
set(GRC_SHARE_SUBDIR_MAIN "gridcoin")
set(GRC_SHARE_SUBDIR_TEST "gridcointestnet")
set(GRC_REFUSED_KEYS_FILE_MAIN "${CMAKE_INSTALL_FULL_DATADIR}/${GRC_SHARE_SUBDIR_MAIN}/refused-setting-keys.txt")
set(GRC_REFUSED_KEYS_FILE_TEST "${CMAKE_INSTALL_FULL_DATADIR}/${GRC_SHARE_SUBDIR_TEST}/refused-setting-keys.txt")

if(GRC_UNIT_NETWORK STREQUAL "main")
    set(GRC_DAEMON "gridcoinresearchd")
    set(GRC_NET_ARG "")
    set(GRC_ACCOUNT "gridcoin")
    set(GRC_ATTACH_GROUP "gridcoin-gui")
    set(GRC_CONF_DIR "/etc/gridcoin")
    set(GRC_STATE_NAME "gridcoinresearchd")
    set(GRC_AUTOUNLOCK "gridcoin-autounlock")
    set(GRC_CRED_PREFIX "gridcoinresearch")
    set(GRC_LIBEXEC_SUBDIR "${GRC_LIBEXEC_SUBDIR_MAIN}")
    set(GRC_SHARE_SUBDIR "${GRC_SHARE_SUBDIR_MAIN}")
    set(GRC_SERVICE_TOOL "gridcoin-service")
elseif(GRC_UNIT_NETWORK STREQUAL "test")
    set(GRC_DAEMON "gridcointestnetd")
    set(GRC_NET_ARG "-testnet")
    set(GRC_ACCOUNT "gridcointestnet")
    set(GRC_ATTACH_GROUP "gridcointestnet-gui")
    set(GRC_CONF_DIR "/etc/gridcoin-testnet")
    set(GRC_STATE_NAME "gridcointestnetd")
    set(GRC_AUTOUNLOCK "gridcointestnet-autounlock")
    set(GRC_CRED_PREFIX "gridcointestnet")
    set(GRC_LIBEXEC_SUBDIR "${GRC_LIBEXEC_SUBDIR_TEST}")
    set(GRC_SHARE_SUBDIR "${GRC_SHARE_SUBDIR_TEST}")
    set(GRC_SERVICE_TOOL "gridcointestnet-service")
else()
    message(FATAL_ERROR "GRC_UNIT_NETWORK must be main or test, not '${GRC_UNIT_NETWORK}'")
endif()

set(GRC_BINDIR "${CMAKE_INSTALL_FULL_BINDIR}")
set(GRC_SBINDIR "${CMAKE_INSTALL_FULL_SBINDIR}")
set(GRC_LIBEXECDIR "${CMAKE_INSTALL_FULL_LIBEXECDIR}")

set(GRC_UNIT_TEMPLATE_DIR "${CMAKE_SOURCE_DIR}/contrib/init")
set(GRC_UNIT_OUTPUT_DIR "${CMAKE_BINARY_DIR}/contrib/init")

if(INSTALL_SYSTEMD_UNITS)
    # Template name and generated name, in one list: name pairs are separated by '|'
    set(_grc_units
        "gridcoinresearchd.service.in|${GRC_DAEMON}.service"
        "gridcoinresearchd@.service.in|${GRC_DAEMON}@.service"
    )
    set(_grc_autounlock_units
        "gridcoin-autounlock.socket.in|${GRC_AUTOUNLOCK}.socket"
        "gridcoin-autounlock.service.in|${GRC_AUTOUNLOCK}.service"
        "gridcoin-autounlock@.socket.in|${GRC_AUTOUNLOCK}@.socket"
        "gridcoin-autounlock@.service.in|${GRC_AUTOUNLOCK}@.service"
    )
    if(INSTALL_SYSTEMD_AUTOUNLOCK)
        list(APPEND _grc_units ${_grc_autounlock_units})
    endif()

    foreach(_pair IN LISTS _grc_units)
        string(REPLACE "|" ";" _pair "${_pair}")
        list(GET _pair 0 _template)
        list(GET _pair 1 _generated)
        configure_file("${GRC_UNIT_TEMPLATE_DIR}/${_template}" "${GRC_UNIT_OUTPUT_DIR}/${_generated}" @ONLY)
        install(FILES "${GRC_UNIT_OUTPUT_DIR}/${_generated}" COMPONENT daemon
            DESTINATION "${GRC_SYSTEMD_UNITDIR}"
        )
    endforeach()

    configure_file("${GRC_UNIT_TEMPLATE_DIR}/gridcoin.sysusers.in" "${GRC_UNIT_OUTPUT_DIR}/sysusers.conf" @ONLY)
    install(FILES "${GRC_UNIT_OUTPUT_DIR}/sysusers.conf" COMPONENT daemon
        DESTINATION "${GRC_SYSUSERSDIR}"
        RENAME "${GRC_DAEMON}.conf"
    )
    configure_file("${GRC_UNIT_TEMPLATE_DIR}/gridcoin.tmpfiles.in" "${GRC_UNIT_OUTPUT_DIR}/tmpfiles.conf" @ONLY)
    install(FILES "${GRC_UNIT_OUTPUT_DIR}/tmpfiles.conf" COMPONENT daemon
        DESTINATION "${GRC_TMPFILESDIR}"
        RENAME "${GRC_DAEMON}.conf"
    )
    # The service tool's state directory, shared by both networks: installed under
    # one name, so a host with both networks declares it once.
    install(FILES "${GRC_UNIT_TEMPLATE_DIR}/gridcoin-service.tmpfiles" COMPONENT daemon
        DESTINATION "${GRC_TMPFILESDIR}"
        RENAME "gridcoin-service.conf"
    )
endif()

# The helper is run by the units above through the interpreter; install(PROGRAMS)
# also sets the executable bit for hand runs.
if(INSTALL_SYSTEMD_UNITS AND INSTALL_SYSTEMD_AUTOUNLOCK)
    install(PROGRAMS "${GRC_UNIT_TEMPLATE_DIR}/gridcoin_autounlock_helper.py" COMPONENT daemon
        DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}/${GRC_LIBEXEC_SUBDIR}"
        RENAME "gridcoin-autounlock-helper"
    )
endif()

# The per-user service setup tool, generated for this network. It compares the unit
# systemd loads with the one installed here, so it takes the unit directory made
# absolute, as install() makes a relative DESTINATION.
if(IS_ABSOLUTE "${GRC_SYSTEMD_UNITDIR}")
    set(GRC_SYSTEMD_UNITDIR_FULL "${GRC_SYSTEMD_UNITDIR}")
else()
    set(GRC_SYSTEMD_UNITDIR_FULL "${CMAKE_INSTALL_PREFIX}/${GRC_SYSTEMD_UNITDIR}")
endif()
if(INSTALL_SYSTEMD_UNITS)
    configure_file("${GRC_UNIT_TEMPLATE_DIR}/gridcoin_service.py.in" "${GRC_UNIT_OUTPUT_DIR}/${GRC_SERVICE_TOOL}" @ONLY)
    install(PROGRAMS "${GRC_UNIT_OUTPUT_DIR}/${GRC_SERVICE_TOOL}" COMPONENT daemon
        DESTINATION "${CMAKE_INSTALL_SBINDIR}"
    )
endif()
