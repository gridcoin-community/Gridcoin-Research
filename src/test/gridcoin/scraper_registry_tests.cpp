// Copyright (c) 2014-2023 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/scraper/scraper_registry.h"

#include <boost/test/unit_test.hpp>

// anonymous namespace
namespace {
void AddRemoveScraperEntryV1(const std::string& address, const std::string& value,
                                             const GRC::ContractAction& action, const int& height,
                                             const uint64_t& time, const bool& reset_registry = false)
{
    GRC::ScraperRegistry& registry = GRC::GetScraperRegistry();

    std::string status_string = ToLower(value);
    CBitcoinAddress scraper_address;

    // Assert if not a valid address.
    assert(scraper_address.SetString(address));

    // Make sure the registry is reset.
    if (reset_registry) registry.Reset();

    CTransaction dummy_tx;
    CBlockIndex dummy_index {};
    dummy_index.nHeight = height;
    dummy_tx.nTime = time;
    dummy_index.nTime = time;

    GRC::Contract contract;

    assert(!(action == GRC::ContractAction::ADD && !(status_string == "false" || status_string == "true")));

    if (action == GRC::ContractAction::REMOVE) {
        status_string = "false";
    }

    contract = GRC::MakeContract<GRC::ScraperEntryPayload>(
                uint32_t {2}, // Contract version (pre v13) - Scraper payload version will be 1.
                action,
                address,
                status_string);

    dummy_tx.vContracts.push_back(contract);

    registry.Add({contract, dummy_tx, &dummy_index});
}

void AddRemoveScraperEntryV2(const std::string& address, const GRC::ScraperEntryStatus& status,
                                             const GRC::ContractAction& action, const int& height,
                                             const uint64_t& time, const bool& reset_registry = false)
{
    GRC::ScraperRegistry& registry = GRC::GetScraperRegistry();

    CBitcoinAddress scraper_address;
    CKeyID key_id;

    scraper_address.SetString(address);

    scraper_address.GetKeyID(key_id);

    // Make sure the registry is reset.
    if (reset_registry) registry.Reset();

    CTransaction dummy_tx;
    CBlockIndex dummy_index {};
    dummy_index.nHeight = height;
    dummy_tx.nTime = time;
    dummy_index.nTime = time;

    GRC::Contract contract;

        contract = GRC::MakeContract<GRC::ScraperEntryPayload>(
                    uint32_t {3}, // Contract version (post v13)
                    action,
                    uint32_t {2}, // Protocol payload version (post v13)
                    key_id,
                    status);

    dummy_tx.vContracts.push_back(contract);

    registry.Add({contract, dummy_tx, &dummy_index});
}

}// anonymous namespace

BOOST_AUTO_TEST_SUITE(scraper_registry_tests)

BOOST_AUTO_TEST_CASE(scraper_entries_added_to_scraper_work_correctly_legacy)
{
    int height = 0;
    uint64_t time = 0;

    auto& registry = GRC::GetScraperRegistry();
    const auto& scraper_map = registry.Scrapers();

    std::vector<std::string> scraper_entries { {"RxKVQ1SGpgyUfMv1zdygmiLi24mxG34k6f",
                                               "S2wGoFavFzTnpaxn6XqLmJDE9FppHiMrCn",
                                               "SA48uv72G9nWcdUG4cnd6EsuqKsyfW6EEN",
                                               "SFxUqNdhdrkhzfjTqALSpCvJbz7JTrvhP3",
                                               "SGhoGYzuHtDgy3NNp6cUx5jaMZJQ8osVaZ",
                                               "SL7PuciT2GVRjjP4MiMLKAxMhfPnBZrsUL",
                                               "SLbdvKZHmtu49VUWm88rbcCo9DaC8Z2urV"} };

    registry.Reset();

    for (const auto& entry : scraper_entries) {
        AddRemoveScraperEntryV1(entry,
                                "true",
                                GRC::ContractAction::ADD,
                                height++,
                                time++,
                                false);
    }

    BOOST_CHECK(scraper_map.size() == 7);

    // Native format from legacy adds.
    for (const auto& entry : scraper_entries) {
        CBitcoinAddress address;
        address.SetString(entry);

        CKeyID key_id;
        address.GetKeyID(key_id);

        auto registry_entry = scraper_map.find(key_id);

        BOOST_CHECK(registry_entry != scraper_map.end());
        BOOST_CHECK(registry_entry->second->m_status == GRC::ScraperEntryStatus::AUTHORIZED);
    }

    // Legacy format from legacy adds.
    for (const auto& entry : scraper_entries) {
        auto& legacy_scraper_entries = registry.GetScrapersLegacy();

        auto legacy_appcache_entry = legacy_scraper_entries.find(entry);

        BOOST_CHECK(legacy_appcache_entry != legacy_scraper_entries.end());

        BOOST_CHECK(legacy_appcache_entry->second.value == "true");
    }
}

