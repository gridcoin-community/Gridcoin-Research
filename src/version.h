// Copyright (c) 2012 The Bitcoin developers
// Copyright (c) 2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_VERSION_H
#define BITCOIN_VERSION_H

// network protocol versioning
//
//! The current protocol version
static const int PROTOCOL_VERSION = 180327;

//! Note that there may be special logic implemented for
//! a hard fork that actually disconnects nodes less than
//! PROTOCOL_VERSION after grace period above the hard
//! fork height. This is activated by setting the
//! DISCONNECT_OLD_VERSION_AFTER_GRACE_PERIOD to true.
static const bool DISCONNECT_OLD_VERSION_AFTER_GRACE_PERIOD = true;

//! This is the number of blocks for the disconnect grace period if
//! DISCONNECT_OLD_VERSION_AFTER_GRACE_PERIOD is true. If it is false, this
//! is inoperative.
static const int DISCONNECT_GRACE_PERIOD = 900 * 7;

//! Disconnect from peers older than this proto version. This is absolute.
static const int MIN_PEER_PROTO_VERSION = 180326;

//! initial proto version, to be increased after version/verack negotiation.
static const int INIT_PROTO_VERSION = 180275;

// database format versioning
//
//! The current database version
static const int DATABASE_VERSION = 180015;

#endif
