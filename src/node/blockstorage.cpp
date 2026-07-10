// Copyright (c) 2011-2021 The Bitcoin Core developers
// Copyright (c) 2021 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "node/blockstorage.h"

#include "chainparams.h"
#include "clientversion.h"
#include "consensus/consensus.h"
#include "dbwrapper.h"
#include "gridcoin/staking/chain_trust.h"
#include "gridcoin/staking/spam.h"
#include "init.h"
#include "main.h"
#include "net.h"
#include "node/chainman.h"
#include "node/ui_interface.h"
#include "protocol.h"
#include "serialize.h"
#include "util/string.h"
#include "util/time.h"
#include "validation.h"

#include <stdio.h>

// Chain-state globals defined in main.cpp that LoadBlockIndex references;
// same ad-hoc extern pattern as node/chainman.cpp and node/coherence.cpp.
extern GRC::ChainTrustCache g_chain_trust GUARDED_BY(cs_main);
extern GRC::SeenStakes g_seen_stakes GUARDED_BY(cs_main);


bool WriteBlockToDisk(const CBlock& block, unsigned int& nFileRet, unsigned int& nBlockPosRet,
                      const CMessageHeader::MessageStartChars& messageStart)
        EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);

    // Open history file to append
    CAutoFile fileout(AppendBlockFile(nFileRet), SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("%s: AppendBlockFile failed", __func__);

    // Write index header
    unsigned int nSize = GetSerializeSize(fileout, block);
    fileout << messageStart << nSize;

    // Write block
    long fileOutPos = ftell(fileout.Get());
    if (fileOutPos < 0)
        return error("%s: ftell failed", __func__);
    nBlockPosRet = fileOutPos;
    fileout << block;

    // Flush stdio buffers and commit to disk before returning
    fflush(fileout.Get());
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
        fShutdown = true;
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

FILE* AppendBlockFile(unsigned int& nFileRet)
{
    nFileRet = 0;
    while (true)
    {
        FILE* file = OpenBlockFile(nCurrentBlockFile, 0, "ab");
        if (!file)
            return nullptr;
        if (fseek(file, 0, SEEK_END) != 0) {
            fclose(file);
            return nullptr;
        }
        // FAT32 file size max 4GB, fseek and ftell max 2GB, so we must stay under 2GB
        if (ftell(file) < (long)(0x7F000000 - MAX_SIZE))
        {
            nFileRet = nCurrentBlockFile;
            return file;
        }
        fclose(file);
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
            while (nPos != (unsigned int)-1 && !fRequestShutdown)
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
                } while(!fRequestShutdown);

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
    else if (fTestNet)
    {
        // GLOBAL TESTNET SETTINGS - R HALFORD
        nStakeMinAge = 1 * 60 * 60; // test net min age is 1 hour
        nCoinbaseMaturity = 10; // test maturity is 10 blocks
        nGrandfather = 196550;
        //1-24-2016
        MAX_OUTBOUND_CONNECTIONS = (int)gArgs.GetArg("-maxoutboundconnections", 8);
    }

    LogPrintf("Mode=%s", Params().IsMockableChain() ? "RegTest" : fTestNet ? "TestNet" : "Prod");

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
                LogPrintf("regtest: WARN — genesis coinbase NOT readable back from tx index");
            }
        }
    }

    if (fRequestShutdown) {
        return true;
    }

    UpdateSyncTime(pindexBest);

    g_chain_trust.Initialize(pindexGenesisBlock, pindexBest);
    g_seen_stakes.Refill(pindexBest);

    return true;
}
