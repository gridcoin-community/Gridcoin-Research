// Copyright (c) 2011-2021 The Bitcoin Core developers
// Copyright (c) 2021 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "node/blockstorage.h"
#include "node/shutdown.h"

#include "chainparams.h"
#include "clientversion.h"
#include "consensus/consensus.h"
#include "dbwrapper.h"
#include "gridcoin/staking/chain_trust.h"
#include "gridcoin/staking/spam.h"
#include "init.h"
#include "net.h"
#include "node/chainman.h"
#include "node/ui_interface.h"
#include "protocol.h"
#include "serialize.h"
#include "util.h"
#include "util/string.h"
#include "util/time.h"
#include "validation.h"

#include <cerrno>
#include <sstream>
#include <stdio.h>

bool WriteBlockToDisk(const CBlock& block, unsigned int& nFileRet, unsigned int& nBlockPosRet,
                      const CMessageHeader::MessageStartChars& messageStart)
        EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);

    // Open history file to append
    // AppendBlockFile hands back a CACHED, non-owning handle (see there). CAutoFile
    // would fclose it on every scope exit, which is exactly the per-block close we
    // are removing, so the pointer is released back below before this goes out of
    // scope. CAutoFile is retained only for its serialization operators.
    CAutoFile fileout(AppendBlockFile(nFileRet), SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("%s: AppendBlockFile failed", __func__);

    // Disown on EVERY exit -- including the error returns below and an exception
    // thrown out of serialization -- because letting CAutoFile close the cached
    // handle would leave g_block_file dangling.
    //
    // Disowning alone is not enough on an UNSUCCESSFUL exit, though. Serializing
    // the header or the block can throw on a short fwrite(), and the old per-call
    // CAutoFile would have closed the file as the exception unwound. With the cache
    // in place, a bare release() hands a partially written, error-state stream back
    // to g_block_file, and the next block appends through it -- writing after a
    // torn record and inheriting the error state. So the handle is invalidated
    // unless the write is known to have completed: ok is set only after
    // serialization, the flush and any sync have all succeeded.
    struct ReleaseCachedHandle {
        CAutoFile& file;
        bool ok = false;
        ~ReleaseCachedHandle()
        {
            file.release();            // never let CAutoFile close a cached handle
            if (!ok) CloseBlockFile(); // ... but do not keep one of unknown state
        }
    } release_guard{fileout};

    // Write index header
    unsigned int nSize = GetSerializeSize(fileout, block);
    fileout << messageStart << nSize;

    // Write block
    long fileOutPos = ftell(fileout.Get());
    if (fileOutPos < 0)
        return error("%s: ftell failed", __func__);
    nBlockPosRet = fileOutPos;
    fileout << block;

    // Flush stdio buffers and commit to disk before returning.
    //
    // The result matters here in a way it did not when this function opened and
    // closed blk*.dat around every block. Below, FileCommit() is skipped for most
    // blocks during IBD, so this fflush is the only thing that moves the block out
    // of stdio's buffer -- and discarding its result lets a disk-full or I/O error
    // report success, after which the block index records a position for data that
    // never left the process.
    //
    // A stream that failed to flush retains its error state, so it must not go back
    // into the cache; the guard above handles that for this and every other
    // unsuccessful exit.
    if (fflush(fileout.Get()) != 0) {
        // The guard invalidates the cached handle on this unsuccessful exit.
        return error("%s: fflush failed, block not written (errno %d)", __func__, errno);
    }
    if (!IsInitialBlockDownload() || (nBestHeight + 1) % 5000 == 0) {
        // Pair the block-file fsync with a LevelDB WAL sync barrier so the
        // block index DB cannot be made durable referencing blk*.dat data
        // that has not itself been fsynced. This converts the "index
        // committed but data still in OS page cache" failure mode
        // (Scenario C in issue #2865) into the safe "data on disk but
        // index doesn't know about it yet" mode (Scenario B), at the cost
        // of one small WAL fsync per fsync boundary.
        //
        // Either of these failing means we cannot uphold the coordination
        // invariant for this block. Return false so AcceptBlock rejects it
        // before AddToBlockIndex runs: the unsynced bytes already written
        // become harmless dead space (the next append seeks past them and
        // no LevelDB entry ever references them), and the peer will
        // re-relay the block. Both calls log their own failure reason.
        if (!FileCommit(fileout.Get())) {
            return error("%s: FileCommit failed for blk%05u.dat", __func__, nFileRet);
        }
        if (!CTxDB().Sync()) {
            return error("%s: CTxDB::Sync failed (block-index WAL barrier)", __func__);
        }
    }

    // Everything that can leave the stream unusable has succeeded: the cached
    // handle may be reused for the next append.
    release_guard.ok = true;
    return true;
}


