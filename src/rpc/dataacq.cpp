// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// and The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "server.h"
#include "txdb.h"
#include "gridcoin/claim.h"
#include "gridcoin/quorum.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/superblock.h"
#include "gridcoin/support/block_finder.h"
#include "util.h"

#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/algorithm/string.hpp>
#include <algorithm>

#include <univalue.h>

using namespace GRC;
using namespace std;

extern GRC::BlockFinder RPCBlockFinder;

// Brod
static bool compare_second(const pair<std::string, int64_t>  &p1, const pair<std::string, int64_t> &p2)
{
    return p1.second > p2.second;
}

UniValue rpc_getblockstats(const UniValue& params, bool fHelp)
{
    if(fHelp || params.size() < 1 || params.size() > 3 )
        throw runtime_error(
            "getblockstats mode [startheight [endheight]]\n"
            "\n"
            "Show stats on what wallets and cpids staked recent blocks.\n"
            "\n"
            "Mode 0: Startheight is the starting height, endheight is the chain head if not specfied.\n"
            "Mode 1: Startheight is actually the number of blocks back from endheight or the chain \n"
            "        head if not specified.");

    unsigned int mode = params[0].get_int();

    int64_t lowheight = 0;

    // Even though these are typed to int64_t for the future, the max here is set to the max for the integer type,
    // because CBlockIndex nHeight is still type int.
    int64_t highheight = std::numeric_limits<int>::max();

    // Default scope to 30000 blocks unless otherwise specified
    int64_t maxblocks = 30000;

    if (mode == 0)
    {
        if (params.size() >= 2)
        {
            lowheight = params[1].get_int();
            maxblocks = std::numeric_limits<int>::max();
        }

        if (params.size() >= 3)
        {
            highheight = params[2].get_int();

            int64_t maxblocks = highheight - lowheight + 1;

            if (maxblocks < 2)
            {
                throw runtime_error("getblockstats: Number of blocks in scope less than two.");
            }
        }
    }
    else if (mode == 1)
    {
        if (params.size() >= 2) maxblocks = params[1].get_int();
        if (params.size() >= 3) highheight = params[2].get_int();

        if (maxblocks < 2)
        {
            throw runtime_error("getblockstats: number of blocks to look back less than two.");
        }
    }
    else
    {
        throw runtime_error("getblockstats: Invalid mode specified");
    }

    CBlockIndex* cur;
    UniValue result1(UniValue::VOBJ);
    {
        LOCK(cs_main);
        cur = pindexBest;
    }
    int64_t blockcount = 0;
    int64_t transactioncount = 0;
    std::map<int, int64_t> c_blockversion;
    std::map<std::string, int64_t> c_version;
    std::map<std::string, int64_t> c_cpid;
    std::map<std::string, int64_t> c_org;
    int64_t researchcount = 0;
    int64_t researchtotal = 0;
    int64_t interesttotal = 0;
    int64_t minttotal = 0;
    int64_t feetotal = 0;
    int64_t poscount = 0;
    int64_t emptyblockscount = 0;
    int64_t l_first = std::numeric_limits<int>::max();
    int64_t l_last = 0;
    unsigned int l_first_time = 0;
    unsigned int l_last_time = 0;
    unsigned int size_min_blk = std::numeric_limits<unsigned int>::max();
    unsigned int size_max_blk = 0;
    uint64_t size_sum_blk = 0;
    double diff_sum = 0;
    double diff_max = 0;
    double diff_min = std::numeric_limits<double>::max();
    int64_t super_count = 0;
    int64_t super_first_time = std::numeric_limits<int64_t>::max();
    int64_t super_last_time = 0;

    for (; cur && cur->nHeight >= lowheight && blockcount < maxblocks; cur = cur->pprev)
    {

        // cur is initialized to pIndexBest. This gets us to the starting point.
        if (cur->nHeight > highheight) continue;

        if (l_first > cur->nHeight)
        {
            l_first = cur->nHeight;
            l_first_time = cur->nTime;
        }
        if (l_last < cur->nHeight)
        {
            l_last = cur->nHeight;
            l_last_time = cur->nTime;
        }

        blockcount++;

        CBlock block;
        if (!block.ReadFromDisk(cur->nFile,cur->nBlockPos,true))
        {
            throw runtime_error("getblockstats: failed to read block");
        }

        assert(block.vtx.size() > 0);

        unsigned txcountinblock = 0;

        if (block.vtx.size() >= 2)
        {
            txcountinblock += block.vtx.size() - 2;

            if (block.vtx[1].IsCoinStake())
            {
                poscount++;
                double diff = GRC::GetDifficulty(cur);
                diff_sum += diff;
                diff_max = std::max(diff_max, diff);
                diff_min = std::min(diff_min, diff);
            }
            else
            {
                txcountinblock += 1;
            }
        }

        const GRC::MintSummary mint = block.GetMint();

        transactioncount += txcountinblock;
        emptyblockscount += (txcountinblock == 0);
        c_blockversion[block.nVersion]++;
        const Claim claim = block.GetClaim();
        c_cpid[claim.m_mining_id.ToString()]++;
        c_org[claim.m_organization]++;
        c_version[claim.m_client_version]++;
        researchtotal += claim.m_research_subsidy;
        interesttotal += claim.m_block_subsidy;
        researchcount += claim.HasResearchReward();
        minttotal += mint.m_total;
        feetotal += mint.m_fees;
        unsigned sizeblock = GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
        size_min_blk = std::min(size_min_blk,sizeblock);
        size_max_blk = std::max(size_max_blk,sizeblock);
        size_sum_blk += sizeblock;

        if (claim.ContainsSuperblock())
        {
            ++super_count;

            super_first_time = std::min<int64_t>(cur->nTime, super_first_time);
            super_last_time = std::max<int64_t>(cur->nTime, super_last_time);
        }
    }

    if (blockcount < 2)
    {
        throw runtime_error("getblockstats: Blockcount of scope is less than two.");
    }

    // general info
    {
        UniValue result(UniValue::VOBJ);
        result.pushKV("blocks", blockcount);
        result.pushKV("first_height", l_first);
        result.pushKV("last_height", l_last);
        result.pushKV("first_time", TimestampToHRDate(l_first_time));
        result.pushKV("last_time", TimestampToHRDate(l_last_time));
        result.pushKV("time_span_hour", (l_last_time - l_first_time) / (double) 3600);

        if (super_count)
        {
            result.pushKV("super_first_time", TimestampToHRDate(super_first_time));
            result.pushKV("super_last_time", TimestampToHRDate(super_last_time));
            result.pushKV("super_time_span_hour", (super_last_time - super_first_time) / (double) 3600);
        }

        result.pushKV("min_blocksizek", size_min_blk / (double) 1024);
        result.pushKV("max_blocksizek", size_max_blk / (double) 1024);
        result.pushKV("min_posdiff", diff_min);
        result.pushKV("max_posdiff", diff_max);
        result1.pushKV("general", result);
    }

    // counts
    {
        UniValue result(UniValue::VOBJ);
        result.pushKV("block", blockcount);
        result.pushKV("empty_block", emptyblockscount);
        result.pushKV("transaction", transactioncount);
        result.pushKV("proof_of_stake", poscount);
        result.pushKV("boincreward", researchcount);
        result.pushKV("super", super_count);
        result1.pushKV("counts", result);
    }

    // totals
    {
        UniValue result(UniValue::VOBJ);
        result.pushKV("block", blockcount);
        result.pushKV("research", ValueFromAmount(researchtotal));
        result.pushKV("interest", ValueFromAmount(interesttotal));
        result.pushKV("mint", ValueFromAmount(minttotal));
        result.pushKV("fees", ValueFromAmount(feetotal));
        result.pushKV("blocksizek", size_sum_blk / (double) 1024);
        result.pushKV("posdiff", diff_sum);
        result1.pushKV("totals", result);
    }

    // averages
    {
        UniValue result(UniValue::VOBJ);

        // check for zero researchcount and if so make research_average 0.
        int64_t research_average = researchcount ? researchtotal / researchcount : 0;

        result.pushKV("research", ValueFromAmount(research_average));
        result.pushKV("interest", ValueFromAmount(interesttotal / blockcount));
        result.pushKV("mint", ValueFromAmount(minttotal / blockcount));
        result.pushKV("fees", ValueFromAmount(feetotal / blockcount));

        double spacing_sec = (l_last_time - l_first_time) / (double) (blockcount - 1);
        result.pushKV("spacing_sec", spacing_sec);
        result.pushKV("block_per_day", 86400.0 / spacing_sec);

        // check for zero blockcount-emptyblockscount and if so make transaction average 0.
        double transaction_average = (blockcount - emptyblockscount) ?
                    transactioncount / (double) (blockcount - emptyblockscount) : 0;
        result.pushKV("transaction", transaction_average);

        result.pushKV("blocksizek", size_sum_blk / (double) blockcount / 1024.0);

        result.pushKV("posdiff", diff_sum / poscount);

        if (super_count > 1)
        {
            result.pushKV("super_spacing_hrs", (super_last_time - super_first_time) / ((double) super_count - 1) / 3600.0);
        }

        result1.pushKV("averages", result);
    }

    // wallet versions
    {
        UniValue result(UniValue::VOBJ);
        std::vector<PAIRTYPE(std::string, int64_t)> list;
        std::copy(c_version.begin(), c_version.end(), back_inserter(list));
        std::sort(list.begin(), list.end(), compare_second);

        for (auto const& item : list)
        {
            result.pushKV(item.first, item.second / (double) blockcount);
        }
        result1.pushKV("versions", result);
    }

    // cpids
    {
        UniValue result(UniValue::VOBJ);
        std::vector<PAIRTYPE(std::string, int64_t)> list;
        std::copy(c_cpid.begin(), c_cpid.end(), back_inserter(list));
        std::sort(list.begin(), list.end(), compare_second);

        for (auto const& item : list)
        {
            result.pushKV(item.first, item.second / (double) blockcount);
        }
        result1.pushKV("cpids", result);
    }

    // orgs
    {
        UniValue result(UniValue::VOBJ);
        std::vector<PAIRTYPE(std::string, int64_t)> list;
        std::copy(c_org.begin(), c_org.end(), back_inserter(list));
        std::sort(list.begin(), list.end(), compare_second);

        for (auto const& item : list)
        {
            result.pushKV(item.first, item.second / (double) blockcount);
        }
        result1.pushKV("orgs", result);
    }

    return result1;
}

