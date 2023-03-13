#ifndef GRIDCOIN_CONTRACT_REGISTRY_H
#define GRIDCOIN_CONTRACT_REGISTRY_H

#include "gridcoin/contract/payload.h"
#include "gridcoin/beacon.h"
#include "gridcoin/project.h"
#include "gridcoin/protocol.h"
#include "gridcoin/scraper/scraper_registry.h"

namespace GRC {

typedef std::unordered_map<ContractType, int> RegistryBlockHeights;

class RegistryBookmarks
{
public:
    RegistryBookmarks()
    {
        m_db_heights.insert(std::make_pair(ContractType::BEACON, GetBeaconRegistry().GetDBHeight()));
        m_db_heights.insert(std::make_pair(ContractType::SCRAPER, GetScraperRegistry().GetDBHeight()));
        m_db_heights.insert(std::make_pair(ContractType::PROTOCOL, GetProtocolRegistry().GetDBHeight()));
        m_db_heights.insert(std::make_pair(ContractType::PROJECT, GetWhitelist().GetDBHeight()));
    }

    std::optional<int> GetRegistryBlockHeight(const ContractType type) const
    {
        auto db_height_entry = m_db_heights.find(type);

        if (db_height_entry == m_db_heights.end()) {
            return std::nullopt;
        }

        return db_height_entry->second;
    }

    void UpdateRegistryBlockHeights()
    {
        // We use array notation here, because we want the latest to override, and if one doesn't exist it will
        // be created.
        m_db_heights[ContractType::BEACON] = GetBeaconRegistry().GetDBHeight();
        m_db_heights[ContractType::SCRAPER] = GetScraperRegistry().GetDBHeight();
        m_db_heights[ContractType::PROTOCOL] = GetProtocolRegistry().GetDBHeight();
        m_db_heights[ContractType::PROJECT] = GetWhitelist().GetDBHeight();
    }

    int GetLowestRegistryBlockHeight()
    {
        int lowest_height = std::numeric_limits<int>::max();

        for (const auto& iter : m_db_heights) {
            if (iter.second < lowest_height) {
                lowest_height = iter.second;
            }
        }

        return lowest_height;
    }

private:
    RegistryBlockHeights m_db_heights;
};

} // GRC namespace
#endif // GRIDCOIN_CONTRACT_REGISTRY_H