bool ReadBlockFromDisk(CBlock& block, unsigned int nFile, unsigned int nBlockPos,
                       const Consensus::Params& params, bool fReadTransactions)
{
    block.SetNull();

    const int ser_flags = SER_DISK | (fReadTransactions ? 0 : SER_BLOCKHEADERONLY);

    // Open history file to read
    CAutoFile filein(OpenBlockFile(nFile, nBlockPos, "rb"), ser_flags, CLIENT_VERSION);
    if (filein.IsNull())
        return error("%s: OpenBlockFile failed", __func__);

    // Read block
    try {
        filein >> block;
    }
    catch (std::exception &e) {
        return error("%s: deserialize or I/O error", __func__);
    }

    // Check the header
    if (fReadTransactions && block.IsProofOfWork() && !CheckProofOfWork(block.GetHash(true), block.nBits, params))
        return error("%s: errors in block header", __func__);

    return true;
}


bool ReadBlockFromDisk(CBlock& block, const CBlockIndex* pindex, const Consensus::Params& params,
                       bool fReadTransactions)
{
    if (!fReadTransactions)
    {
        block.SetNull();
        *(static_cast<CBlockHeader*>(&block)) = pindex->GetBlockHeader();
        return true;
    }

    if (!ReadBlockFromDisk(block, pindex->nFile, pindex->nBlockPos, params, fReadTransactions))
        return false;

    if (block.GetHash(true) != pindex->GetBlockHash())
        return error("%s: hash doesn't match index (%s != %s)", __func__, block.GetHash(true).GetHex(),
                                                                          pindex->GetBlockHash().GetHex());
    return true;
}


// Minimum disk space required - used in CheckDiskSpace()
static const uint64_t nMinDiskSpace = 52428800;

bool CheckDiskSpace(uint64_t nAdditionalBytes)
{
    uint64_t nFreeBytesAvailable = fs::space(GetDataDir()).available;

    // Check for nMinDiskSpace bytes (currently 50MB)
    if (nFreeBytesAvailable < nMinDiskSpace + nAdditionalBytes)
    {
        SetShutdownInProgress();
        std::string strMessage = _("Warning: Disk space is low!");
        strMiscWarning = strMessage;
        LogPrintf("*** %s", strMessage);
        uiInterface.ThreadSafeMessageBox(strMessage, "Gridcoin", CClientUIInterface::MSG_ERROR);
        StartShutdown();
        return false;
    }
    return true;
}

static fs::path BlockFilePath(unsigned int nFile)
{
    std::string strBlockFn = strprintf("blk%04u.dat", nFile);
    return GetDataDir() / strBlockFn;
}

FILE* OpenBlockFile(unsigned int nFile, unsigned int nBlockPos, const char* pszMode)
{
    if ((nFile < 1) || (nFile == (unsigned int) -1))
        return nullptr;
    FILE* file = fsbridge::fopen(BlockFilePath(nFile), pszMode);
    if (!file)
        return nullptr;
    if (nBlockPos != 0 && !strchr(pszMode, 'a') && !strchr(pszMode, 'w'))
    {
        if (fseek(file, nBlockPos, SEEK_SET) != 0)
        {
            fclose(file);
            return nullptr;
        }
    }
    return file;
}

static unsigned int nCurrentBlockFile = 1;

