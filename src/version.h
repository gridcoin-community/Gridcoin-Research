// Copyright (c) 2012 The Bitcoin developers
// Copyright (c) 2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_VERSION_H
#define BITCOIN_VERSION_H

// network protocol versioning
//
//! The current protocol version
static const int PROTOCOL_VERSION = 180330;

//! Minimum protocol version required to gate PSGT pool relay (MSG_PSGT, #2910).
//! The relay itself lands in a later PR of the Phase II chain; peers below this
//! version will never be sent PSGT inventory once it does. This is a FIXED
//! marker equal to the PROTOCOL_VERSION of the release that adds PSGT relay
//! (180330, the block v15 minimum) and must NOT be changed to track future
//! PROTOCOL_VERSION bumps -- a 180330 peer still supports PSGT relay regardless
//! of later protocol versions. Hence the literal rather than an alias.
static const int PSGT_PROTO_VERSION = 180330;

//! Minimum protocol version required once the block v14 hard fork grace
//! period has elapsed. Kept when PROTOCOL_VERSION moved on to 180330 (v15)
//! so the staged v14 transition still disconnects pre-v14 peers without
//! cutting off 180329 (v14-capable) peers.
static const int V14_MIN_PROTO_VERSION = 180329;

//! Note that there may be special logic implemented for
//! a hard fork that actually disconnects nodes less than
//! the version its fork height requires after a grace period above that
//! height. This is activated by setting the
//! DISCONNECT_OLD_VERSION_AFTER_GRACE_PERIOD to true.
static const bool DISCONNECT_OLD_VERSION_AFTER_GRACE_PERIOD = true;

//! The disconnect grace period is now per-network in Consensus::Params::ProtocolVersionGracePeriod.

//! Disconnect from peers older than this proto version. This is absolute.
static const int MIN_PEER_PROTO_VERSION = 180327;

//! initial proto version, to be increased after version/verack negotiation.
static const int INIT_PROTO_VERSION = 180275;

// database format versioning
//
//! The current database version
static const int DATABASE_VERSION = 180015;

#endif
