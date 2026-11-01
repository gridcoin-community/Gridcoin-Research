// Copyright (c) 2014-2024 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_CONTRACT_REGISTRY_H
#define GRIDCOIN_CONTRACT_REGISTRY_H

#include "chainparams.h"
#include "gridcoin/contract/payload.h"
#include "gridcoin/beacon.h"
#include "gridcoin/pool.h"
#include "gridcoin/project.h"
#include "gridcoin/protocol.h"
#include "gridcoin/sidestake.h"
#include "gridcoin/scraper/scraper_registry.h"
#include "gridcoin/voting/registry.h"

namespace GRC {

class RegistryBookmarks_error : public std::runtime_error
{
public:
    explicit RegistryBookmarks_error(const std::string& str)
        : std::runtime_error("ERROR: " + str)
    {
        LogPrintf("ERROR: %s", str);
    }
};

//!
//! \brief Registry is simply a mnemonic alias for IContractHandler.
//!
typedef IContractHandler Registry;

typedef std::unordered_map<ContractType, int> RegistryBlockHeights;

class RegistryBookmarks
{
public:
    static const std::vector<GRC::ContractType> CONTRACT_TYPES_WITH_REG_DB;

    static const std::vector<GRC::ContractType> CONTRACT_TYPES_SUPPORTING_REVERT;

    RegistryBookmarks()
    {
        UpdateRegistryBookmarks();
    }

    static Registry& GetRegistryWithDB(const ContractType type)
    {
        switch (type) {
        case ContractType::BEACON:         return GetBeaconRegistry();
        case ContractType::PROJECT:        return GetWhitelist();
        case ContractType::PROTOCOL:       return GetProtocolRegistry();
        case ContractType::SCRAPER:        return GetScraperRegistry();
        case ContractType::SIDESTAKE:      return GetSideStakeRegistry();
        case ContractType::POOL_REGISTER:  return GetPoolRegistry();
        case ContractType::POOL_APPROVE:   return GetPoolRegistry();
        case ContractType::UNKNOWN:
            [[fallthrough]];
        case ContractType::CLAIM:
            [[fallthrough]];
        case ContractType::MESSAGE:
            [[fallthrough]];
        case ContractType::POLL:
            [[fallthrough]];
        case ContractType::VOTE:
            [[fallthrough]];
        case ContractType::MRC:
            [[fallthrough]];
        case ContractType::OUT_OF_BOUND:
            break;
        }

        throw RegistryBookmarks_error("Contract type has no registry db.");
    }

    static Registry& GetRegistryWithRevert(const ContractType type)
    {
        switch (type) {
        case ContractType::BEACON:         return GetBeaconRegistry();
        case ContractType::POLL:           return GetPollRegistry();
        case ContractType::PROJECT:        return GetWhitelist();
        case ContractType::PROTOCOL:       return GetProtocolRegistry();
        case ContractType::SCRAPER:        return GetScraperRegistry();
        case ContractType::VOTE:           return GetPollRegistry();
        case ContractType::SIDESTAKE:      return GetSideStakeRegistry();
        case ContractType::POOL_REGISTER:  return GetPoolRegistry();
        case ContractType::POOL_APPROVE:   return GetPoolRegistry();
            [[fallthrough]];
        case ContractType::UNKNOWN:
            [[fallthrough]];
        case ContractType::CLAIM:
            [[fallthrough]];
        case ContractType::MESSAGE:
            [[fallthrough]];
        case ContractType::MRC:
            [[fallthrough]];
        case ContractType::OUT_OF_BOUND:
            break;
        }

        throw RegistryBookmarks_error("Contract type has no contract handler reversion capability.");
    }


    static bool IsRegistryBackedByDB(const ContractType& type)
    {
        auto iter = std::find(CONTRACT_TYPES_WITH_REG_DB.begin(),
                              CONTRACT_TYPES_WITH_REG_DB.end(), type);

        return (iter != CONTRACT_TYPES_WITH_REG_DB.end());
    }

    static bool IsRegistryRevertCapable(const ContractType& type)
    {
        auto iter = std::find(CONTRACT_TYPES_SUPPORTING_REVERT.begin(),
                              CONTRACT_TYPES_SUPPORTING_REVERT.end(), type);

        return (iter != CONTRACT_TYPES_SUPPORTING_REVERT.end());
    }

