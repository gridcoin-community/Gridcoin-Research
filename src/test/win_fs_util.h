// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_WIN_FS_UTIL_H
#define GRIDCOIN_TEST_WIN_FS_UTIL_H

#include "fs.h"
#include "util/win_acl.h"

#include <string>

namespace grc_test {

//! Whether \p a and \p b read the same: the owner, the DACL's presence and
//! protection, and every entry, in order.
bool SameAcl(const util::winacl::ObservedAcl& a, const util::winacl::ObservedAcl& b);

//! Writes \p text to the new file \p path, a file of this account's own. On Windows it is
//! created with this account as its owner and the owner-only list, as the core creates a file:
//! a file an elevated token creates otherwise belongs to Administrators, which is not this
//! account (fsmodes::OwnedByUs). Under Wine, which may not keep the list, it is written plainly.
void WriteOwnFile(const fs::path& path, const std::string& text);

#ifdef WIN32
//! Create the new file \p path holding \p text, with \p spec as its owner and access list from
//! the start (CreateFileW with MakeSecurityDescriptor(spec)).
void CreateFileWithAcl(const fs::path& path, const util::winacl::AclSpec& spec, const std::string& text);

//! Make the new, empty directory \p link a junction (a mount-point reparse point)
//! to the directory \p target. No privilege is needed for that.
void MakeJunction(const fs::path& link, const fs::path& target);

//! Whether Windows refuses to open \p path, following any junction on it, with
//! ERROR_UNTRUSTED_MOUNT_POINT: current Windows 11 builds do not follow a junction that
//! an account that is not an administrator made. Decides which behaviour a case states.
bool JunctionUntrustedOn(const fs::path& path);

//! Whether this process's token holds the privilege \p name (such as SE_BACKUP_NAME),
//! and whether it is enabled.
enum class PrivilegeState
{
    NotHeld,
    Disabled,
    Enabled,
};
PrivilegeState TokenPrivilegeState(const wchar_t* name);
#endif

} // namespace grc_test

#endif // GRIDCOIN_TEST_WIN_FS_UTIL_H