//! Cached append handle for the current block file. Guarded by cs_main: every
//! caller of AppendBlockFile holds it (WriteBlockToDisk asserts it, and the
//! shutdown flush in init.cpp takes it explicitly).
//!
//! Why cache. WriteBlockToDisk used to open and close blk*.dat for every single
//! block, purely because CAutoFile owns what it is handed and closes it on scope
//! exit -- incidental RAII inherited from the Bitcoin 0.8-era code, not a
//! durability mechanism. Durability is already explicit and independent: fflush
//! after each block, and FileCommit gated to once per 5000 blocks during IBD
//! (issue #2865). Nothing needed the handle closed.
//!
//! What caching does NOT buy, recorded because it was the original motivation and
//! it turned out to be wrong. Windows Defender makes a sync from zero roughly 3.5x
//! slower on an unexcluded node (measured on a 2-core Windows 11 VM, mainnet blocks
//! 1..100,000 from a LAN peer: 605s with MsMpEng at 84% CPU against the wallet's
//! 21%; 172s with blk*.dat excluded). The theory was that Defender was scanning on
//! close-after-modify, so removing the per-block close would remove the trigger.
//!
//! It does not. Measured again with the cache in place and no exclusion: 674s, with
//! MsMpEng still at 70%. Defender scans the WRITES, through on-access protection,
//! and holding the handle open cannot change that because the appends still happen.
//! The blk*.dat exclusion documented in contrib/windows/README.md is therefore
//! required, not optional -- do not remove it on the strength of this cache.
//!
//! Nothing here changes what reaches the platter: the same fflush, the same
//! FileCommit cadence, the same LevelDB WAL barrier ordering.
static FILE* g_block_file = nullptr;
static unsigned int g_block_file_num = 0;

void CloseBlockFile()
{
    if (g_block_file == nullptr) return;

    fclose(g_block_file);
    g_block_file = nullptr;
    g_block_file_num = 0;
}

FILE* AppendBlockFile(unsigned int& nFileRet)
{
    nFileRet = 0;
    while (true)
    {
        // Reuse the cached handle when it is the file we are appending to. It was
        // opened "ab", so writes always land at the end regardless of where any
        // intervening ftell/fseek left the stream.
        FILE* file = (g_block_file != nullptr && g_block_file_num == nCurrentBlockFile)
            ? g_block_file
            : nullptr;

        if (file == nullptr) {
            CloseBlockFile();   // a stale handle for a previous file, if any
            file = OpenBlockFile(nCurrentBlockFile, 0, "ab");
            if (!file)
                return nullptr;
            g_block_file = file;
            g_block_file_num = nCurrentBlockFile;
        }

        if (fseek(file, 0, SEEK_END) != 0) {
            // Drop the cached handle rather than hand back one we cannot position.
            CloseBlockFile();
            return nullptr;
        }
        // FAT32 file size max 4GB, fseek and ftell max 2GB, so we must stay under 2GB
        if (ftell(file) < (long)(0x7F000000 - MAX_SIZE))
        {
            nFileRet = nCurrentBlockFile;
            return file;
        }
        // Rolled over: this file is full and will never be appended to again.
        // Commit it before closing -- the previous code only fclose()d here, which
        // flushes stdio to the OS but does not put the bytes on the platter. Doing
        // it once per ~2GB boundary is free and makes the handoff strictly more
        // durable than before, not less.
        if (fflush(file) != 0 || !FileCommit(file)) {
            LogPrintf("WARN: %s: could not commit blk%05u.dat before rolling over to "
                      "the next block file; its tail may not be durable yet",
                      __func__, nCurrentBlockFile);
        }
        CloseBlockFile();
        nCurrentBlockFile++;
    }
}

