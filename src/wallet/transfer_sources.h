// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WALLET_TRANSFER_SOURCES_H
#define GRIDCOIN_WALLET_TRANSFER_SOURCES_H

#include "fs.h"
#include "interfaces/file_transfer.h"
#include "wallet/capability.h"

#include <cstddef>
#include <cstdint>
#include <functional>
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

//! The name of the dump stream's one entry.
extern const char* const WALLET_DUMP_ENTRY_NAME;

//! A source with one entry, "wallet-dump.txt" (size -1, its size_hint the
//! generator's), that streams the text dumpwallet writes, generated as it is
//! read: nothing is written to disk.
//!
//! The source holds capability \p cap of kind FileTransfer for its run, and
//! makes the dump only under it: the generator is made, and each read()
//! generates, inside a CapabilityScope(wallet, cap, FileTransfer) opened on
//! the calling thread. Once the capability has ended (EndElevation(), its
//! lifetime, or the client's UnlockContext ending it), every read returns an
//! error chunk. Lines generated beyond a read's bound wait in a buffer in
//! secure memory for the next read, and every intermediate string is
//! cleansed.
//!
//! The source ends \p cap itself (wallet.EndElevation()) at the dump's end,
//! on cancel(), when \p scope closes, on a read error, and in the destructor,
//! whichever comes first. Registers one close hook on \p scope, which the
//! destructor removes; a null \p scope (in process) registers nothing.
std::unique_ptr<interfaces::FileSource> MakeDumpWalletSource(CWallet& wallet, CapabilityId cap,
                                                             std::shared_ptr<interfaces::ConnectionScope> scope);

//! A sink that takes an upload of a wallet dump into memory and imports it.
//!
//! It accepts up to \p size bytes, at most the importwallet upload cap, into
//! a buffer in secure memory (pieces from the locked pool; never on the
//! core's disk). finish() checks the size and SHA-256 the client sends
//! against what arrived, then runs ImportWalletFromStream() on the buffer
//! inside a CapabilityScope(wallet, cap, FileTransfer) opened on the calling
//! thread, and reports its result as output, or its refusal as error. The
//! buffer is cleansed on every path: finish, cancel(), a refused write, the
//! scope's close and the destructor. \p cap is ended (wallet.EndElevation())
//! on each of them as well. A null \p scope registers no close hook.
std::unique_ptr<interfaces::FileSink> MakeImportWalletSink(CWallet& wallet, CapabilityId cap, uint64_t size,
                                                           std::shared_ptr<interfaces::ConnectionScope> scope);

//! Runs one RPC and returns its reply as text: a string reply as it is, a null
//! reply as "", any other as write(2). Throws what the RPC throws.
using RpcRunner = std::function<std::string(const std::string& method, const std::vector<std::string>& args)>;

//! The runner the console uses: refused while the RPC server warms up, then
//! tableRPC.execute(method, RPCConvertValues(method, args)).
RpcRunner DefaultRpcRunner();

//! A sink for a console command that reads a file (parseaccrualsnapshotfile).
//!
//! It writes the upload to a fresh core temporary file in the transfer
//! directory through util::TransferTempFile, accepting up to \p size bytes
//! and never more than \p max. finish() checks the size and SHA-256, renames
//! the file into place, inserts its path into \p args at \p path_index, runs
//! \p run and returns its text as output (or what it threw as error). The
//! file is deleted on every path: after the command, on a refused write,
//! cancel(), the scope's close and the destructor. A null \p scope registers
//! no close hook.
std::unique_ptr<interfaces::FileSink> MakeRpcFileSink(std::string method, std::vector<std::string> args,
                                                      size_t path_index, uint64_t size, uint64_t max,
                                                      std::shared_ptr<interfaces::ConnectionScope> scope,
                                                      RpcRunner run = DefaultRpcRunner());

//! A source for a console command that writes a file (dumpcontracts).
//!
//! Inserts a fresh path in the transfer directory, where nothing is yet, into
//! \p args at \p path_index, runs \p run, and returns a snapshot source
//! (MakeSnapshotSource()) over the file the command wrote, with the command's
//! reply text as the entry's note. When the command throws, or writes no
//! regular file there, returns null with \p error set and leaves nothing in
//! the transfer directory.
std::unique_ptr<interfaces::FileSource> MakeRpcOutputSource(const std::string& method, std::vector<std::string> args,
                                                            size_t path_index,
                                                            std::shared_ptr<interfaces::ConnectionScope> scope,
                                                            std::string& error, RpcRunner run = DefaultRpcRunner());

} // namespace wallet

#endif // GRIDCOIN_WALLET_TRANSFER_SOURCES_H
