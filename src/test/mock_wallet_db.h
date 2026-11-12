// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_MOCK_WALLET_DB_H
#define GRIDCOIN_TEST_MOCK_WALLET_DB_H

#include "clientversion.h"
#include "serialize.h"
#include "span.h"
#include "streams.h"
#include "wallet/db.h"

#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

namespace grc_test {

//! The mock BDB env backs every wallet file name with one shared in-memory database, so a case
//! that reloads a wallet sees every record any earlier case wrote (and an earlier case's vin-less
//! records make LoadWallet fail). This moves those records aside for the life of the scope and puts
//! them back afterwards, in both directions: the case's own records are deleted again before the
//! put-back. The restore-pin case in wallet_tests pins the second direction.
class MockWalletDbScope : private CDB
{
public:
    explicit MockWalletDbScope(const std::string& file) : CDB(file, "cr+")
    {
        BOOST_REQUIRE(bitdb.IsMock());
        BOOST_REQUIRE(pdb != nullptr);

        BOOST_REQUIRE(Walk(m_saved));

        // From the first del on, nothing below may throw.
        bool all_deleted = true;
        for (const CDBEnv::KeyValPair& kv : m_saved) {
            if (Del(kv) != 0) all_deleted = false;
        }

        std::vector<CDBEnv::KeyValPair> left;
        const bool walked = Walk(left);
        m_isolated = all_deleted && walked && left.empty();
    }

    ~MockWalletDbScope()
    {
        std::vector<CDBEnv::KeyValPair> present;
        if (!Walk(present)) BOOST_ERROR("MockWalletDbScope: cannot walk the database to restore it");
        for (const CDBEnv::KeyValPair& kv : present) {
            if (Del(kv) != 0) BOOST_ERROR("MockWalletDbScope: cannot delete a case record");
        }
        for (const CDBEnv::KeyValPair& kv : m_saved) {
            Dbt k(const_cast<unsigned char*>(kv.first.data()), kv.first.size());
            Dbt v(const_cast<unsigned char*>(kv.second.data()), kv.second.size());
            if (pdb->put(nullptr, &k, &v, 0) != 0) BOOST_ERROR("MockWalletDbScope: cannot restore a record");
        }
    }

    bool isolated() const { return m_isolated; }

private:
    // Reads every record; the cursor is closed before returning so the caller may del and put.
    bool Walk(std::vector<CDBEnv::KeyValPair>& out)
    {
        out.clear();
        Dbc* pcursor = GetCursor();
        if (!pcursor) return false;
        bool ok = true;
        while (true) {
            CDataStream ss_key(SER_DISK, CLIENT_VERSION);
            CDataStream ss_value(SER_DISK, CLIENT_VERSION);
            const int ret = ReadAtCursor(pcursor, ss_key, ss_value, DB_NEXT);
            if (ret == DB_NOTFOUND) break;
            if (ret != 0) {
                ok = false;
                break;
            }
            out.emplace_back(std::vector<unsigned char>{UCharCast(ss_key.data()), UCharCast(ss_key.data()) + ss_key.size()},
                             std::vector<unsigned char>{UCharCast(ss_value.data()), UCharCast(ss_value.data()) + ss_value.size()});
        }
        pcursor->close();
        return ok;
    }

    int Del(const CDBEnv::KeyValPair& kv)
    {
        Dbt k(const_cast<unsigned char*>(kv.first.data()), kv.first.size());
        return pdb->del(nullptr, &k, 0);
    }

    std::vector<CDBEnv::KeyValPair> m_saved;
    bool m_isolated{false};
};

} // namespace grc_test

#endif // GRIDCOIN_TEST_MOCK_WALLET_DB_H