UniValue rpc_getsupervotes(const UniValue& params, bool fHelp)
{
    if(fHelp || params.size() != 2 )
        throw runtime_error(
            "getsupervotes mode superblock\n"
            "Report votes for specified superblock.\n"
            "mode: 0=text, 1,2=json\n"
            "superblock: block hash or last= currently active, now= ongoing sb votes.\n"
            );
    long mode= RoundFromString(params[0].get_str(),0);
    CBlockIndex* pStart=NULL;
    long nMaxDepth_weight= 0;
    UniValue result1(UniValue::VOBJ);
    if("last"==params[1].get_str())
    {
        const uint64_t height = Quorum::CurrentSuperblock().m_height;
        if(!height)
        {
            result1.pushKV("error","No superblock loaded");
            return result1;
        }
        CBlockIndex* pblockindex = RPCBlockFinder.FindByHeight(height);
        if(!pblockindex)
        {
            result1.pushKV("height_cache", height);
            result1.pushKV("error","Superblock not found in block index");
            return result1;
        }
        if(!pblockindex->IsSuperblock())
        {
            result1.pushKV("height_cache", height);
            result1.pushKV("block_hash",pblockindex->GetBlockHash().GetHex());
            result1.pushKV("error","Superblock loaded not a Superblock");
            return result1;
        }
        pStart=pblockindex;

        /* sb votes are evaluated on content of the previous block */
        nMaxDepth_weight= pStart->nHeight -1;
    }
    else
    if("now"==params[1].get_str())
    {
        LOCK(cs_main);
        pStart=pindexBest;
        nMaxDepth_weight= pStart->nHeight;
    }
    else
    {
        LOCK(cs_main);
        uint256 hash = uint256S(params[1].get_str());

        if (mapBlockIndex.count(hash) == 0)
        {
            result1.pushKV("error","Block hash not found in block index");
            return result1;
        }


        CBlockIndex* pblockindex = mapBlockIndex[hash];

        if(!pblockindex->IsSuperblock())
        {
            result1.pushKV("block_hash",pblockindex->GetBlockHash().GetHex());
            result1.pushKV("error","Requested block is not a Superblock");
            return result1;
        }
        pStart = pblockindex;

        /* sb votes are evaluated on content of the previous block */
        nMaxDepth_weight= pStart->nHeight -1;
    }

    {
        UniValue info(UniValue::VOBJ);
        CBlock block;
        if(!block.ReadFromDisk(pStart->nFile,pStart->nBlockPos,true))
            throw runtime_error("failed to read block");
        //assert(block.vtx.size() > 0);
        const Claim claim = block.GetClaim();
        const Superblock& sb = *claim.m_superblock;

        info.pushKV("block_hash",pStart->GetBlockHash().GetHex());
        info.pushKV("height",pStart->nHeight);
        info.pushKV("quorum_hash", claim.m_quorum_hash.ToString());

        if (sb.m_version == 1) {
            info.pushKV("packed_size", (int64_t)sb.PackLegacy().size());
        } else {
            info.pushKV("packed_size", (int64_t)GetSerializeSize(sb, 1, 1));
        }

        info.pushKV("contract_hash", QuorumHash::Hash(sb).ToString());
        result1.pushKV("info", info );
    }

    UniValue votes(UniValue::VOBJ);

    long blockcount=0;
    long maxblocks= 200;

    CBlockIndex* cur = pStart;

    for( ; (cur
            &&( blockcount<maxblocks )
        );
        cur= cur->pprev, ++blockcount
        )
    {

        double diff = GRC::GetDifficulty(cur);
        signed int delta = 0;
        if(cur->pprev)
            delta = (cur->nTime - cur->pprev->nTime);

        CBlock block;
        if(!block.ReadFromDisk(cur->nFile,cur->nBlockPos,true))
            throw runtime_error("failed to read block");
        //assert(block.vtx.size() > 0);
        const Claim& claim = block.GetClaim();

        if(!claim.m_quorum_hash.Valid())
            continue;

        uint64_t stakeout = 0;
        if(block.vtx.size()>1 && block.vtx[1].vout.size()>1)
        {
            stakeout += block.vtx[1].vout[1].nValue;
            if(block.vtx[1].vout.size()>2)
                stakeout += block.vtx[1].vout[2].nValue;
            //could have used for loop
        }

        long distance= (nMaxDepth_weight-cur->nHeight)+10;
        double multiplier = 200;
        if (distance < 40) multiplier = 400;
        double weight = (1.0/distance)*multiplier;

        if(mode==0)
        {
            std::string line
            =     claim.m_quorum_hash.ToString()
            + "|"+RoundToString(weight/10.0,5)
            + "|"+claim.m_organization
            + "|"+claim.m_client_version
            + "|"+RoundToString(diff,3)
            + "|"+RoundToString(delta,0)
            + "|"+claim.m_mining_id.ToString()
            ;
            votes.pushKV(ToString(cur->nHeight), line );
        }
        else
        {
            UniValue result2(UniValue::VOBJ);
            result2.pushKV("quorum_hash", claim.m_quorum_hash.ToString());
            result2.pushKV("weight", weight );
            result2.pushKV("cpid", cur->GetMiningId().ToString() );
            result2.pushKV("organization", claim.m_organization);
            result2.pushKV("cversion", claim.m_client_version);
            if(mode>=2)
            {
                result2.pushKV("difficulty", diff );
                result2.pushKV("delay", delta );
                result2.pushKV("hash", cur->GetBlockHash().GetHex() );
                result2.pushKV("stakeout", (double) stakeout / COIN );
            }
            votes.pushKV(ToString(cur->nHeight), result2 );
        }

    }
    result1.pushKV("votes", votes );

    return result1;
}

