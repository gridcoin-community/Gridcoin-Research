// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// and The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "rpcserver.h"
#include "kernel.h"
#include "block.h"
#include "txdb.h"
#include "beacon.h"
#include "appcache.h"
#include "util.h"

#include <boost/filesystem.hpp>
#include <iostream>
#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <algorithm>

#include <univalue.h>

using namespace std;

extern BlockFinder RPCBlockFinder;

// Brod
static bool compare_second(const pair<std::string, long>  &p1, const pair<std::string, long> &p2)
{
    return p1.second > p2.second;
}

UniValue rpc_getblockstats(const UniValue& params, bool fHelp)
{
    if(fHelp || params.size() < 1 || params.size() > 3 )
        throw runtime_error(
            "getblockstats mode [startheight [endheight]]\n"
            "\n"
            "Show stats on what wallets and cpids staked recent blocks.\n");
    long mode= params[0].get_int();
    (void)mode; //TODO
    long lowheight= 0;
    long highheight= INT_MAX;
    long maxblocks= 14000;
    if (mode==0)
    {
        if(params.size()>=2)
        {
            lowheight= params[1].get_int();
            maxblocks= INT_MAX;
        }
        if(params.size()>=3)
            highheight= params[2].get_int();
    }
    else if(mode==1)
    {
        /* count highheight */
        maxblocks= 30000;
        if(params.size()>=2)
            maxblocks= params[1].get_int();
        if(params.size()>=3)
            highheight= params[2].get_int();
    }
    else throw runtime_error("getblockstats: Invalid mode specified");
    CBlockIndex* cur;
    UniValue result1(UniValue::VOBJ);
    {
        LOCK(cs_main);
        cur= pindexBest;
    }
    int64_t blockcount = 0;
    int64_t transactioncount = 0;
    std::map<int,long> c_blockversion;
    std::map<std::string,long> c_version;
    std::map<std::string,long> c_cpid;
    std::map<std::string,long> c_org;
    int64_t researchcount = 0;
    double researchtotal = 0;
    double interesttotal = 0;
    int64_t minttotal = 0;
    //int64_t stakeinputtotal = 0;
    int64_t poscount = 0;
    int64_t emptyblockscount = 0;
    int64_t l_first = INT_MAX;
    int64_t l_last = 0;
    unsigned int l_first_time = 0;
    unsigned int l_last_time = 0;
    unsigned size_min_blk=INT_MAX;
    unsigned size_max_blk=0;
    uint64_t size_sum_blk=0;
    double diff_sum = 0;
    double diff_max=0;
    double diff_min=INT_MAX;
    int64_t super_count = 0;
    for( ; (cur
            &&( cur->nHeight>=lowheight )
            &&( blockcount<maxblocks )
        );
        cur= cur->pprev
        )
    {
        if(cur->nHeight>highheight)
            continue;
        if(l_first>cur->nHeight)
        {
            l_first=cur->nHeight;
            l_first_time=cur->nTime;
        }
        if(l_last<cur->nHeight)
        {
            l_last=cur->nHeight;
            l_last_time=cur->nTime;
        }
        blockcount++;
        CBlock block;
        if(!block.ReadFromDisk(cur->nFile,cur->nBlockPos,true))
            throw runtime_error("failed to read block");
        assert(block.vtx.size() > 0);
        unsigned txcountinblock = 0;
        if(block.vtx.size()>=2)
        {
            txcountinblock+=block.vtx.size()-2;
            if(block.vtx[1].IsCoinStake())
            {
                poscount++;
                //stakeinputtotal+=block.vtx[1].vin[0].nValue;
                double diff = GetDifficulty(cur);
                diff_sum += diff;
                diff_max=std::max(diff_max,diff);
                diff_min=std::min(diff_min,diff);
            }
            else
                txcountinblock+=1;
        }
        transactioncount+=txcountinblock;
        emptyblockscount+=(txcountinblock==0);
        c_blockversion[block.nVersion]++;
        MiningCPID bb = DeserializeBoincBlock(block.vtx[0].hashBoinc, block.nVersion);
        c_cpid[bb.cpid]++;
        c_org[bb.Organization]++;
        c_version[bb.clientversion]++;
        researchtotal+=bb.ResearchSubsidy;
        interesttotal+=bb.InterestSubsidy;
        researchcount+=(bb.ResearchSubsidy>0.001);
        minttotal+=cur->nMint;
        unsigned sizeblock = GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
        size_min_blk=std::min(size_min_blk,sizeblock);
        size_max_blk=std::max(size_max_blk,sizeblock);
        size_sum_blk+=sizeblock;
        super_count += (bb.superblock.length()>20);
    }

    {
        UniValue result(UniValue::VOBJ);
        result.pushKV("blocks", blockcount);
        result.pushKV("first_height", l_first);
        result.pushKV("last_height", l_last);
        result.pushKV("first_time", TimestampToHRDate(l_first_time));
        result.pushKV("last_time", TimestampToHRDate(l_last_time));
        result.pushKV("time_span_hour", ((double)l_last_time-(double)l_first_time)/(double)3600);
        result.pushKV("min_blocksizek", size_min_blk/(double)1024);
        result.pushKV("max_blocksizek", size_max_blk/(double)1024);
        result.pushKV("min_posdiff", diff_min);
        result.pushKV("max_posdiff", diff_max);
        result1.pushKV("general", result);
    }
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
    {
        UniValue result(UniValue::VOBJ);
        result.pushKV("block", blockcount);
        result.pushKV("research", researchtotal);
        result.pushKV("interest", interesttotal);
        result.pushKV("mint", minttotal/(double)COIN);
        //result.pushKV("stake_input", stakeinputtotal/(double)COIN);
        result.pushKV("blocksizek", size_sum_blk/(double)1024);
        result.pushKV("posdiff", diff_sum);
        result1.pushKV("totals", result);
    }
    {
        UniValue result(UniValue::VOBJ);
        result.pushKV("research", researchtotal/(double)researchcount);
        result.pushKV("interest", interesttotal/(double)blockcount);
        result.pushKV("mint", (minttotal/(double)blockcount)/(double)COIN);
        //result.pushKV("stake_input", (stakeinputtotal/(double)poscount)/(double)COIN);
        result.pushKV("spacing_sec", ((double)l_last_time-(double)l_first_time)/(double)blockcount);
        result.pushKV("block_per_day", ((double)blockcount*86400.0)/((double)l_last_time-(double)l_first_time));
        result.pushKV("transaction", transactioncount/(double)(blockcount-emptyblockscount));
        result.pushKV("blocksizek", size_sum_blk/(double)blockcount/(double)1024);
        result.pushKV("posdiff", diff_sum/(double)poscount);
        if (super_count > 0)
            result.pushKV("super_spacing_hrs", ((l_last_time-l_first_time)/(double)super_count)/3600.0);

        result1.pushKV("averages", result);
    }
    {
        UniValue result(UniValue::VOBJ);
        std::vector<PAIRTYPE(std::string, long)> list;
        std::copy(c_version.begin(), c_version.end(), back_inserter(list));
        std::sort(list.begin(),list.end(),compare_second);
        for (auto const& item : list)
        {
            result.pushKV(item.first, item.second/(double)blockcount);
        }
        result1.pushKV("versions", result);
    }
    {
        UniValue result(UniValue::VOBJ);
        std::vector<PAIRTYPE(std::string, long)> list;
        std::copy(c_cpid.begin(), c_cpid.end(), back_inserter(list));
        std::sort(list.begin(),list.end(),compare_second);
        int limit=64;
        for (auto const& item : list)
        {
            if(!(limit--)) break;
            result.pushKV(item.first, item.second/(double)blockcount);
        }
        result1.pushKV("cpids", result);
    }
    {
        UniValue result(UniValue::VOBJ);
        std::vector<PAIRTYPE(std::string, long)> list;
        std::copy(c_org.begin(), c_org.end(), back_inserter(list));
        std::sort(list.begin(),list.end(),compare_second);
        int limit=64;
        for (auto const& item : list)
        {
            if(!(limit--)) break;
            result.pushKV(item.first, item.second/(double)blockcount);
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
        std::string sheight= ReadCache(Section::SUPERBLOCK, "block_number").value;
        long height= RoundFromString(sheight,0);
        if(!height)
        {
            result1.pushKV("error","No superblock loaded");
            return result1;
        }
        CBlockIndex* pblockindex = RPCBlockFinder.FindByHeight(height);
        if(!pblockindex)
        {
            result1.pushKV("height_cache",sheight);
            result1.pushKV("error","Superblock not found in block index");
            return result1;
        }
        if(!pblockindex->nIsSuperBlock)
        {
            result1.pushKV("height_cache",sheight);
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
        std::string strHash = params[1].get_str();
        uint256 hash(strHash);

        if (mapBlockIndex.count(hash) == 0)
        {
            result1.pushKV("error","Block hash not found in block index");
            return result1;
        }


        CBlockIndex* pblockindex = mapBlockIndex[hash];

        if(!pblockindex->nIsSuperBlock)
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
        MiningCPID bb = DeserializeBoincBlock(block.vtx[0].hashBoinc, block.nVersion);
        info.pushKV("block_hash",pStart->GetBlockHash().GetHex());
        info.pushKV("height",pStart->nHeight);
        info.pushKV("neuralhash", bb.NeuralHash );
        std::string superblock = UnpackBinarySuperblock(bb.superblock);
        std::string neural_hash = GetQuorumHash(superblock);
        info.pushKV("contract_size", (int64_t)superblock.size() );
        info.pushKV("packed_size", (int64_t)bb.superblock.size() );
        info.pushKV("contract_hash", neural_hash );
        result1.pushKV("info", info );
    }

    UniValue votes(UniValue::VOBJ);
    std::map<std::string,double> tally;

    long blockcount=0;
    long maxblocks= 200;

    CBlockIndex* cur = pStart;

    for( ; (cur
            &&( blockcount<maxblocks )
        );
        cur= cur->pprev, ++blockcount
        )
    {

        double diff = GetDifficulty(cur);
        signed int delta = 0;
        if(cur->pprev)
            delta = (cur->nTime - cur->pprev->nTime);

        CBlock block;
        if(!block.ReadFromDisk(cur->nFile,cur->nBlockPos,true))
            throw runtime_error("failed to read block");
        //assert(block.vtx.size() > 0);
        MiningCPID bb = DeserializeBoincBlock(block.vtx[0].hashBoinc, block.nVersion);

        if(bb.NeuralHash.empty())
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

        /* Tally votes */
        tally[bb.NeuralHash] += weight;

        if(mode==0)
        {
            std::string line
            =     bb.NeuralHash
            + "|"+RoundToString(weight/10.0,5)
            + "|"+bb.Organization
            + "|"+bb.clientversion
            + "|"+RoundToString(diff,3)
            + "|"+RoundToString(delta,0)
            + "|"+bb.cpid
            ;
            votes.pushKV(ToString(cur->nHeight), line );
        }
        else
        {
            UniValue result2(UniValue::VOBJ);
            result2.pushKV("neuralhash", bb.NeuralHash );
            result2.pushKV("weight", weight );
            result2.pushKV("cpid", cur->GetCPID() );
            result2.pushKV("organization", bb.Organization );
            result2.pushKV("cversion", bb.clientversion );
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
            "exportstats1 [maxblocks agregate [endblock]] \n");
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
    unsigned long cnt_neuralvote = 0;
    unsigned long cnt_neuralcurr = 0;
    unsigned long cnt_contract = 0;

    int64_t blockcount = 0;
    unsigned long points = 0;
    double samples = 0; /* this is double for easy division */
    std::ofstream Output;
    boost::filesystem::path o_path = GetDataDir() / "reports" / ( "export_" + std::to_string(GetTime()) + ".txt" );
    boost::filesystem::create_directories(o_path.parent_path());
    Output.open (o_path.string().c_str());
    Output.imbue(std::locale::classic());
    Output << std::fixed << std::setprecision(4);
    Output << "#midheight  ave_diff min_diff max_diff  "
    "ave_spacing min_spacing max_spacing  ave_size min_size max_size  "
    "ave_research avenz_research max_research  ave_interest max_interest  "
    "fra_empty cnt_empty  fra_investor cnt_investor  ave_trans avenz_trans cnt_trans  "
    "fra_research cnt_research  fra_contract cnt_contract  "
    "fra_neuralvote cnt_neuralvote fra_neuralcur cnt_neuralcurr  "
    "avenz_magnitude  \n";

    while( (blockcount < maxblocks) && cur && cur->pprev )
    {
        if(cur->nHeight>endblock)
            continue;

        double i_diff = GetDifficulty(cur);
        sum_diff= sum_diff + i_diff;
        min_diff=std::min(min_diff,i_diff);
        max_diff=std::max(max_diff,i_diff);

        const double i_spacing = (double)cur->nTime - (double)cur->pprev->nTime;
        sum_spacing= sum_spacing + i_spacing;
        min_spacing=std::min(min_spacing,i_spacing);
        max_spacing=std::max(max_spacing,i_spacing);

        cnt_investor += !! (cur->nFlags & CBlockIndex::INVESTOR_CPID);
        cnt_contract += !! cur->nIsContract;

        CBlock block;
        if(!block.ReadFromDisk(cur->nFile,cur->nBlockPos,true))
            throw runtime_error("failed to read block");

        cnt_trans += block.vtx.size()-2; /* 2 transactions are special */
        cnt_empty += ( block.vtx.size()<=2 );
        double i_size = GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
        sum_size= sum_size + i_size;
        min_size=std::min(min_size,i_size);
        max_size=std::max(max_size,i_size);

        const MiningCPID bb = DeserializeBoincBlock(block.vtx[0].hashBoinc, block.nVersion);
        cnt_neuralvote += (bb.NeuralHash.size()>0);
        if( bb.CurrentNeuralHash.size()>0
            && bb.CurrentNeuralHash != "d41d8cd98f00b204e9800998ecf8427e"
            && bb.CurrentNeuralHash != "TOTAL_VOTES" )
        {
            cnt_neuralcurr += 1;
        }

        const double i_research = bb.ResearchSubsidy;
        sum_research= sum_research + i_research;
        max_research=std::max(max_research,i_research);
        const double i_interest = bb.InterestSubsidy;
        sum_interest= sum_interest + i_interest;
        max_interest=std::max(max_interest,i_interest);

        if(i_research>0)
        {
            const double i_magnitude = cur->nMagnitude;
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
            Output << (cnt_neuralvote / samples) << " " << cnt_neuralvote << "  ";
            Output << (cnt_neuralcurr / samples) << " " << cnt_neuralcurr << "  ";
            Output << (sum_magnitude / samples_w_cpid) << "  ";
            // missing: trans, empty, size, neural

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
            cnt_neuralvote = 0;
            cnt_neuralcurr = 0;
            cnt_contract = 0;
        }
        /* This is wery important */
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
            21 height: hash diff spacing, flg, org, ver, cpid, neural

            100 json
        */

        double diff = GetDifficulty(cur);
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
                    + std::string((cur->nIsSuperBlock?"S":(cur->nIsContract?"C":"-")))
                    + (cur->IsUserCPID()? (cur->nResearchSubsidy>0? "R": "U"): "I")
                    //+ (cur->GeneratedStakeModifier()? "M": "-")
                    ;
            }

            if(detail>=2 && detail<20)
            {
                line+="<|>"+cur->GetCPID()
                    + "<|>"+RoundToString(cur->nResearchSubsidy,4)
                    + "<|>"+RoundToString(cur->nInterestSubsidy,4);
            }
        }
        else
        {
            result2.pushKV("hash", line );
            result2.pushKV("difficulty", diff );
            result2.pushKV("deltatime", (int64_t)delta );
            result2.pushKV("issuperblock", (bool)cur->nIsSuperBlock );
            result2.pushKV("iscontract", (bool)cur->nIsContract );
            result2.pushKV("ismodifier", (bool)cur->GeneratedStakeModifier() );
            result2.pushKV("cpid", cur->GetCPID() );
            result2.pushKV("research", cur->nResearchSubsidy );
            result2.pushKV("interest", cur->nInterestSubsidy );
            result2.pushKV("magnitude", cur->nMagnitude );
        }

        if( (detail<100 && detail>=20) || (detail>=120) )
        {
            CBlock block;
            if(!block.ReadFromDisk(cur->nFile,cur->nBlockPos,true))
                throw runtime_error("failed to read block");
            //assert(block.vtx.size() > 0);
            MiningCPID bb = DeserializeBoincBlock(block.vtx[0].hashBoinc, block.nVersion);

            if(detail<100)
            {
                if(detail>=20)
                {
                    line+="<|>"+bb.Organization
                        + "<|>"+bb.clientversion
                        + "<|>"+ToString(block.vtx.size()-2);
                }
                if(detail==21)
                {
                    line+="<|>"+bb.cpid
                        + "<|>"+(bb.NeuralHash.empty()? "--" : bb.NeuralHash);
                }
            }
            else
            {
                result2.pushKV("organization", bb.Organization );
                result2.pushKV("cversion", bb.clientversion );
                result2.pushKV("neuralhash", bb.NeuralHash );
                result2.pushKV("superblocksize", bb.NeuralHash );
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