BOOST_AUTO_TEST_CASE(scraper_entry_deauthorize_and_delete_works_correctly_legacy)
{
    int height = 0;
    uint64_t time = 0;

    auto& registry = GRC::GetScraperRegistry();
    const auto& scraper_map = registry.Scrapers();

    std::vector<std::string> scraper_entries { {"RxKVQ1SGpgyUfMv1zdygmiLi24mxG34k6f",
                                               "S2wGoFavFzTnpaxn6XqLmJDE9FppHiMrCn",
                                               "SA48uv72G9nWcdUG4cnd6EsuqKsyfW6EEN",
                                               "SFxUqNdhdrkhzfjTqALSpCvJbz7JTrvhP3",
                                               "SGhoGYzuHtDgy3NNp6cUx5jaMZJQ8osVaZ",
                                               "SL7PuciT2GVRjjP4MiMLKAxMhfPnBZrsUL",
                                               "SLbdvKZHmtu49VUWm88rbcCo9DaC8Z2urV"} };

    registry.Reset();

    for (const auto& entry : scraper_entries) {
        AddRemoveScraperEntryV1(entry,
                                "true",
                                GRC::ContractAction::ADD,
                                height++,
                                time++,
                                false);
    }

    BOOST_CHECK(scraper_map.size() == 7);

    // Deauthorize a scraper
    AddRemoveScraperEntryV1("SL7PuciT2GVRjjP4MiMLKAxMhfPnBZrsUL",
                            "false",
                            GRC::ContractAction::ADD,
                            height++,
                            time++,
                            false);

    // Still should be 7 current elements.
    BOOST_CHECK(scraper_map.size() == 7);

    // Native format from legacy adds.
    for (const auto& entry : scraper_entries) {
        CBitcoinAddress address;
        address.SetString(entry);

        CKeyID key_id;
        address.GetKeyID(key_id);

        auto registry_entry = scraper_map.find(key_id);

        BOOST_CHECK(registry_entry != scraper_map.end());

        if (entry == "SL7PuciT2GVRjjP4MiMLKAxMhfPnBZrsUL") {
            BOOST_CHECK(registry_entry->second->m_status == GRC::ScraperEntryStatus::NOT_AUTHORIZED);
        } else {
            BOOST_CHECK(registry_entry->second->m_status == GRC::ScraperEntryStatus::AUTHORIZED);
        }
    }

    // Legacy format from legacy adds.
   for (const auto& entry : scraper_entries) {
       auto legacy_scraper_entries = registry.GetScrapersLegacy();

        auto legacy_appcache_entry = legacy_scraper_entries.find(entry);

        BOOST_CHECK(legacy_appcache_entry != legacy_scraper_entries.end());

        if (entry == "SL7PuciT2GVRjjP4MiMLKAxMhfPnBZrsUL") {
            BOOST_CHECK(legacy_appcache_entry->second.value == "false");
        } else {
            BOOST_CHECK(legacy_appcache_entry->second.value == "true");
        }
    }

    // Remove a scraper. Here we do it manually because we need the ctx to survive to do the
    // reversion later.
    height++;
    time++;

    CTransaction dummy_tx;
    CBlockIndex dummy_index {};
    dummy_index.nHeight = height;
    dummy_tx.nTime = time;
    dummy_index.nTime = time;

    GRC::Contract contract;

    contract = GRC::MakeContract<GRC::ScraperEntryPayload>(
                uint32_t {2}, // Contract version (pre v13) - Scraper payload version will be 1.
                GRC::ContractAction::REMOVE,
                "SLbdvKZHmtu49VUWm88rbcCo9DaC8Z2urV",
                "false");

    dummy_tx.vContracts.push_back(contract);

    GRC::ContractContext ctx(contract, dummy_tx, &dummy_index);

    registry.Add(ctx);

    // Native map should still be 7 elements
    BOOST_CHECK(scraper_map.size() == 7);

    // Legacy extended scrapers map should still be 7 elements
    BOOST_CHECK(registry.GetScrapersLegacyExt().size() == 7);

    // Legacy scrapers map should be 6 elements.
    BOOST_CHECK(registry.GetScrapersLegacy().size() == 6);


    // Native format from legacy adds, deauthorize, and delete for active scraper map..
    for (const auto& entry : scraper_entries) {
        CBitcoinAddress address;
        address.SetString(entry);

        CKeyID key_id;
        address.GetKeyID(key_id);

        auto registry_entry = scraper_map.find(key_id);

        BOOST_CHECK(registry_entry != scraper_map.end());

        if (entry == "SLbdvKZHmtu49VUWm88rbcCo9DaC8Z2urV") {
            BOOST_CHECK(registry_entry->second->m_status == GRC::ScraperEntryStatus::DELETED);
        } else if (entry == "SL7PuciT2GVRjjP4MiMLKAxMhfPnBZrsUL") {
            BOOST_CHECK(registry_entry->second->m_status == GRC::ScraperEntryStatus::NOT_AUTHORIZED);
        } else {
            BOOST_CHECK(registry_entry->second->m_status == GRC::ScraperEntryStatus::AUTHORIZED);
        }
    }


    // Legacy format from legacy adds, deauthorize, and delete for active scraper map.
    for (const auto& entry : scraper_entries) {
        auto legacy_scraper_entries = registry.GetScrapersLegacy();

        auto legacy_appcache_entry = legacy_scraper_entries.find(entry);

        // deleted entry should not be in active map
        if (entry == "SLbdvKZHmtu49VUWm88rbcCo9DaC8Z2urV") {
            BOOST_CHECK(legacy_appcache_entry == legacy_scraper_entries.end());
        } else if (entry == "SL7PuciT2GVRjjP4MiMLKAxMhfPnBZrsUL") {
            BOOST_CHECK(legacy_appcache_entry->second.value == "false");
        } else {
            BOOST_CHECK(legacy_appcache_entry->second.value == "true");
        }
    }


    // Legacy format from legacy adds, deauthorize, and delete for extended scraper map.
    for (const auto& entry : scraper_entries) {
        auto legacy_scraper_entries_ext = registry.GetScrapersLegacyExt();

        auto legacy_appcache_ext_entry = legacy_scraper_entries_ext.find(entry);


        // All entries SHOULD be in extended map.
        BOOST_CHECK(legacy_appcache_ext_entry != legacy_scraper_entries_ext.end());

        if (entry == "SL7PuciT2GVRjjP4MiMLKAxMhfPnBZrsUL" || entry == "SLbdvKZHmtu49VUWm88rbcCo9DaC8Z2urV") {
            BOOST_CHECK(legacy_appcache_ext_entry->second.value == "false");
        }

        // Deleted scraper should be marked deleted.
        if (entry == "SLbdvKZHmtu49VUWm88rbcCo9DaC8Z2urV") {
            BOOST_CHECK(legacy_appcache_ext_entry->second.deleted == true);
        }
    }

    BOOST_CHECK(registry.GetDBHeight() == 9);

    // Revert the scraper removal using the ctx from the last add. This simulates at a low level what heppens
    // when a reorg happens.
    registry.Revert(ctx);

    int post_revert_height = 8;
    registry.SetDBHeight(post_revert_height);

    // After reversion...
    // Native map should be back to 7 elements.
    BOOST_CHECK(scraper_map.size() == 7);

    // Legacy extended scrapers map should be 7 elements.
    BOOST_CHECK(registry.GetScrapersLegacyExt().size() == 7);

    // Legacy scrapers map should be back to 7 elements.
    BOOST_CHECK(registry.GetScrapersLegacy().size() == 7);

    // The specific scraper entry that was removed should now be resurrected after the reversion.
    auto resurrected_scraper_entries = registry.GetScrapersLegacy();

    auto resurrected_scraper = resurrected_scraper_entries.find("SLbdvKZHmtu49VUWm88rbcCo9DaC8Z2urV");

    BOOST_CHECK(resurrected_scraper != registry.GetScrapersLegacy().end());

    BOOST_CHECK(resurrected_scraper->first == "SLbdvKZHmtu49VUWm88rbcCo9DaC8Z2urV");
    BOOST_CHECK(resurrected_scraper->second.value == "true");
    BOOST_CHECK(resurrected_scraper->second.timestamp = 6);
}


