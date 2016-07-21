// Copyright (c) 2014 - Gridcoin

#include <boost/assign/list_of.hpp> // for 'map_list_of()'
#include <boost/foreach.hpp>
#include "sql.h"
#include "txdb.h"
#include "main.h"
#include "uint256.h"

static const int nSqlQuerySpan = 100;

namespace SQL
{
    typedef std::map<int, uint256> MapCheckpoints;
    // Gridcoin : SQL Message (centrally broadcasted)
    uint256 hashSql = 0;
    CCriticalSection cs_sql;
    // ppcoin: get last synchronized checkpoint
    CBlockIndex* GetLastSyncCheckpoint()
    {
        LOCK(cs_sql);
        if (!mapBlockIndex.count(hashSql))
		{
            error("GetSyncCheckpoint: block index missing for current sync-checkpoint %s", hashSql.ToString().c_str());
		}
        else
		{
            return mapBlockIndex[hashSql];
		}
        return NULL;
    }

    
    
    void AskForSqlRow(CNode* pfrom)
    {
        LOCK(cs_sql);
        if (pfrom && hashSql != 0)
		{
            pfrom->AskFor(CInv(MSG_BLOCK, hashSql));
		}

    }

    bool SendSqlRow(uint256 urow)
    {
        CSQL csql;
        //csql.hashsql = urow;
        CDataStream sMsg(SER_NETWORK, PROTOCOL_VERSION);
        sMsg << (CSQLbase)csql;
		csql.vchMsg = std::vector<unsigned char>(sMsg.begin(), sMsg.end());
        // Relay sql row to every node
        {
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodes)
			{
                csql.RelayTo(pnode);
			}
        }
        return true;
    }

}


//const std::string CSQL::sData = "";
//std::string CSQL::sData2 = "";


bool CSQL::ProcessSQLRow(CNode* pfrom)
{
   
    LOCK(SQL::cs_sql);
	//Loop through sql rows, look for missing data, ask for missing data

    if (!mapBlockIndex.count(hashSql))
    {
        // We haven't received the sql block

        if (pfrom)
        {
            pfrom->PushGetBlocks(pindexBest, hashSql, false);
            // ask directly as well in case rejected earlier by duplicate
            // proof-of-stake because getblocks may not get it this time
            pfrom->AskFor(CInv(MSG_BLOCK, mapOrphanBlocks.count(hashSql)? WantedByOrphan(mapOrphanBlocks[hashSql]) : hashSql));
        }
        return false;
    }

	    //SQL::MessagePending.SetNull();
    return true;

}