UniValue rpc_exportstats(const UniValue& params, bool fHelp)
{
    if(fHelp)
        throw runtime_error(
            "exportstats1 [maxblocks aggregate [endblock]] \n");
    /* count, high */
    long endblock= INT_MAX;
    long maxblocks= 805;
    int  smoothing= 23;
    if(params.size()>=2)
    {
        maxblocks= RoundFromString(params[0].get_str(),0);
        smoothing= RoundFromString(params[1].get_str(),0);
    }
    if(params.size()>=3)
        endblock= RoundFromString(params[2].get_str(),0);
    if( (smoothing<1) || (smoothing%2) )
        throw runtime_error(
            "smoothing must be even positive\n");
    /*
    if( maxblocks % smoothing )
        throw runtime_error(
            "maxblocks not a smoothing multiple\n");
    */
    CBlockIndex* cur;
    UniValue result1(UniValue::VOBJ);
    {
        LOCK(cs_main);
        cur= pindexBest;
    }

    double sum_diff = 0;
    double min_diff = INT_MAX;
    double max_diff = 0;
    double sum_spacing = 0;
    double min_spacing = INT_MAX;
    double max_spacing = 0;
    double sum_size = 0;
    double min_size = INT_MAX;
    double max_size = 0;
    double sum_research = 0;
    double max_research = 0;
    double sum_interest = 0;
    double max_interest = 0;
    double sum_magnitude = 0;
    unsigned long cnt_empty = 0;
    unsigned long cnt_investor = 0;
    unsigned long cnt_trans = 0;
    unsigned long cnt_research = 0;
    unsigned long cnt_quorumvote = 0;
    unsigned long cnt_quorumcurr = 0;
    unsigned long cnt_contract = 0;

    int64_t blockcount = 0;
    unsigned long points = 0;
    double samples = 0; /* this is double for easy division */
    fsbridge::ofstream Output;
    fs::path o_path = GetDataDir() / "reports" / ( "export_" + std::to_string(GetTime()) + ".txt" );
    fs::create_directories(o_path.parent_path());
    Output.open (o_path);
    Output.imbue(std::locale::classic());
    Output << std::fixed << std::setprecision(4);
    Output << "#midheight  ave_diff min_diff max_diff  "
    "ave_spacing min_spacing max_spacing  ave_size min_size max_size  "
    "ave_research avenz_research max_research  ave_interest max_interest  "
    "fra_empty cnt_empty  fra_investor cnt_investor  ave_trans avenz_trans cnt_trans  "
    "fra_research cnt_research  fra_contract cnt_contract  "
    "fra_quorumvote cnt_quorumvote fra_quorumcur cnt_quorumcurr  "
    "avenz_magnitude  \n";

    while( (blockcount < maxblocks) && cur && cur->pprev )
    {
        if(cur->nHeight>endblock)
            continue;

        double i_diff = GRC::GetDifficulty(cur);
        sum_diff= sum_diff + i_diff;
        min_diff=std::min(min_diff,i_diff);
        max_diff=std::max(max_diff,i_diff);

        const double i_spacing = (double)cur->nTime - (double)cur->pprev->nTime;
        sum_spacing= sum_spacing + i_spacing;
        min_spacing=std::min(min_spacing,i_spacing);
        max_spacing=std::max(max_spacing,i_spacing);

        cnt_investor += !! (cur->nFlags & CBlockIndex::INVESTOR_CPID);
        cnt_contract += !! cur->IsContract();

        CBlock block;
        if(!block.ReadFromDisk(cur->nFile,cur->nBlockPos,true))
            throw runtime_error("failed to read block");

        cnt_trans += block.vtx.size()-2; /* 2 transactions are special */
        cnt_empty += ( block.vtx.size()<=2 );
        double i_size = GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
        sum_size= sum_size + i_size;
        min_size=std::min(min_size,i_size);
        max_size=std::max(max_size,i_size);

        const Claim& claim = block.GetClaim();
        cnt_quorumvote += (claim.m_quorum_hash.Valid());
        if (claim.m_quorum_hash.Valid()
            && claim.m_quorum_hash != "d41d8cd98f00b204e9800998ecf8427e")
        {
            cnt_quorumcurr += 1;
        }

        const double i_research = claim.m_research_subsidy;
        sum_research= sum_research + i_research;
        max_research=std::max(max_research,i_research);
        const double i_interest = claim.m_block_subsidy;
        sum_interest= sum_interest + i_interest;
        max_interest=std::max(max_interest,i_interest);

        if(i_research>0)
        {
            const double i_magnitude = cur->Magnitude();
            sum_magnitude= sum_magnitude + i_magnitude;
            cnt_research += 1;
        }

        blockcount++;
        samples++;
        if(samples>=smoothing)
        {
            int midheight = cur->nHeight + (smoothing/2);
            double samples_w_cpid = samples - cnt_investor;
            if(samples == cnt_investor)
                samples_w_cpid = std::numeric_limits<double>::max();
            double samples_w_research = cnt_research;
            if(cnt_research==0)
                samples_w_research = std::numeric_limits<double>::max();
            double samples_w_trans = (samples-cnt_empty);
            if(samples==cnt_empty)
                samples_w_trans = std::numeric_limits<double>::max();
            points++;
            Output << midheight << "  ";

            Output << (sum_diff / samples) << " " << min_diff << " " << max_diff << "  ";
            Output << (sum_spacing / samples) << " " << min_spacing << " " << max_spacing << "  ";
            Output << (sum_size / samples) << " " << min_size << " " << max_size << "  ";
            Output << (sum_research / samples) << " " << (sum_research / samples_w_research) << " " << max_research << "  ";
            Output << (sum_interest / samples) << " " << max_interest << "  ";
            Output << (cnt_empty / samples) << " " << cnt_empty << "  ";
            Output << (cnt_investor / samples) << " " << cnt_investor << "  ";
            Output << (cnt_trans / samples) << " " << (cnt_trans / samples_w_trans) << " " << cnt_trans << "  ";
            Output << (cnt_research / samples) << " " << cnt_research << "  ";
            Output << (cnt_contract / samples) << " " << cnt_contract << "  ";
            Output << (cnt_quorumvote / samples) << " " << cnt_quorumvote << "  ";
            Output << (cnt_quorumcurr / samples) << " " << cnt_quorumcurr << "  ";
            Output << (sum_magnitude / samples_w_cpid) << "  ";
            // missing: trans, empty, size, quorum

            Output << "\n";
            samples = 0;
            sum_diff = 0;
            min_diff = INT_MAX;
            max_diff = 0;
            cnt_empty = 0;
            cnt_investor = 0;
            sum_spacing = 0;
            min_spacing = INT_MAX;
            max_spacing = 0;
            sum_size = 0;
            min_size = INT_MAX;
            max_size = 0;
            cnt_trans = 0;
            sum_research = 0;
            max_research = 0;
            sum_interest = 0;
            max_interest = 0;
            sum_magnitude = 0;
            cnt_research = 0;
            cnt_quorumvote = 0;
            cnt_quorumcurr = 0;
            cnt_contract = 0;
        }
        /* This is very important */
        cur = cur->pprev;
    }

    result1.pushKV("file", o_path.string());
    result1.pushKV("points",(uint64_t)points);
    result1.pushKV("smoothing",smoothing);
    result1.pushKV("blockcount",blockcount);
    Output.close();
    return result1;
}