BOOST_AUTO_TEST_CASE(scraper_entry_deauthorize_and_delete_works_correctly_native)
{
    int height = 0;
    uint64_t time = 0;

    auto& registry = GRC::GetScraperRegistry();
    const auto& scraper_map = registry.Scrapers();

    std::vector<std::string> scraper_entries { {"RxKVQ1SGpgyUfMv1zdygmiLi24mxG34k6f",
                                               "S2wGoFavFzTnpaxn6XqLmJDE9FppHiMrCn",
                                               "SA48uv72G9nWcdUG4cnd6EsuqKsyfW6EEN",
                                               "SFxUqNdhdrkhzfjTqALSpCvJbz7JTrvhP3",
                                               "SGhoGYzuHtDgy3NNp6cUx5jaMZJQ8osVaZ",
                                               "SL7PuciT2GVRjjP4MiMLKAxMhfPnBZrsUL",
                                               "SLbdvKZHmtu49VUWm88rbcCo9DaC8Z2urV"} };

    registry.Reset();

    for (const auto& entry : scraper_entries) {
        AddRemoveScraperEntryV2(entry,
                                GRC::ScraperEntryStatus::AUTHORIZED,
                                GRC::ContractAction::ADD,
                                height++,
                                time++,
                                false);
    }

    BOOST_CHECK(scraper_map.size() == 7);

    // Deauthorize a scraper
    AddRemoveScraperEntryV2("SL7PuciT2GVRjjP4MiMLKAxMhfPnBZrsUL",
                            GRC::ScraperEntryStatus::NOT_AUTHORIZED,
                            GRC::ContractAction::ADD,
                            height++,
                            time++,
                            false);

    // Still should be 7 current elements.
    BOOST_CHECK(scraper_map.size() == 7);

    // Native format from native adds.
    for (const auto& entry : scraper_entries) {
        CBitcoinAddress address;
        address.SetString(entry);

        CKeyID key_id;
        address.GetKeyID(key_id);

        auto registry_entry = scraper_map.find(key_id);

        BOOST_CHECK(registry_entry != scraper_map.end());

        if (entry == "SL7PuciT2GVRjjP4MiMLKAxMhfPnBZrsUL") {
            BOOST_CHECK(registry_entry->second->m_status == GRC::ScraperEntryStatus::NOT_AUTHORIZED);
        } else {
            BOOST_CHECK(registry_entry->second->m_status == GRC::ScraperEntryStatus::AUTHORIZED);
        }
    }

    // Legacy format from native adds.
   for (const auto& entry : scraper_entries) {
       auto legacy_scraper_entries = registry.GetScrapersLegacy();

        auto legacy_appcache_entry = legacy_scraper_entries.find(entry);

        BOOST_CHECK(legacy_appcache_entry != legacy_scraper_entries.end());

        if (entry == "SL7PuciT2GVRjjP4MiMLKAxMhfPnBZrsUL") {
            BOOST_CHECK(legacy_appcache_entry->second.value == "false");
        } else {
            BOOST_CHECK(legacy_appcache_entry->second.value == "true");
        }
    }

    // Remove a scraper. Here we do it manually because we need the ctx to survive to do the
    // reversion later.
    height++;
    time++;

    CTransaction dummy_tx;
    CBlockIndex dummy_index {};
    dummy_index.nHeight = height;
    dummy_tx.nTime = time;
    dummy_index.nTime = time;

    GRC::Contract contract;

    CBitcoinAddress address;
    address.SetString("SLbdvKZHmtu49VUWm88rbcCo9DaC8Z2urV");

    CKeyID key_id;
    address.GetKeyID(key_id)
;
    contract = GRC::MakeContract<GRC::ScraperEntryPayload>(
                uint32_t {3}, // Contract version (pre v13)
                GRC::ContractAction::REMOVE,
                uint32_t {2},
                key_id,
                GRC::ScraperEntryStatus::NOT_AUTHORIZED);

    dummy_tx.vContracts.push_back(contract);

    GRC::ContractContext ctx(contract, dummy_tx, &dummy_index);

    registry.Add(ctx);

    // Native map should still be 7 elements
    BOOST_CHECK(scraper_map.size() == 7);

    // Legacy extended scrapers map should still be 7 elements
    BOOST_CHECK(registry.GetScrapersLegacyExt().size() == 7);

    // Legacy scrapers map should be 6 elements.
    BOOST_CHECK(registry.GetScrapersLegacy().size() == 6);

    // Native format from legacy adds, deauthorize, and delete for active scraper map..
    for (const auto& entry : scraper_entries) {
        CBitcoinAddress address;
        address.SetString(entry);

        CKeyID key_id;
        address.GetKeyID(key_id);

        auto registry_entry = scraper_map.find(key_id);

        BOOST_CHECK(registry_entry != scraper_map.end());

        if (entry == "SLbdvKZHmtu49VUWm88rbcCo9DaC8Z2urV") {
            BOOST_CHECK(registry_entry->second->m_status == GRC::ScraperEntryStatus::DELETED);
        } else if (entry == "SL7PuciT2GVRjjP4MiMLKAxMhfPnBZrsUL") {
            BOOST_CHECK(registry_entry->second->m_status == GRC::ScraperEntryStatus::NOT_AUTHORIZED);
        } else {
            BOOST_CHECK(registry_entry->second->m_status == GRC::ScraperEntryStatus::AUTHORIZED);
        }
    }


    // Legacy format from legacy adds, deauthorize, and delete for active scraper map.
    for (const auto& entry : scraper_entries) {
        auto legacy_scraper_entries = registry.GetScrapersLegacy();

        auto legacy_appcache_entry = legacy_scraper_entries.find(entry);

        // deleted entry should not be in active map
        if (entry == "SLbdvKZHmtu49VUWm88rbcCo9DaC8Z2urV") {
            BOOST_CHECK(legacy_appcache_entry == legacy_scraper_entries.end());
        } else if (entry == "SL7PuciT2GVRjjP4MiMLKAxMhfPnBZrsUL") {
            BOOST_CHECK(legacy_appcache_entry->second.value == "false");
        } else {
            BOOST_CHECK(legacy_appcache_entry->second.value == "true");
        }
    }


    // Legacy format from legacy adds, deauthorize, and delete for extended scraper map.
    for (const auto& entry : scraper_entries) {
        auto legacy_scraper_entries_ext = registry.GetScrapersLegacyExt();

        auto legacy_appcache_ext_entry = legacy_scraper_entries_ext.find(entry);


        // All entries SHOULD be in extended map.
        BOOST_CHECK(legacy_appcache_ext_entry != legacy_scraper_entries_ext.end());

        if (entry == "SL7PuciT2GVRjjP4MiMLKAxMhfPnBZrsUL" || entry == "SLbdvKZHmtu49VUWm88rbcCo9DaC8Z2urV") {
            BOOST_CHECK(legacy_appcache_ext_entry->second.value == "false");
        }

        // Deleted scraper should be marked deleted.
        if (entry == "SLbdvKZHmtu49VUWm88rbcCo9DaC8Z2urV") {
            BOOST_CHECK(legacy_appcache_ext_entry->second.deleted == true);
        }
    }

    BOOST_CHECK(registry.GetDBHeight() == 9);

    // Revert the scraper removal using the ctx from the last add. This simulates at a low level what heppens
    // when a reorg happens.
    registry.Revert(ctx);

    int post_revert_height = 8;
    registry.SetDBHeight(post_revert_height);

    // After reversion...
    // Native map should be back to 7 elements.
    BOOST_CHECK(scraper_map.size() == 7);

    // Legacy extended scrapers map should be 7 elements.
    BOOST_CHECK(registry.GetScrapersLegacyExt().size() == 7);

    // Legacy scrapers map should be back to 7 elements.
    BOOST_CHECK(registry.GetScrapersLegacy().size() == 7);

    // The specific scraper entry that was removed should now be resurrected after the reversion.
    auto resurrected_scraper_entries = registry.GetScrapersLegacy();

    auto resurrected_scraper = resurrected_scraper_entries.find("SLbdvKZHmtu49VUWm88rbcCo9DaC8Z2urV");

    BOOST_CHECK(resurrected_scraper != registry.GetScrapersLegacy().end());

    BOOST_CHECK(resurrected_scraper->first == "SLbdvKZHmtu49VUWm88rbcCo9DaC8Z2urV");
    BOOST_CHECK(resurrected_scraper->second.value == "true");
    BOOST_CHECK(resurrected_scraper->second.timestamp = 6);
}

BOOST_AUTO_TEST_SUITE_END()