bool LoadExternalBlockFile(FILE* fileIn, size_t file_size, unsigned int percent_start, unsigned int percent_end)
{
    int64_t nStart = GetTimeMillis();
    int nLoaded = 0;

    bool display_progress = (file_size > 0 && (percent_end - percent_start) > 0) ? true : false;
    unsigned int cached_percent_progress = 0;

    if (display_progress) {
        uiInterface.InitMessage(_("Block file load progress ") + ToString(percent_start) + "%");
    }

    {
        LOCK(cs_main);
        try {
            CAutoFile blkdat(fileIn, SER_DISK, CLIENT_VERSION);
            unsigned int nPos = 0;
            while (nPos != (unsigned int)-1 && !ShutdownRequested())
            {
                unsigned char pchData[65536];
                do {
                    fseek(blkdat.Get(), nPos, SEEK_SET);
                    int nRead = fread(pchData, 1, sizeof(pchData), blkdat.Get());
                    if (nRead <= 8)
                    {
                        nPos = (unsigned int)-1;
                        break;
                    }
                    void* nFind = memchr(pchData, Params().MessageStart()[0], nRead + 1 - CMessageHeader::MESSAGE_START_SIZE);
                    if (nFind)
                    {
                        if (memcmp(nFind, Params().MessageStart(), CMessageHeader::MESSAGE_START_SIZE) == 0)
                        {
                            nPos += ((unsigned char*)nFind - pchData) + CMessageHeader::MESSAGE_START_SIZE;
                            break;
                        }
                        nPos += ((unsigned char*)nFind - pchData) + 1;
                    }
                    else
                        nPos += sizeof(pchData) - CMessageHeader::MESSAGE_START_SIZE + 1;
                } while(!ShutdownRequested());

                if (nPos == (unsigned int)-1) {
                    if (display_progress) {
                        uiInterface.InitMessage(_("Block file load progress ") + ToString(percent_end) + "%");
                    }

                    break;
                }

                fseek(blkdat.Get(), nPos, SEEK_SET);
                unsigned int nSize;
                blkdat >> nSize;
                if (nSize > 0 && nSize <= MAX_BLOCK_SIZE)
                {
                    CBlock block;
                    blkdat >> block;
                    CValidationState load_state;
                    if (ProcessBlock(nullptr, &block, false, load_state)) {
                        ++nLoaded;

                        if (display_progress) {
                            unsigned int percent_progress = percent_start + (uint64_t) nPos
                                    * (uint64_t) (percent_end - percent_start) / file_size;

                            if (percent_progress != cached_percent_progress) {
                                uiInterface.InitMessage(_("Block file load progress ") + ToString(percent_progress) + "%");
                                LogPrintf("INFO: %s: blocks/s: %f, progress: %u%%", __func__,
                                          nLoaded / ((GetTimeMillis() - nStart) / 1000.0), percent_progress);

                                cached_percent_progress = percent_progress;
                            }
                        } else if (nLoaded % 10000 == 0) {
                            LogPrintf("Blocks/s: %f", nLoaded / ((GetTimeMillis() - nStart) / 1000.0));
                        }

                        nPos += 4 + nSize;
                    }
                }
            }
        }
        catch (std::exception &e) {
            LogPrintf("%s() : Deserialize or I/O error caught during load",
                   __PRETTY_FUNCTION__);
        }
    }
    LogPrintf("Loaded %i blocks from external file in %" PRId64 "ms", nLoaded, GetTimeMillis() - nStart);
    return nLoaded > 0;
}


// Load-time entry point moved here from main.cpp (issue #3125, workstream
// C5): loads the block index from LevelDB and, on an empty datadir, creates
// the genesis block via CreateGenesisBlock() (chainparams.cpp) and writes it
// to disk. Takes cs_main internally.
bool LoadBlockIndex(bool fAllowNew)
{
    LOCK(cs_main);

    if (Params().IsMockableChain())
    {
        // GLOBAL REGTEST SETTINGS — staking and maturity gated to 0 so the kernel
        // checks at gridcoin/staking/kernel.cpp:617,654 pass trivially. nGrandfather
        // is irrelevant at regtest heights.
        nStakeMinAge = 0;
        nCoinbaseMaturity = 10;
        nGrandfather = 0;
        MAX_OUTBOUND_CONNECTIONS = (int)gArgs.GetArg("-maxoutboundconnections", 8);
    }
    else if (OnTestnet())
    {
        // GLOBAL TESTNET SETTINGS - R HALFORD
        nStakeMinAge = 1 * 60 * 60; // test net min age is 1 hour
        nCoinbaseMaturity = 10; // test maturity is 10 blocks
        nGrandfather = 196550;
        //1-24-2016
        MAX_OUTBOUND_CONNECTIONS = (int)gArgs.GetArg("-maxoutboundconnections", 8);
    }

    LogPrintf("Mode=%s", Params().IsMockableChain() ? "RegTest" : OnTestnet() ? "TestNet" : "Prod");

    //
    // Load block index
    //
    CTxDB txdb("cr+");
    if (!txdb.LoadBlockIndex())
        return false;

    //
    // Init with genesis block
    //
    if (mapBlockIndex.empty())
    {
        if (!fAllowNew)
            return false;

        CBlock block = CreateGenesisBlock();
        const bool fRegTest = Params().IsMockableChain();
        { CValidationState genesis_state; assert(CheckBlock(block, genesis_state, 1)); }

        // Start new block file
        unsigned int nFile;
        unsigned int nBlockPos;
        if (!WriteBlockToDisk(block, nFile, nBlockPos, Params().MessageStart()))
            return error("LoadBlockIndex() : writing genesis block to disk failed");
        const uint256 genesis_proof = fRegTest ? block.GetHash(true)
                                               : hashGenesisBlock;
        if (!AddToBlockIndex(block, nFile, nBlockPos, genesis_proof))
            return error("LoadBlockIndex() : genesis block not accepted");

        // Under -regtest the genesis coinbase carries spendable premine
        // outputs (see chainparams.cpp). LoadBlockIndex bypasses ConnectBlock
        // for genesis, so the txindex/UpdateTxIndex path that normally writes
        // each block's transactions is never taken — leaving CheckProofOfStakeV8
        // unable to ReadStakedInput for the premine UTXO. Write the genesis
        // coinbase to the tx index directly so the staker can spend it at
        // height 1.
        if (fRegTest) {
            CTxDB regtxdb;
            const unsigned int nTxPos = nBlockPos
                + ::GetSerializeSize<CBlockHeader>(block, SER_DISK, CLIENT_VERSION)
                + GetSizeOfCompactSize(block.vtx.size());
            const CDiskTxPos pos(nFile, nBlockPos, nTxPos);
            const uint256 genesis_coinbase_hash = block.vtx[0].GetHash();
            if (!regtxdb.AddTxIndex(block.vtx[0], pos, /*nHeight=*/0)) {
                return error("LoadBlockIndex() : failed to write regtest genesis tx index");
            }
            LogPrintf("regtest: wrote genesis coinbase %s to tx index at file=%u blockpos=%u txpos=%u",
                      genesis_coinbase_hash.ToString(), nFile, nBlockPos, nTxPos);
            CTxIndex verify_index;
            if (regtxdb.ReadTxIndex(genesis_coinbase_hash, verify_index)) {
                LogPrintf("regtest: verified genesis coinbase readable from tx index");
            } else {
                LogPrintf("regtest: WARN - genesis coinbase NOT readable back from tx index");
            }
        }
    }

    if (ShutdownRequested()) {
        return true;
    }

    UpdateSyncTime(pindexBest);

    g_chain_trust.Initialize(pindexGenesisBlock, pindexBest);
    g_seen_stakes.Refill(pindexBest);

    return true;
}

