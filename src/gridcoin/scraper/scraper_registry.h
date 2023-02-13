// Copyright (c) 2014-2023 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_SCRAPER_SCRAPER_REGISTRY_H
#define GRIDCOIN_SCRAPER_SCRAPER_REGISTRY_H

#include "amount.h"
#include "base58.h"
#include "dbwrapper.h"
#include "serialize.h"
#include "gridcoin/scraper/fwd.h"
#include "gridcoin/contract/handler.h"
#include "gridcoin/contract/payload.h"
#include "gridcoin/support/enumbytes.h"


namespace GRC {

enum class ScraperEntryStatus
{
    UNKNOWN,
    DELETED,
    NOT_AUTHORIZED,
    AUTHORIZED,
    EXPLORER,
    OUT_OF_BOUND
};

class ScraperEntry
{
public:
    using Status = EnumByte<ScraperEntryStatus>;

    CKeyID m_keyid;

    int64_t m_timestamp;

    uint256 m_hash;

    uint256 m_previous_hash;

    Status m_status;

    ScraperEntry();

    ScraperEntry(CKeyID key_id);

    ScraperEntry(CKeyID key_id, int64_t tx_timestamp, uint256 hash);

    //static ScraperEntry Parse(const std::string& value);

    bool WellFormed() const;

    CKeyID GetId() const;

    CBitcoinAddress GetAddress() const;

    std::string GetAddressString();

    bool WalletHasPrivateKey(const CWallet* const wallet) const;

    AppCacheEntryExt GetLegacyScraperEntry();

    bool operator==(ScraperEntry b);

    bool operator!=(ScraperEntry b);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_keyid);
        READWRITE(m_timestamp);
        READWRITE(m_hash);
        READWRITE(m_previous_hash);
        READWRITE(m_status);
    }
};

//!
//! \brief The type that defines a shared pointer to a ScraperEntry
//!
typedef std::shared_ptr<ScraperEntry> ScraperEntry_ptr;

//!
//! \brief A type that either points to some ScraperEntry or does not.
//!
typedef const ScraperEntry_ptr ScraperEntryOption;

class ScraperEntryPayload : public IContractPayload
{
public:
    static constexpr uint32_t CURRENT_VERSION = 2;

    uint32_t m_version = CURRENT_VERSION;
    ScraperEntry m_scraper_entry;

    ScraperEntryPayload();

    ScraperEntryPayload(const uint32_t version, ScraperEntry scraper_entry);

    ScraperEntryPayload(ScraperEntry scraper_entry);

    static ScraperEntryPayload Parse(const std::string& key, const std::string& value);

    //!
    //! \brief Get the type of contract that this payload contains data for.
    //!
    GRC::ContractType ContractType() const override
    {
        return GRC::ContractType::SCRAPER;
    }

    //!
    //! \brief Determine whether the instance represents a complete payload.
    //!
    //! \return \c true if the payload contains each of the required elements.
    //!
    bool WellFormed(const ContractAction action) const override
    {
        if (m_version <= 0 || m_version > CURRENT_VERSION) {
            return false;
        }

        if (m_version == 1) {
            return m_scraper_entry.WellFormed() || action == ContractAction::REMOVE;
        }

        return m_scraper_entry.WellFormed();
    }

    //!
    //! \brief Get a string for the key used to construct a legacy contract.
    //!
    std::string LegacyKeyString() const override
    {
        return m_scraper_entry.m_keyid.ToString();
    }

    //!
    //! \brief Get a string for the value used to construct a legacy contract.
    //!
    std::string LegacyValueString() const override
    {
        if (m_scraper_entry.m_status <= ScraperEntryStatus::NOT_AUTHORIZED) {
            return "false";
        } else {
            return "true";
        }
    }

    //!
    //! \brief Get the burn fee amount required to send a particular contract. This
    //! is the same as the LegacyPayload to insure compatibility between the scraper
    //! registry and non-upgraded nodes before the block v13/contract version 3 height
    //!
    //! \return Burn fee in units of 1/100000000 GRC.
    //!
    CAmount RequiredBurnAmount() const override
    {
        return Contract::STANDARD_BURN_AMOUNT;
    }

    ADD_CONTRACT_PAYLOAD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(
        Stream& s,
        Operation ser_action,
        const ContractAction contract_action)
    {
        READWRITE(m_version);
        READWRITE(m_scraper_entry);
    }
}; // ScraperEntryPayload

class ScraperRegistry : public IContractHandler
{
public:
    typedef std::map<CKeyID, ScraperEntry_ptr> ScraperMap;

    typedef std::map<uint256, ScraperEntry_ptr> HistoricalScraperMap;

    const ScraperMap& Scrapers() const;

    const AppCacheSectionExt GetScrapersLegacy() const;

    ScraperEntryOption Try(const CKeyID key_id) const;

    ScraperEntryOption TryAuhorized(const CKeyID key_id) const;

    void Reset() override;

    bool Validate(const Contract& contract, const CTransaction& tx, int &DoS) const override;

    bool BlockValidate(const ContractContext& ctx, int& DoS) const override;

    void Add(const ContractContext& ctx) override;

    void Delete(const ContractContext& ctx) override;

    void Revert(const ContractContext& ctx) override;

    int Initialize();

    int GetDBHeight();

    void SetDBHeight(int& height);

    void ResetInMemoryOnly();

    uint64_t PassivateDB();

    static void RunScraperDBPassivation();

private:
    void AddDelete(const ContractContext& ctx);

    ScraperMap m_scrapers;

    class ScraperEntryDB
    {
    public:
        static constexpr uint32_t CURRENT_VERSION = 1;

        int Initialize(ScraperMap& scrapers);

        void clear_in_memory_only();

        bool clear_leveldb();

        uint64_t passivate_db();

        bool clear();

        size_t size();

        bool StoreDBHeight(const int& height_stored);

        bool LoadDBHeight(int& height_stored);

        bool insert(const uint256& hash, const int& height, const ScraperEntry& scraper);

        bool erase(const uint256& hash);

        std::pair<ScraperRegistry::HistoricalScraperMap::iterator, bool>
            passivate(ScraperRegistry::HistoricalScraperMap::iterator& iter);

        HistoricalScraperMap::iterator begin();

        HistoricalScraperMap::iterator end();

        HistoricalScraperMap::iterator find(const uint256& hash);

        HistoricalScraperMap::iterator advance(HistoricalScraperMap::iterator iter);

    private:
        typedef std::map<uint256, std::pair<uint64_t, ScraperEntry>> StorageScraperMap;

        typedef std::map<uint64_t, ScraperEntry> StorageScraperMapByRecordNum;

        HistoricalScraperMap m_historical;

        bool m_database_init = false;

        int m_height_stored = 0;

        uint64_t m_recnum_stored = 0;

        bool m_needs_passivation = false;

        bool Store(const uint256& hash, const ScraperEntry& scraper_entry);

        bool Load(const uint256 &hash, ScraperEntry& scraper_entry);

        bool Delete(const uint256& hash);
    }; // ScraperEntryDB

    ScraperEntryDB m_scraper_db;

public:

    ScraperEntryDB& GetScraperDB();
}; // ScraperRegistry

//!
//! \brief Get the global beacon registry.
//!
//! \return Current global beacon registry instance.
//!
ScraperRegistry& GetScraperRegistry();
} // namespace GRC

#endif // GRIDCOIN_SCRAPER_SCRAPER_REGISTRY_H
