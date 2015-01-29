// Copyright (c) 11-11-2014 Rob Halford
#ifndef BITCOIN_SQL_H
#define BITCOIN_SQL_H

#include <map>
#include "net.h"
#include "util.h"


class uint256;
class CBlockIndex;
class Sql;


namespace SQL
{
    enum SqlMode
    {
        // Scrict checkpoints policy, perform conflicts verification and resolve conflicts
        Test = 0,
        // Advisory checkpoints policy, perform conflicts verification but don't try to resolve them
        Prod = 1
    };
	extern std::string sQuery;

	//bool SendSyncCheckpointWithBalance(uint256 hashCheckpoint, double nBalance, std::string SendingWalletAddress);
}

// Gridcoin


class CSQLbase
{
public:
    int nVersion;
    uint256 hashSql;      // SQL block
    std::string payload;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(hashSql);
		READWRITE(payload);
    )

    void SetNull()
    {
        nVersion = 1;
        hashSql = 0;
    }

    std::string ToString() const
    {
        return strprintf(
                "CSQL(\n"
                "    nVersion            = %d\n"
                "    hashSql             = %s\n"
			    ")\n",
            nVersion,
            hashSql.ToString().c_str());
    }

    void print() const
    {
        printf("%s", ToString().c_str());
    }
};

class CSQL : public CSQLbase
{
public:
    static const std::string strMasterPubKey;
  
    std::vector<unsigned char> vchMsg;
    std::vector<unsigned char> vchSig;

    CSQL()
    {
        SetNull();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(vchMsg);
        READWRITE(vchSig);
		READWRITE(payload);
    )

    void SetNull()
    {
		CSQLbase::SetNull();
        vchMsg.clear();
        vchSig.clear();
    }

    bool IsNull() const
    {
        return (hashSql == 0);
    }

    uint256 GetHash() const
    {
        return SerializeHash(*this);
    }

    bool RelayTo(CNode* pnode) const
    {
        // returns true if wasn't already sent
       // if (pnode->hashSqlKnown != hashSql)
        //{
         //   pnode->hashSqlKnown = hashSql;
            pnode->PushMessage("SQL", *this);
          //  return true;
       // }
        return false;
    }

    //bool CheckSignature();
	//b//ool CheckSignatureWithBalance();
    //bool ProcessSyncCheckpoint(CNode* pfrom);
	
	bool ProcessSQLRow(CNode* pfrom);

};

#endif
