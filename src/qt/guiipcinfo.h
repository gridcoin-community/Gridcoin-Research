// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_GUIIPCINFO_H
#define GRIDCOIN_QT_GUIIPCINFO_H

#include <QString>

//! Pre-formatted facts about the GUI<->node IPC connection, for the About
//! dialog's multiprocess section and the mixed-build banner. Populated in main()
//! from the connect handshake in the -multiprocess split build; in the monolith
//! build (GUI and node share one process) `active` is false and there is nothing
//! to show. Strings are display-ready values (not tr()-wrapped captions); the
//! dialog owns the labels. Doing the formatting in main() keeps the GUI widgets
//! free of any ipc/handshake dependency.
struct GuiIpcInfo
{
    bool active = false;               //!< Split (-multiprocess) build: show the section.
    bool git_commit_mismatch = false;  //!< GUI and node built from different commits (banner).
    QString gui_version;               //!< The GUI process's own version (FormatFullVersion).
    QString node_version;              //!< The connected daemon's version.
    QString node_built_at;             //!< The daemon's build timestamp.
    QString ipc_schema;                //!< Negotiated IPC schema, "major.minor".
    QString ipc_protocol;              //!< Negotiated IPC protocol version.
    QString socket_path;               //!< The AF_UNIX socket the GUI connected on.
    QString node_identity;             //!< The node's raw identity token; empty means unavailable
                                       //!< (the About dialog substitutes an "unavailable" label).
    QString network;                   //!< The node's chain network ("main"/"test"/...).
};

#endif // GRIDCOIN_QT_GUIIPCINFO_H
