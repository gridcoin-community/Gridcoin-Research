// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WALLET_TRANSFER_SOURCES_H
#define GRIDCOIN_WALLET_TRANSFER_SOURCES_H

#include "fs.h"
#include "interfaces/file_transfer.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

class CWallet;

namespace interfaces {
class ConnectionScope;
} // namespace interfaces

//! The core side of the file-transfer channel. The core copies what a client
//! asks for into a directory of its own, streams the copy, and deletes it. Nothing
//! is written to, or read from, a path the client named.
namespace wallet {

//! <network datadir>/transfers, which holds the files the core streams to a
//! client. Created owner-only (util::CreateOwnerOnlyDirectory) when missing. A
//! failure to create it is logged here; the snapshot functions below report it to
//! their caller. They also refuse a transfer directory that is a symbolic link,
//! which CleanTransferDir() would not reach, and leave such a link as it is.
fs::path GetTransferDir();

//! Remove what an earlier run left in the transfer directory: every regular file
//! and symbolic link directly inside it. A link is removed itself, never its
//! target, and subdirectories are left as they are. A transfer directory that is
//! itself a symbolic link is not followed. Logs the count. Called at startup once
//! the data directory lock is held, so no transfer of this process is running.
void CleanTransferDir();

//! Whether the configuration file may be backed up. False, with \p reason set,
//! in the deployments where the core runs under another account:
//! -ipcattachgroup is set (read through GetConfiguredOption, so
//! -noipcattachgroup is unset), or (POSIX) the configuration file exists and
//! its owner is not this process's effective uid. The file holds the RPC
//! password then, and it belongs to the administrator. A configuration file
//! that does not exist counts as permitted.
bool ConfigBackupPermitted(std::string& reason);

//! One file a snapshot source streams. \p path is a file in the transfer
//! directory that the source owns and deletes. An empty \p path means there is
//! nothing to send for the item: the entry is listed with size 0 and its
//! \p note, and reading it returns an error chunk carrying the note.
struct SnapshotEntry
{
    std::string name;
    fs::path path;
    std::string note;
};

//! A source that streams each entry from its file.
//!
//! An entry is read sequentially, once. Reading one entry while another is
//! partly read, reading an entry again after its end, or any call after cancel()
//! or the scope's close returns an error chunk (an error trailer from trailer()).
//! Each read is capped at FILE_TRANSFER_CHUNK_BYTES.
//!
//! The source owns the entry files and deletes them all: each one as soon as it
//! has been read to its end (so before its trailer is asked for), and every one
//! left on cancel(), on a read error, when \p scope closes, and in the
//! destructor. A file the scope's close hook finds being read is left to that
//! read, which deletes it when it returns. Registers one close hook on \p scope,
//! which the destructor removes. A null \p scope (in process, where there is no
//! connection) registers nothing. The mutex inside is a leaf, and no file I/O
//! happens under it.
std::unique_ptr<interfaces::FileSource> MakeSnapshotSource(std::vector<SnapshotEntry> entries,
                                                           std::shared_ptr<interfaces::ConnectionScope> scope);

//! Copy the wallet file with GRC::BackupWallet (under bitdb.cs_db, after a
//! flush) to a fresh "snap-<16 hex>.tmp" in the transfer directory, and make it
//! owner-only (0600 on POSIX; on Windows it inherits the directory's owner-only
//! access). nullopt, with \p error set and nothing left behind, on failure.
std::optional<fs::path> SnapshotWallet(const CWallet& wallet, std::string& error);

//! Copy a regular file to a fresh "snap-<16 hex>.tmp" in the transfer directory
//! through util::TransferTempFile (created exclusively and owner-only, then
//! renamed). nullopt, with \p error set and nothing left behind, on failure.
std::optional<fs::path> SnapshotPlainFile(const fs::path& source, std::string& error);

} // namespace wallet

#endif // GRIDCOIN_WALLET_TRANSFER_SOURCES_H
