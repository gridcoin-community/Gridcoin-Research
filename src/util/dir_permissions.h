// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_UTIL_DIR_PERMISSIONS_H
#define GRIDCOIN_UTIL_DIR_PERMISSIONS_H

#include "fs.h"

#include <optional>
#include <string>

//! Owner-only directory creation, shared by every place a Gridcoin data directory
//! can come into existence.
//!
//! Deliberately in util/ rather than ipc/: the multiprocess IPC library is only
//! built when ENABLE_MULTIPROCESS=ON, but a data directory is created in the
//! monolithic build too (the Qt first-run chooser), and it needs the same
//! protection. Keeping the Windows DACL machinery here means one implementation
//! serves the chooser, the daemon's first start, and the IPC transport.
//!
//! The header dependency is deliberately just fs.h: the circular-dependency lint
//! ratchet is at zero, and this must not be the file that starts a cycle.
namespace util {

//! Create \p path (and any missing parents) and, IF THIS CALL CREATED IT, restrict
//! it to the current user: mode 0700 on POSIX, an explicit owner+SYSTEM PROTECTED
//! DACL on Windows (read back and verified, so a volume that silently stores no
//! ACL is reported rather than trusted).
//!
//! CREATE-ONLY, and that is a rule rather than an accident: a directory that
//! already exists is left exactly as the operator configured it. Someone who
//! deliberately widened their data directory must not have it silently
//! re-tightened on the next start.
//!
//! \param error  On false, a human-readable reason (never empty).
//! \return true if the directory exists and is usable when this returns — whether
//!         created here or already present.
//!
//! FALSE IS FATAL, and callers must treat it that way. It means one of exactly two
//! things, because a pre-existing directory returns TRUE without being inspected:
//!
//!   1. the directory could not be created; or
//!   2. this call created it and could not restrict it — in which case the
//!      directory is REMOVED again before returning, so a retry is a real retry.
//!
//! What it never means is "an existing directory has permissions we dislike". That
//! case returns true untouched, per the create-only rule. So a caller that sees
//! false and carries on because the directory exists is reasoning about a state
//! this function cannot produce: the directory exists because IT just made it and
//! failed to protect it. Carrying on puts a wallet somewhere known to be exposed,
//! and on the next start the directory is pre-existing, returns true, and the
//! failure is never reported again.
//!
//! In the one case where the withdrawal itself fails, the directory is left in
//! place and \p error says so explicitly.
bool CreateOwnerOnlyDirectory(const fs::path& path, std::string& error);

//! CreateOwnerOnlyDirectory for a directory whose files are better kept with weaker
//! protection than not kept at all: wallet backups in -backupdir. When this call
//! created \p path and cannot restrict it because the filesystem keeps no modes (on
//! POSIX chmod fails with EPERM, ENOTSUP, EOPNOTSUPP or EROFS, as on FAT; on Windows
//! the volume lacks FILE_PERSISTENT_ACLS), the directory is kept, \p unrestricted is
//! set to the reason, and true is returned. Any other failure to restrict it
//! withdraws it and returns false, as CreateOwnerOnlyDirectory does, so a directory
//! left with permissive inherited access is never kept. False, with \p error set,
//! also when the directory could not be created. A directory that exists is left as
//! it is.
bool CreateDirectoryRestrictedIfPossible(const fs::path& path, std::string& error,
                                         std::optional<std::string>& unrestricted);

#ifndef WIN32
//! Whether chmod failing with \p err means the filesystem keeps no POSIX modes
//! (EPERM, ENOTSUP, EOPNOTSUPP or EROFS), the only failures
//! CreateDirectoryRestrictedIfPossible tolerates.
bool RestrictErrorMeansNoModes(int err);
#endif

//! Restrict the process umask so that every file and directory this process
//! creates afterwards is owner-only from the moment it exists.
//!
//! MUST be called before anything is created -- before logging is initialised and
//! before settings are read or written. The ordering is the entire point. The
//! alternative, a chmod after the fact, leaves a window in which the file sits at
//! whatever the ambient umask allowed; CreateNewConfigFile() documents that same
//! reasoning for the RPC credentials file, which is narrowed before the password
//! is written rather than after.
//!
//! This governs the files a data directory accumulates that are not created
//! through CreateOwnerOnlyDirectory(): debug.log, the read-write settings file,
//! and the block index subdirectories. On a default desktop umask of 022 those
//! land group- and world-readable, which matters most for debug.log.
//!
//! It is a deliberate blunt instrument: process-wide, set once, never restored.
//! Code that needs a different mode for one file should say so explicitly at that
//! call site, as ipc/process.cpp does when it creates the IPC socket.
//!
//! No-op on Windows, which has no umask. There the equivalent protection is the
//! data directory's inheritable owner+SYSTEM DACL, applied by
//! CreateOwnerOnlyDirectory() and inherited by everything created inside it.
void SetOwnerOnlyUmask();

#ifdef WIN32
//! Make the current user the owner of an existing file or directory, apply an
//! owner+SYSTEM PROTECTED DACL to it, and verify both by reading them back exactly.
//! Throws on failure: std::system_error when a call fails, std::runtime_error when
//! what is read back differs. Callers that hold secrets (the IPC socket and cookie)
//! rely on this failing closed.
//!
//! \param inheritable  On a directory, make the ACEs inheritable so entries created
//!                     inside are owner-only from creation rather than after the
//!                     fact, which closes the create/protect window.
void ApplyOwnerOnlyDacl(const fs::path& path, bool inheritable);

//! Confirm \p path's DACL is present and protected, its owner is the current user or
//! SYSTEM, and every allow entry of any kind, inherit-only ones included, names one
//! of the two. Throws std::runtime_error describing the discrepancy otherwise, and
//! std::system_error when the ACL cannot be read.
void VerifyOwnerOnlyDacl(const fs::path& path);

//! The same DACL as a raw PACL, for callers that need it at CREATE time via
//! SECURITY_ATTRIBUTES (CreateFileW) rather than applying it afterwards. The caller
//! owns the result and must LocalFree() it. Returned as void* so this header stays
//! free of <windows.h>.
void* MakeOwnerOnlyDacl(bool inheritable);
#endif // WIN32

} // namespace util

#endif // GRIDCOIN_UTIL_DIR_PERMISSIONS_H