    std::optional<int> GetRegistryBlockHeight(const ContractType type) const
    {
        auto db_height_entry = m_db_heights.find(type);

        if (db_height_entry == m_db_heights.end()) {
            return std::nullopt;
        }

        return db_height_entry->second;
    }

    void UpdateRegistryBookmarks()
    {
        // We use array notation here, because we want the latest to override, and if one doesn't exist it will
        // be created.
        for (const auto& registry_type : CONTRACT_TYPES_WITH_REG_DB) {
            m_db_heights[registry_type] = GetRegistryWithDB(registry_type).GetDBHeight();
        }
    }

    //!
    //! \brief This method is used in the cleanup after disconnecting blocks in DisconnectBlocksBatch to reset the db
    //! bookmark heights. It will reset the DB block height AND bookmark for a registry if the new (head of chain)
    //! height is less than the recorded bookmark for that contract type.
    //!
    //! \param block_height.
    //!
    void UpdateRegistryBlockHeights(int& block_height)
    {
        for (const auto& registry_type : CONTRACT_TYPES_WITH_REG_DB) {
            if (GetRegistryBlockHeight(registry_type) > block_height) {
                GetRegistryWithDB(registry_type).SetDBHeight(block_height);

                m_db_heights[registry_type] = block_height;
            }
        }
    }

    int GetLowestRegistryBlockHeight()
    {
        int lowest_height = std::numeric_limits<int>::max();

        for (const auto& iter : m_db_heights) {
            int db_height = iter.second;

            //! When below the operational range of the sidestake contracts and registry, initialization of the sidestake
            //! registry will report zero for height. It is undesirable to return this in the GetLowestRegistryBlockHeight()
            //! method, because it will cause the contract replay clamp to go to the Fern mandatory blockheight. Setting
            //! the db_height recorded in the bookmarks at V13 height for the sidestake registry for the purpose of contract
            //! replay solves the problem.
            //!
            //! This code can be removed after the V13 mandatory blockheight has been reached.
            if (iter.first == GRC::ContractType::SIDESTAKE and db_height < Params().GetConsensus().BlockV13Height) {
                db_height = Params().GetConsensus().BlockV13Height;
            }

            //! Analogous treatment for POOL contracts (issue #1783) gated behind V15. Both contract
            //! types share the same PoolRegistry/RegistryDB instance, so both bookmark entries get
            //! the clamp. This is a conditional MAX — exactly the shape of the SIDESTAKE/V13 line
            //! above (`if (db_height < V13) db_height = V13;`), just keyed to the V15 height.
            //!
            //! Pre-activation GetBlockV15Height() is std::numeric_limits<int>::max(), so the
            //! comparison `db_height < ::max()` is true for any real bookmark (including the
            //! initialization-reported 0) and clamps db_height up to ::max(). That can never lower
            //! `lowest_height` (the comparison below never assigns it), so POOL stays out of the
            //! floor entirely until V15 is pinned — and crucially POOL's raw bookmark (0) cannot
            //! drag the replay floor down to ~V11.
            //!
            //! Post-activation it raises a too-low bookmark up to the V15 height the same way
            //! SIDESTAKE raises it to V13, but PRESERVES the real near-tip bookmark once it has
            //! advanced past V15. An unconditional `db_height = GetBlockV15Height()` would instead
            //! pin the floor to the V15 height forever, discarding the real bookmark and forcing a
            //! full V15->tip contract replay on every startup — the very regression this clamp
            //! exists to prevent.
            //!
            //! Uses GetBlockV15Height() so the `-blockv15height` isolated-testnet override is
            //! honored consistently with IsV15Enabled and the startup log line.
            if ((iter.first == GRC::ContractType::POOL_REGISTER || iter.first == GRC::ContractType::POOL_APPROVE)
                and db_height < GetBlockV15Height()) {
                db_height = GetBlockV15Height();
            }

            //! Compare and assign the *clamped* db_height. (The prior form compared the unclamped
            //! iter.second but assigned db_height, so a clamped SIDESTAKE/POOL entry could lower the
            //! floor below its own clamp when another registry was present.)
            if (db_height < lowest_height) {
                lowest_height = db_height;
            }
        }

        return lowest_height;
    }

private:
    RegistryBlockHeights m_db_heights;
};

} // GRC namespace
#endif // GRIDCOIN_CONTRACT_REGISTRY_H