UniValue rpc_getrecentblocks(const UniValue& params, bool fHelp)
{
    if(fHelp || params.size() < 1 || params.size() > 3 )
        throw runtime_error(
            "getrecentblocks detail count\n"
            "Show list of <count> recent block hashes and optional details.\n"
            "detail 0 -> height and hash dict\n"
            "detail 1,2 -> text data from blockindex\n"
            "detail 20,21 -> text data from index and block\n"
            "detail 100 -> json from index\n"
            "detail 120 -> json from index and block\n"
        );

    long detail= RoundFromString(params[0].get_str(),0);
    long blockcount=0;
    long maxblocks= RoundFromString(params[1].get_str(),0);

    CBlockIndex* cur;
    UniValue result1(UniValue::VOBJ);
    {
        LOCK(cs_main);
        cur= pindexBest;
    }

    for( ; (cur
            &&( blockcount<maxblocks )
        );
        cur= cur->pprev, ++blockcount
        )
    {
        /* detail:
            0 height: hash
            1 height: hash diff spacing, flg
            2 height: hash diff spacing, flg, cpid, R, I, F
            20 height: hash diff spacing, flg, org, ver
            21 height: hash diff spacing, flg, org, ver, cpid, quorum

            100 json
        */

        double diff = GRC::GetDifficulty(cur);
        signed int delta = 0;
        if(cur->pprev)
            delta = (cur->nTime - cur->pprev->nTime);

        UniValue result2(UniValue::VOBJ);
        std::string line = cur->GetBlockHash().GetHex();

        if(detail<100)
        {
            if(detail>=1)
            {
                line+="<|>"+RoundToString(diff,4)
                    + "<|>"+ToString(delta);

                line+= "<|>"
                    + std::string((cur->IsSuperblock()?"S":(cur->IsContract()?"C":"-")))
                    + (cur->IsUserCPID()? (cur->ResearchSubsidy()>0? "R": "U"): "I")
                    //+ (cur->GeneratedStakeModifier()? "M": "-")
                    ;
            }

            if(detail>=2 && detail<20)
            {
                line+="<|>"+cur->GetMiningId().ToString()
                    + "<|>"+FormatMoney(cur->ResearchSubsidy());
            }
        }
        else
        {
            result2.pushKV("hash", line );
            result2.pushKV("difficulty", diff );
            result2.pushKV("deltatime", (int64_t)delta );
            result2.pushKV("issuperblock", cur->IsSuperblock());
            result2.pushKV("iscontract", cur->IsContract());
            result2.pushKV("ismodifier", cur->GeneratedStakeModifier());
            result2.pushKV("cpid", cur->GetMiningId().ToString() );
            result2.pushKV("research", ValueFromAmount(cur->ResearchSubsidy()));
            result2.pushKV("magnitude", cur->Magnitude());
        }

        if( (detail<100 && detail>=20) || (detail>=120) )
        {
            CBlock block;
            if(!block.ReadFromDisk(cur->nFile,cur->nBlockPos,true))
                throw runtime_error("failed to read block");
            //assert(block.vtx.size() > 0);
            const Claim& claim = block.GetClaim();

            if(detail<100)
            {
                if(detail>=20)
                {
                    line+="<|>"+claim.m_organization
                        + "<|>"+claim.m_client_version
                        + "<|>"+ToString(block.vtx.size()-2);
                }
                if(detail==21)
                {
                    line+="<|>"+claim.m_mining_id.ToString()
                        + "<|>"+(claim.m_quorum_hash.Valid() ? claim.m_quorum_hash.ToString() : "--");
                }
            }
            else
            {
                result2.pushKV("interest", ValueFromAmount(claim.m_block_subsidy));
                result2.pushKV("organization", claim.m_organization);
                result2.pushKV("cversion", claim.m_client_version);
                result2.pushKV("quorum_hash", claim.m_quorum_hash.ToString());
                result2.pushKV("superblocksize", claim.m_quorum_hash.ToString());
                result2.pushKV("vtxsz", (int64_t)block.vtx.size() );
            }
        }
        if(detail<100)
            result1.pushKV(ToString(cur->nHeight), line );
        else
            result1.pushKV(ToString(cur->nHeight), result2 );

    }
    return result1;
}
