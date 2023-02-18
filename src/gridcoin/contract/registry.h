#ifndef GRIDCOIN_CONTRACT_REGISTRY_H
#define GRIDCOIN_CONTRACT_REGISTRY_H

#include "gridcoin/contract/payload.h"
#include "gridcoin/beacon.h"
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
    }

    int GetRegistryBlockHeight(const ContractType type) const
    {
        int db_height = 0;
        auto db_height_entry = m_db_heights.find(type);

        if (db_height_entry != m_db_heights.end()) {
            db_height = db_height_entry->second;
        }

        return db_height;
    }

    void UpdateRegistryBlockHeights()
    {
        // We use array notation here, because we want the latest to override, and if one doesn't exist it will
        // be created.
        m_db_heights[ContractType::BEACON] = GetBeaconRegistry().GetDBHeight();
        m_db_heights[ContractType::SCRAPER] = GetScraperRegistry().GetDBHeight();
    }

private:
    RegistryBlockHeights m_db_heights;
};

} // GRC namespace
#endif // GRIDCOIN_CONTRACT_REGISTRY_H
