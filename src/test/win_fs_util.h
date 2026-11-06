// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_WIN_FS_UTIL_H
#define GRIDCOIN_TEST_WIN_FS_UTIL_H

#include "fs.h"
#include "util/win_acl.h"

namespace grc_test {

//! Whether \p a and \p b read the same: the owner, the DACL's presence and
//! protection, and every entry, in order.
bool SameAcl(const util::winacl::ObservedAcl& a, const util::winacl::ObservedAcl& b);

#ifdef WIN32
//! Make the new, empty directory \p link a junction (a mount-point reparse point)
//! to the directory \p target. No privilege is needed for that.
void MakeJunction(const fs::path& link, const fs::path& target);
#endif

} // namespace grc_test

#endif // GRIDCOIN_TEST_WIN_FS_UTIL_H
