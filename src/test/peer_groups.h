// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_PEER_GROUPS_H
#define GRIDCOIN_TEST_PEER_GROUPS_H

namespace grc_test {

//! Whether this process can read SO_PEERGROUPS, which the IPC peer check needs to identify a
//! Unix-socket peer on Linux. qemu-user, which runs the armhf suites in CI, hands the option to
//! the kernel with room for one int, so a process with supplementary groups gets ERANGE whatever
//! size it asks for; a real kernel never does given room for every group. The peer check then
//! refuses every peer, so a case that needs it asks this first. True where there is no
//! SO_PEERGROUPS.
bool PeerGroupsReadable();

} // namespace grc_test

#endif // GRIDCOIN_TEST_PEER_GROUPS_H