void PrintBlockTree() EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);
    // pre-compute tree structure
    std::map<CBlockIndex*, std::vector<CBlockIndex*> > mapNext;
    for (BlockMap::iterator mi = mapBlockIndex.begin(); mi != mapBlockIndex.end(); ++mi)
    {
        CBlockIndex* pindex = mi->second;
        mapNext[pindex->pprev].push_back(pindex);
    }

    std::vector<std::pair<int, CBlockIndex*> > vStack;
    vStack.push_back(std::make_pair(0, pindexGenesisBlock));

    int nPrevCol = 0;
    while (!vStack.empty())
    {
        int nCol = vStack.back().first;
        CBlockIndex* pindex = vStack.back().second;
        vStack.pop_back();

        std::stringstream output;

        // print split or gap
        if (nCol > nPrevCol)
        {
            for (int i = 0; i < nCol-1; i++) {
                output << "| \n";
            }

            output << "|\\\n";
        }
        else if (nCol < nPrevCol)
        {
            for (int i = 0; i < nCol; i++) {
                output << "| \n";
            }

            output << "|\n";
        }
        nPrevCol = nCol;

        // print columns
        for (int i = 0; i < nCol; i++) {
            output << "| \n";
        }

        // print item (and also prepend above formatting)
        CBlock block;
        ReadBlockFromDisk(block, pindex, Params().GetConsensus());
        LogPrintf("%s%d (%u,%u) %s  %08x  %s  tx %" PRIszu "",
                  output.str(),
                  pindex->nHeight,
                  pindex->nFile,
                  pindex->nBlockPos,
                  block.GetHash(true).ToString().c_str(),
                  block.nBits,
                  DateTimeStrFormat("%x %H:%M:%S", block.GetBlockTime()).c_str(),
                  block.vtx.size());

        // put the main time-chain first
        std::vector<CBlockIndex*>& vNext = mapNext[pindex];
        for (unsigned int i = 0; i < vNext.size(); i++)
        {
            if (vNext[i]->pnext)
            {
                std::swap(vNext[0], vNext[i]);
                break;
            }
        }

        // iterate children
        for (unsigned int i = 0; i < vNext.size(); i++)
            vStack.push_back(std::make_pair(nCol+i, vNext[i]));
    }
}
