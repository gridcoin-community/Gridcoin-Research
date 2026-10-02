// Copyright (c) 2012 The Bitcoin developers
// Copyright (c) 2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_VERSION_H
#define BITCOIN_VERSION_H

// network protocol versioning
//
//! The current protocol version. It is bumped once per hard fork, in the release
//! that carries that fork, and the post-grace disconnect in net_processing.cpp
//! (the VERSION handler) must then be pointed at that fork's activation height.
static const int PROTOCOL_VERSION = 180330;

//! Minimum protocol version required to gate PSGT pool relay (MSG_PSGT, #2910).
//! The relay itself lands in a later PR of the Phase II chain; peers below this
//! version will never be sent PSGT inventory once it does. This is a FIXED
//! marker equal to the PROTOCOL_VERSION of the release that adds PSGT relay
//! (180330, the block v15 minimum) and must NOT be changed to track future
//! PROTOCOL_VERSION bumps -- a 180330 peer still supports PSGT relay regardless
//! of later protocol versions. Hence the literal rather than an alias.
static const int PSGT_PROTO_VERSION = 180330;

//! Note that there may be special logic implemented for
//! a hard fork that actually disconnects nodes less than
//! PROTOCOL_VERSION after a grace period above the height of
//! the fork this PROTOCOL_VERSION carries. This is activated by setting the
//! DISCONNECT_OLD_VERSION_AFTER_GRACE_PERIOD to true.
static const bool DISCONNECT_OLD_VERSION_AFTER_GRACE_PERIOD = true;

//! The disconnect grace period is now per-network in Consensus::Params::ProtocolVersionGracePeriod.

//! Disconnect from peers older than this proto version. This is absolute.
//! It is always the previous release's protocol version: such a peer
//! satisfies every fork before the one this release carries, so it stays
//! connected until that fork's grace period has elapsed. Anything older
//! predates an activated fork. Derived rather than a literal so it cannot
//! fall behind a PROTOCOL_VERSION bump.
static const int MIN_PEER_PROTO_VERSION = PROTOCOL_VERSION - 1;

//! initial proto version, to be increased after version/verack negotiation.
static const int INIT_PROTO_VERSION = 180275;

// database format versioning
//
//! The current database version
static const int DATABASE_VERSION = 180015;

#endif
