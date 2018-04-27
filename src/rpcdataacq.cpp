// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// and The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "bitcoinrpc.h"
#include "cpid.h"
#include "kernel.h"
#include "init.h" // for pwalletMain
#include "block.h"
#include "txdb.h"
#include "beacon.h"
#include "util.h"
#include "neuralnet.h"
#include "grcrestarter.h"
#include "backup.h"
#include "appcache.h"
#include "tally.h"
#include "contract/polls.h"
#include "contract/contract.h"

#include <boost/filesystem.hpp>
#include <iostream>
#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <algorithm>
using namespace json_spirit;
using namespace std;


// Brod
static bool compare_second(const pair<std::string, long>  &p1, const pair<std::string, long> &p2)
{
    return p1.second > p2.second;
}

Value rpc_getblockstats(const json_spirit::Array& params, bool fHelp)
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
    Object result1;
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
        unsigned sizeblock = block.GetSerializeSize(SER_NETWORK, PROTOCOL_VERSION);
        size_min_blk=std::min(size_min_blk,sizeblock);
        size_max_blk=std::max(size_max_blk,sizeblock);
        size_sum_blk+=sizeblock;
        super_count += (bb.superblock.length()>20);
    }

    {
        Object result;
        result.push_back(Pair("blocks", blockcount));
        result.push_back(Pair("first_height", l_first));
        result.push_back(Pair("last_height", l_last));
        result.push_back(Pair("first_time", TimestampToHRDate(l_first_time)));
        result.push_back(Pair("last_time", TimestampToHRDate(l_last_time)));
        result.push_back(Pair("time_span_hour", ((double)l_last_time-(double)l_first_time)/(double)3600));
        result.push_back(Pair("min_blocksizek", size_min_blk/(double)1024));
        result.push_back(Pair("max_blocksizek", size_max_blk/(double)1024));
        result.push_back(Pair("min_posdiff", diff_min));
        result.push_back(Pair("max_posdiff", diff_max));
        result1.push_back(Pair("general", result));
    }
    {
        Object result;
        result.push_back(Pair("block", blockcount));
        result.push_back(Pair("empty_block", emptyblockscount));
        result.push_back(Pair("transaction", transactioncount));
        result.push_back(Pair("proof_of_stake", poscount));
        result.push_back(Pair("boincreward", researchcount));
        result.push_back(Pair("super", super_count));
        result1.push_back(Pair("counts", result));
    }
    {
        Object result;
        result.push_back(Pair("block", blockcount));
        result.push_back(Pair("research", researchtotal));
        result.push_back(Pair("interest", interesttotal));
        result.push_back(Pair("mint", minttotal/(double)COIN));
        //result.push_back(Pair("stake_input", stakeinputtotal/(double)COIN));
        result.push_back(Pair("blocksizek", size_sum_blk/(double)1024));
        result.push_back(Pair("posdiff", diff_sum));
        result1.push_back(Pair("totals", result));
    }
    {
        Object result;
        result.push_back(Pair("research", researchtotal/(double)researchcount));
        result.push_back(Pair("interest", interesttotal/(double)blockcount));
        result.push_back(Pair("mint", (minttotal/(double)blockcount)/(double)COIN));
        //result.push_back(Pair("stake_input", (stakeinputtotal/(double)poscount)/(double)COIN));
        result.push_back(Pair("spacing_sec", ((double)l_last_time-(double)l_first_time)/(double)blockcount));
        result.push_back(Pair("block_per_day", ((double)blockcount*86400.0)/((double)l_last_time-(double)l_first_time)));
        result.push_back(Pair("transaction", transactioncount/(double)(blockcount-emptyblockscount)));
        result.push_back(Pair("blocksizek", size_sum_blk/(double)blockcount/(double)1024));
        result.push_back(Pair("posdiff", diff_sum/(double)poscount));
        result.push_back(Pair("super_spacing_hrs", (((double)l_last_time-(double)l_first_time)/(double)super_count)/3600.0));
        result1.push_back(Pair("averages", result));
    }
    {
        Object result;
        std::vector<PAIRTYPE(std::string, long)> list;
        std::copy(c_version.begin(), c_version.end(), back_inserter(list));
        std::sort(list.begin(),list.end(),compare_second);
        for (auto const& item : list)
        {
            result.push_back(Pair(item.first, item.second/(double)blockcount));
        }
        result1.push_back(Pair("versions", result));
    }
    {
        Object result;
        std::vector<PAIRTYPE(std::string, long)> list;
        std::copy(c_cpid.begin(), c_cpid.end(), back_inserter(list));
        std::sort(list.begin(),list.end(),compare_second);
        int limit=64;
        for (auto const& item : list)
        {
            if(!(limit--)) break;
            result.push_back(Pair(item.first, item.second/(double)blockcount));
        }
        result1.push_back(Pair("cpids", result));
    }
    {
        Object result;
        std::vector<PAIRTYPE(std::string, long)> list;
        std::copy(c_org.begin(), c_org.end(), back_inserter(list));
        std::sort(list.begin(),list.end(),compare_second);
        int limit=64;
        for (auto const& item : list)
        {
            if(!(limit--)) break;
            result.push_back(Pair(item.first, item.second/(double)blockcount));
        }
        result1.push_back(Pair("orgs", result));
    }
    return result1;
}
