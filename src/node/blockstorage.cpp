// Copyright (c) 2011-2021 The Bitcoin Core developers
// Copyright (c) 2021 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "clientversion.h"
#include "dbwrapper.h"
#include "main.h"
#include "protocol.h"
#include "serialize.h"
#include "validation.h"

#include <stdio.h>


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
                       const Consensus::Params& params, bool fReadTransactions=true)
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
                       bool fReadTransactions=true)
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

