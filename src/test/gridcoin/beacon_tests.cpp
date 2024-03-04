// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "dbwrapper.h"
#include "gridcoin/beacon.h"
#include "rpc/blockchain.h"
#include "test/data/testnet_beacon.bin.h"
#include "test/data/mainnet_beacon.bin.h"
#include <util/string.h>

#include <boost/test/unit_test.hpp>
#include <vector>

extern leveldb::DB *txdb;

namespace {
//!
//! \brief Provides various public and private key representations for tests.
//!
//! Keys match the shared contract message keys embedded in the application.
//!
struct TestKey
{
    //!
    //! \brief Create a valid private key for tests.
    //!
    static CKey Private()
    {
        std::vector<unsigned char> private_key = ParseHex(
            "308201130201010420fbd45ffb02ff05a3322c0d77e1e7aea264866c24e81e5ab6"
            "a8e150666b4dc6d8a081a53081a2020101302c06072a8648ce3d0101022100ffff"
            "fffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f300604"
            "010004010704410479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959"
            "f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47"
            "d08ffb10d4b8022100fffffffffffffffffffffffffffffffebaaedce6af48a03b"
            "bfd25e8cd0364141020101a144034200044b2938fbc38071f24bede21e838a0758"
            "a52a0085f2e034e7f971df445436a252467f692ec9c5ba7e5eaa898ab99cbd9949"
            "496f7e3cafbf56304b1cc2e5bdf06e");

        CKey key;
        key.Load(CPrivKey(private_key.begin(), private_key.end()), CPubKey(), true);

        return key;
    }

    //!
    //! \brief Create a valid public key for tests.
    //!
    static CPubKey Public()
    {
        return CPubKey(std::vector<unsigned char> {
            0x04, 0x4b, 0x29, 0x38, 0xfb, 0xc3, 0x80, 0x71, 0xf2, 0x4b, 0xed,
            0xe2, 0x1e, 0x83, 0x8a, 0x07, 0x58, 0xa5, 0x2a, 0x00, 0x85, 0xf2,
            0xe0, 0x34, 0xe7, 0xf9, 0x71, 0xdf, 0x44, 0x54, 0x36, 0xa2, 0x52,
            0x46, 0x7f, 0x69, 0x2e, 0xc9, 0xc5, 0xba, 0x7e, 0x5e, 0xaa, 0x89,
            0x8a, 0xb9, 0x9c, 0xbd, 0x99, 0x49, 0x49, 0x6f, 0x7e, 0x3c, 0xaf,
            0xbf, 0x56, 0x30, 0x4b, 0x1c, 0xc2, 0xe5, 0xbd, 0xf0, 0x6e
        });
    }

    //!
    //! \brief Create a key ID from the test public key.
    //!
    static CKeyID KeyId()
    {
        return Public().GetID();
    }

    //!
    //! \brief Create an address from the test public key.
    //!
    static CBitcoinAddress Address()
    {
        return CBitcoinAddress(CTxDestination(KeyId()));
    }

    //!
    //! \brief Create a beacon verification code from the test public key.
    //!
    static std::string VerificationCode()
    {
        const CKeyID key_id = KeyId();

        return EncodeBase58(key_id.begin(), key_id.end());
    }

    static GRC::Cpid Cpid()
    {
        return GRC::Cpid::Parse("00010203040506070809101112131415");
    }

    //!
    //! \brief Create a beacon payload signature signed by this private key.
    //!
    static std::vector<uint8_t> Signature()
    {
        CHashWriter hasher(SER_NETWORK, PROTOCOL_VERSION);

        hasher
            << GRC::BeaconPayload::CURRENT_VERSION
            << GRC::Beacon(Public())
            << Cpid();

        std::vector<uint8_t> signature;
        CKey private_key = Private();

        private_key.Sign(hasher.GetHash(), signature);

        return signature;
    }

    //!
    //! \brief Create a beacon payload signature signed by this private key.
    //!
    static std::vector<uint8_t> Signature(GRC::BeaconPayload payload)
    {
        CHashWriter hasher(SER_NETWORK, PROTOCOL_VERSION);

        hasher
            << payload.m_version
            << payload.m_beacon
            << payload.m_cpid;

        std::vector<uint8_t> signature;
        CKey private_key = Private();

        private_key.Sign(hasher.GetHash(), signature);

        return signature;
    }

}; // TestKey


class BeaconRegistryTest
{
public:
    BeaconRegistryTest(CDataStream beacon_data
                       ,int64_t high_height_time
                       ,int low_height
                       ,int high_height
                       ,int num_blocks) :
        m_high_height_time_check(high_height_time),
        m_low_height_check(low_height),
        m_high_height_check(high_height),
        m_num_blocks_check(num_blocks)
    {
        GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();

        // Make sure the registry is reset.
        registry.Reset();

        beacon_data >> m_high_height_time;
        beacon_data >> m_low_height;
        beacon_data >> m_high_height;
        beacon_data >> m_num_blocks;

        // Import the blocks in the file and replay the relevant contracts.
        for (int i = 0; i < m_num_blocks; ++i)
        {
            BOOST_TEST_CHECKPOINT("Processing block = " << i);

            GRC::ExportContractElement element;

            beacon_data >> element;

            uint256 block_hash = element.m_disk_block_index.GetBlockHash();

            // Construct block index object. This comes from the guts of CtxDB::LoadBlockIndex()
            CBlockIndex* pindex    = GRC::MockBlockIndex::InsertBlockIndex(block_hash);
            // Note the mock CBlockIndex objects created here are SPARSE; therefore the blocks
            // pointed to by the pprev and pnext hashes will more than likely NOT be present here,
            // and are not needed anyway for this test, so ensure set to nullptr.
            pindex->pprev          = nullptr;
            pindex->pnext          = nullptr;
            pindex->nFile          = element.m_disk_block_index.nFile;
            pindex->nBlockPos      = element.m_disk_block_index.nBlockPos;
            pindex->nHeight        = element.m_disk_block_index.nHeight;
            pindex->nMoneySupply   = element.m_disk_block_index.nMoneySupply;
            pindex->nFlags         = element.m_disk_block_index.nFlags;
            pindex->nStakeModifier = element.m_disk_block_index.nStakeModifier;
            pindex->hashProof      = element.m_disk_block_index.hashProof;
            pindex->nVersion       = element.m_disk_block_index.nVersion;
            pindex->hashMerkleRoot = element.m_disk_block_index.hashMerkleRoot;
            pindex->nTime          = element.m_disk_block_index.nTime;
            pindex->nBits          = element.m_disk_block_index.nBits;
            pindex->nNonce         = element.m_disk_block_index.nNonce;
            pindex->m_researcher   = element.m_disk_block_index.m_researcher;

            // Update hashBestChain to fixup global for BeaconRegistry::Initialize call.
            hashBestChain = block_hash;

            // Import and apply all of the contracts from the file for the given block.
            for (const auto& iter : element.m_ctx)
            {
                // ----------------------- contract ------- tx
                GRC::ContractContext ctx({iter.first, iter.second, pindex});

                // This is the "thin" version of g_dispatcher.Apply in GRC::ApplyContracts for beacons.
                if (ctx->m_action == GRC::ContractAction::ADD)
                {
                    registry.Add(ctx);

                    GRC::Beacon_ptr beacon = registry.FindHistorical(ctx.m_tx.GetHash());

                    if (beacon != nullptr) {
                        std::cout << "add beacon record: "
                                  << "blockheight = " << ctx.m_pindex->nHeight
                                  << ", cpid = " << beacon->m_cpid.ToString()
                                  << ", public key = " << HexStr(beacon->m_public_key)
                                  << ", address = " << beacon->GetAddress().ToString()
                                  << ", timestamp = " << beacon->m_timestamp
                                  << ", hash = " << beacon->m_hash.GetHex()
                                  << ", prev beacon hash = " << beacon->m_previous_hash.GetHex()
                                  << ", status = " << beacon->StatusToString()
                                  << std::endl;
                    }
                }

                if (ctx->m_action == GRC::ContractAction::REMOVE)
                {
                    registry.Delete(ctx);

                    GRC::Beacon_ptr beacon = registry.FindHistorical(ctx.m_tx.GetHash());

                    if (beacon != nullptr) {
                        std::cout << "delete beacon record: "
                                  << "blockheight = " << ctx.m_pindex->nHeight
                                  << ", cpid = " << beacon->m_cpid.ToString()
                                  << ", public key = " << HexStr(beacon->m_public_key)
                                  << ", address = " << beacon->GetAddress().ToString()
                                  << ", timestamp = " << beacon->m_timestamp
                                  << ", hash = " << beacon->m_hash.GetHex()
                                  << ", prev beacon hash = " << beacon->m_previous_hash.GetHex()
                                  << ", status = " << beacon->StatusToString()
                                  << std::endl;
                    }
                }
            }

            // Activate the pending beacons that are now verified, and also mark expired pending beacons expired.
            if (pindex->IsSuperblock())
            {
                std::vector<uint256> pending_beacon_hashes;

                for (const auto& iter : element.m_verified_beacons) {
                    auto found_beacon_iter = registry.PendingBeacons().find(iter);

                    if (found_beacon_iter != registry.PendingBeacons().end()) {
                        pending_beacon_hashes.push_back(found_beacon_iter->second->m_hash);
                    }
                }

                registry.ActivatePending(element.m_verified_beacons,
                                         pindex->nTime,
                                         block_hash,
                                         pindex->nHeight);

                for (const auto& iter : pending_beacon_hashes) {
                    uint256 activated_beacon_hash = Hash(pindex->GetBlockHash(), iter);

                    GRC::Beacon_ptr activated_beacon = registry.FindHistorical(activated_beacon_hash);

                    if (activated_beacon != nullptr) {
                        std::cout << "activated beacon record: "
                                  << "blockheight = " << pindex->nHeight
                                  << ", cpid = " << activated_beacon->m_cpid.ToString()
                                  << ", public key = " << HexStr(activated_beacon->m_public_key)
                                  << ", address = " << activated_beacon->GetAddress().ToString()
                                  << ", timestamp = " << activated_beacon->m_timestamp
                                  << ", hash = " << activated_beacon->m_hash.GetHex()
                                  << ", prev beacon hash = " << activated_beacon->m_previous_hash.GetHex()
                                  << ", status = " << activated_beacon->StatusToString()
                                  << std::endl;
                    }
                }

                for (const auto& iter : registry.ExpiredBeacons()) {
                    if (iter != nullptr) {
                        std::cout << "expired beacon record: "
                                  << "blockheight = " << pindex->nHeight
                                  << ", cpid = " << iter->m_cpid.ToString()
                                  << ", public key = " << HexStr(iter->m_public_key)
                                  << ", address = " << iter->GetAddress().ToString()
                                  << ", timestamp = " << iter->m_timestamp
                                  << ", hash = " << iter->m_hash.GetHex()
                                  << ", prev beacon hash = " << iter->m_previous_hash.GetHex()
                                  << ", status = " << iter->StatusToString()
                                  << std::endl;
                    }

                }
            }
        }

        // Passivate the beacon db to remove unnecessary historical elements in memory.
        registry.PassivateDB();

        for (const auto& iter : registry.Beacons())
        {
            m_beacons_init[iter.first] = *iter.second;
        }

        m_init_number_beacons = m_beacons_init.size();

        for (const auto& iter : registry.PendingBeacons())
        {
            m_pending_beacons_init[iter.first] = *iter.second;
        }

        m_init_number_pending_beacons = m_pending_beacons_init.size();

        m_init_beacon_db_size = registry.GetBeaconDB().size();

        auto& init_beacon_db = registry.GetBeaconDB();

        auto init_beacon_db_iter = init_beacon_db.begin();
        while (init_beacon_db_iter != init_beacon_db.end())
        {
            const uint256& hash = init_beacon_db_iter->first;
            const GRC::Beacon_ptr& beacon_ptr = init_beacon_db_iter->second;

            // Create a copy of the referenced beacon object with a shared pointer to it and store.
            m_local_historical_beacon_map_init[hash] = std::make_shared<GRC::Beacon>(*beacon_ptr);

            std::cout << "init beacon db record: "
                      << ", cpid = " << beacon_ptr->m_cpid.ToString()
                      << ", public key = " << HexStr(beacon_ptr->m_public_key)
                      << ", address = " << beacon_ptr->GetAddress().ToString()
                      << ", timestamp = " << beacon_ptr->m_timestamp
                      << ", hash = " << beacon_ptr->m_hash.GetHex()
                      << ", prev beacon hash = " << beacon_ptr->m_previous_hash.GetHex()
                      << ", status = " << beacon_ptr->StatusToString()
                      << std::endl;

            init_beacon_db_iter = init_beacon_db.advance(init_beacon_db_iter);
        }

        // Reinitialize from LevelDB to do comparison checks for reinit integrity.

        // Reset in memory structures only (which leaves LevelDB undisturbed).
        registry.ResetInMemoryOnly();

        // Reinitialize from LevelDB.
        registry.Initialize();

        // Passivate the beacon db to remove unnecessary historical elements in memory.
        registry.PassivateDB();

        for (const auto& iter : registry.Beacons())
        {
            m_beacons_reinit[iter.first] = *iter.second;
        }

        m_reinit_number_beacons = m_beacons_reinit.size();

        for (const auto& iter : registry.PendingBeacons())
        {
            m_pending_beacons_reinit[iter.first] = *iter.second;
        }

        m_reinit_number_pending_beacons = m_pending_beacons_reinit.size();

        m_reinit_beacon_db_size = registry.GetBeaconDB().size();

        auto& reinit_beacon_db = registry.GetBeaconDB();

        auto reinit_beacon_db_iter = reinit_beacon_db.begin();
        while (reinit_beacon_db_iter != reinit_beacon_db.end())
        {
            const uint256& hash = reinit_beacon_db_iter->first;
            const GRC::Beacon_ptr& beacon_ptr = reinit_beacon_db_iter->second;

            // Create a copy of the referenced beacon object with a shared pointer to it and store.
            m_local_historical_beacon_map_reinit[hash] = std::make_shared<GRC::Beacon>(*beacon_ptr);

            std::cout << "init beacon db record: "
                      << ", cpid = " << beacon_ptr->m_cpid.ToString()
                      << ", public key = " << HexStr(beacon_ptr->m_public_key)
                      << ", address = " << beacon_ptr->GetAddress().ToString()
                      << ", timestamp = " << beacon_ptr->m_timestamp
                      << ", hash = " << beacon_ptr->m_hash.GetHex()
                      << ", prev beacon hash = " << beacon_ptr->m_previous_hash.GetHex()
                      << ", status = " << beacon_ptr->StatusToString()
                      << std::endl;

            reinit_beacon_db_iter = reinit_beacon_db.advance(reinit_beacon_db_iter);
        }
    };

    void RunBasicChecks()
    {
        // These should be set to correspond to the dumpcontracts run used to create testnet_beacon.bin
        BOOST_CHECK(m_high_height_time == m_high_height_time_check);
        BOOST_CHECK(m_low_height == m_low_height_check);
        BOOST_CHECK(m_high_height == m_high_height_check);
        BOOST_CHECK(m_num_blocks == m_num_blocks_check);

        BOOST_TEST_CHECKPOINT("init_beacon_db_size = " << m_init_beacon_db_size << ", "
                              << "reinit_beacon_db_size = " << m_reinit_beacon_db_size);

        BOOST_CHECK_EQUAL(m_init_beacon_db_size, m_reinit_beacon_db_size);
    };

    void BeaconDatabaseComparisonChecks_m_historical()
    {
        // m_historical checks

        BOOST_CHECK_EQUAL(m_local_historical_beacon_map_init.size(), m_local_historical_beacon_map_reinit.size());

        bool historical_beacon_db_comparison_success = true;

        // left join with init on the left
        for (const auto& left : m_local_historical_beacon_map_init)
        {
            uint256 hash = left.first;
            GRC::Beacon_ptr left_beacon_ptr = left.second;

            auto right_beacon_iter = m_local_historical_beacon_map_reinit.find(hash);

            if (right_beacon_iter == m_local_historical_beacon_map_reinit.end())
            {
                BOOST_TEST_CHECKPOINT("beacon in init beacon db not found in reinit beacon db for cpid "
                                      << left_beacon_ptr->m_cpid.ToString());

                historical_beacon_db_comparison_success = false;

                std::cout << "MISSING: Reinit record missing for init record: "
                          << "hash = " << hash.GetHex()
                          << ", cpid = " << left.second->m_cpid.ToString()
                          << ", public key = " << HexStr(left.second->m_public_key)
                          << ", address = " << left.second->GetAddress().ToString()
                          << ", timestamp = " << left.second->m_timestamp
                          << ", hash = " << left.second->m_hash.GetHex()
                          << ", prev beacon hash = " << left.second->m_previous_hash.GetHex()
                          << ", status = " << left.second->StatusToString()
                          << std::endl;

            }
            else if (*left_beacon_ptr != *right_beacon_iter->second)
            {
                BOOST_TEST_CHECKPOINT("beacon in init beacon db does not match corresponding beacon"
                                      " in reinit beacon db for cpid "
                                      << left_beacon_ptr->m_cpid.ToString());

                historical_beacon_db_comparison_success = false;

                // This is for console output in case the test fails and you run test_gridcoin manually from the command line.
                // You should be in the src directory for that, so the command would be ./test/test_gridcoin.
                std::cout << "MISMATCH: beacon in reinit beacon db does not match corresponding beacon"
                             " in init beacon db for hash = " << hash.GetHex() << std::endl;

                std::cout << "cpid = " << left_beacon_ptr->m_cpid.ToString() << std::endl;

                std::cout << "init_beacon public key = " << HexStr(left_beacon_ptr->m_public_key)
                          << ", reinit_beacon public key = " << HexStr(right_beacon_iter->second->m_public_key) << std::endl;

                std::cout << "init_beacon address = " << left_beacon_ptr->GetAddress().ToString()
                          << ", reinit_beacon address = " << right_beacon_iter->second->GetAddress().ToString() << std::endl;

                std::cout << "init_beacon timestamp = " << left_beacon_ptr->m_timestamp
                          << ", reinit_beacon timestamp = " << right_beacon_iter->second->m_timestamp << std::endl;

                std::cout << "init_beacon hash = " << left_beacon_ptr->m_hash.GetHex()
                          << ", reinit_beacon hash = " << right_beacon_iter->second->m_hash.GetHex() << std::endl;

                std::cout << "init_beacon prev beacon hash = " << left_beacon_ptr->m_previous_hash.GetHex()
                          << ", reinit_beacon prev beacon hash = " << right_beacon_iter->second->m_previous_hash.GetHex() << std::endl;

                std::cout << "init_beacon status = " << left_beacon_ptr->StatusToString()
                          << ", reinit_beacon status = " << right_beacon_iter->second->StatusToString() << std::endl;
            }
        }

        // left join with reinit on the left
        for (const auto& left : m_local_historical_beacon_map_reinit)
        {
            uint256 hash = left.first;
            GRC::Beacon_ptr left_beacon_ptr = left.second;

            auto right_beacon_iter = m_local_historical_beacon_map_init.find(hash);

            if (right_beacon_iter == m_local_historical_beacon_map_init.end())
            {
                BOOST_TEST_CHECKPOINT("beacon in reinit beacon db not found in init beacon db for cpid "
                                      << left_beacon_ptr->m_cpid.ToString());

                historical_beacon_db_comparison_success = false;

                std::cout << "MISSING: init record missing for reinit record: "
                          << "hash = " << hash.GetHex()
                          << ", cpid = " << left.second->m_cpid.ToString()
                          << ", public key = " << HexStr(left.second->m_public_key)
                          << ", address = " << left.second->GetAddress().ToString()
                          << ", timestamp = " << left.second->m_timestamp
                          << ", hash = " << left.second->m_hash.GetHex()
                          << ", prev beacon hash = " << left.second->m_previous_hash.GetHex()
                          << ", status = " << left.second->StatusToString()
                          << std::endl;

            }
            else if (*left_beacon_ptr != *right_beacon_iter->second)
            {
                BOOST_TEST_CHECKPOINT("beacon in init beacon db does not match corresponding beacon"
                                      " in reinit beacon db for cpid "
                                      << left_beacon_ptr->m_cpid.ToString());

                historical_beacon_db_comparison_success = false;

                // This is for console output in case the test fails and you run test_gridcoin manually from the command line.
                // You should be in the src directory for that, so the command would be ./test/test_gridcoin.
                std::cout << "MISMATCH: beacon in init beacon db does not match corresponding beacon"
                             " in reinit beacon db for hash = " << hash.GetHex() << std::endl;

                std::cout << "cpid = " << left_beacon_ptr->m_cpid.ToString() << std::endl;

                std::cout << "reinit_beacon public key = " << HexStr(left_beacon_ptr->m_public_key)
                          << ", init_beacon public key = " << HexStr(right_beacon_iter->second->m_public_key) << std::endl;

                std::cout << "reinit_beacon address = " << left_beacon_ptr->GetAddress().ToString()
                          << ", init_beacon address = " << right_beacon_iter->second->GetAddress().ToString() << std::endl;

                std::cout << "reinit_beacon timestamp = " << left_beacon_ptr->m_timestamp
                          << ", init_beacon timestamp = " << right_beacon_iter->second->m_timestamp << std::endl;

                std::cout << "reinit_beacon hash = " << left_beacon_ptr->m_hash.GetHex()
                          << ", init_beacon hash = " << right_beacon_iter->second->m_hash.GetHex() << std::endl;

                std::cout << "reinit_beacon prev beacon hash = " << left_beacon_ptr->m_previous_hash.GetHex()
                          << ", init_beacon prev beacon hash = " << right_beacon_iter->second->m_previous_hash.GetHex() << std::endl;

                std::cout << "reinit_beacon status = " << left_beacon_ptr->StatusToString()
                          << ", init_beacon status = " << right_beacon_iter->second->StatusToString() << std::endl;
            }
        }

        BOOST_CHECK(historical_beacon_db_comparison_success);
    };

    void BeaconDatabaseComparisonChecks_m_beacons()
    {
        BOOST_TEST_CHECKPOINT("init_number_beacons = " << m_init_number_beacons << ", "
                              << "reinit_number_beacons = " << m_reinit_number_beacons);

        bool number_beacons_equal = (m_init_number_beacons == m_reinit_number_beacons);

        if (!number_beacons_equal)
        {
            for (const auto& iter : m_beacons_init)
            {
                const GRC::Cpid& cpid = iter.first;
                const GRC::Beacon& beacon = iter.second;

                // This is for console output in case the test fails and you run test_gridcoin manually from the command line.
                // You should be in the src directory for that, so the command would be ./test/test_gridcoin.
                std::cout << "init_beacon cpid = " << cpid.ToString()
                          << ", public key = " << HexStr(beacon.m_public_key)
                          << ", address = " << beacon.GetAddress().ToString()
                          << ", timestamp = " << beacon.m_timestamp
                          << ", hash = " << beacon.m_hash.GetHex()
                          << ", prev beacon hash = " << beacon.m_previous_hash.GetHex()
                          << ", status = " << beacon.StatusToString()
                          << std::endl;
            }

            for (const auto& iter : m_beacons_reinit)
            {
                const GRC::Cpid& cpid = iter.first;
                const GRC::Beacon& beacon = iter.second;

                // This is for console output in case the test fails and you run test_gridcoin manually from the command line.
                // You should be in the src directory for that, so the command would be ./test/test_gridcoin.
                std::cout << "reinit beacon cpid = " << cpid.ToString()
                          << ", public key = " << HexStr(beacon.m_public_key)
                          << ", address = " << beacon.GetAddress().ToString()
                          << ", timestamp = " << beacon.m_timestamp
                          << ", hash = " << beacon.m_hash.GetHex()
                          << ", prev beacon hash = " << beacon.m_previous_hash.GetHex()
                          << ", status = " << beacon.StatusToString()
                          << std::endl;
            }
        }

        BOOST_CHECK_EQUAL(m_init_number_beacons, m_reinit_number_beacons);

        bool beacon_comparison_success = true;

        // left join with init on the left
        for (const auto& left : m_beacons_init)
        {
            GRC::Beacon left_beacon = left.second;
            auto right = m_beacons_reinit.find(left.first);

            if (right == m_beacons_reinit.end())
            {
                BOOST_TEST_CHECKPOINT("MISSING: beacon in init not found in reinit for cpid "
                                      << left.first.ToString());
                beacon_comparison_success = false;

                std::cout << "MISSING: reinit beacon record missing for init beacon record: "
                          << "hash = " << left.second.m_hash.GetHex()
                          << ", cpid = " << left.second.m_cpid.ToString()
                          << ", public key = " << HexStr(left.second.m_public_key)
                          << ", address = " << left.second.GetAddress().ToString()
                          << ", timestamp = " << left.second.m_timestamp
                          << ", hash = " << left.second.m_hash.GetHex()
                          << ", prev beacon hash = " << left.second.m_previous_hash.GetHex()
                          << ", status = " << left.second.StatusToString()
                          << std::endl;
            }
            else if (left_beacon != right->second)
            {
                BOOST_TEST_CHECKPOINT("MISMATCH: beacon in reinit mismatches init for cpid "
                                      << left.first.ToString());
                beacon_comparison_success = false;

                // This is for console output in case the test fails and you run test_gridcoin manually from the command line.
                // You should be in the src directory for that, so the command would be ./test/test_gridcoin.
                std::cout << "MISMATCH: beacon in reinit mismatches init for cpid = "
                          << left_beacon.m_cpid.ToString() << std::endl;

                std::cout << "init_beacon public key = " << HexStr(left_beacon.m_public_key)
                          << ", reinit_beacon public key = " << HexStr(right->second.m_public_key) << std::endl;

                std::cout << "init_beacon timestamp = " << left_beacon.m_timestamp
                          << ", reinit_beacon timestamp = " << right->second.m_timestamp << std::endl;

                std::cout << "init_beacon hash = " << left_beacon.m_hash.GetHex()
                          << ", reinit_beacon hash = " << right->second.m_hash.GetHex() << std::endl;

                std::cout << "init_beacon prev beacon hash = " << left_beacon.m_previous_hash.GetHex()
                          << ", reinit_beacon prev beacon hash = " << right->second.m_previous_hash.GetHex() << std::endl;

                std::cout << "init_beacon status = " << left_beacon.StatusToString()
                          << ", reinit_beacon status = " << right->second.StatusToString() << std::endl;
            }
        }


        // left join with reinit on the left
        for (const auto& left : m_beacons_reinit)
        {
            GRC::Beacon left_beacon = left.second;

            auto right = m_beacons_init.find(left.first);

            if (right == m_beacons_reinit.end())
            {
                BOOST_TEST_CHECKPOINT("MISSING: beacon in reinit not found in init for cpid "
                                      << left.first.ToString());
                beacon_comparison_success = false;

                std::cout << "MISSING: init beacon record missing for reinit beacon record: "
                          << "hash = " << left.second.m_hash.GetHex()
                          << ", cpid = " << left.second.m_cpid.ToString()
                          << ", public key = " << HexStr(left.second.m_public_key)
                          << ", address = " << left.second.GetAddress().ToString()
                          << ", timestamp = " << left.second.m_timestamp
                          << ", hash = " << left.second.m_hash.GetHex()
                          << ", prev beacon hash = " << left.second.m_previous_hash.GetHex()
                          << ", status = " << left.second.StatusToString()
                          << std::endl;

            }
            else if (left_beacon != right->second)
            {
                BOOST_TEST_CHECKPOINT("MISMATCH: beacon in init mismatches reinit for cpid "
                                      << left.first.ToString());
                beacon_comparison_success = false;

                // This is for console output in case the test fails and you run test_gridcoin manually from the command line.
                // You should be in the src directory for that, so the command would be ./test/test_gridcoin.
                std::cout << "MISMATCH: beacon in reinit mismatches init for cpid = "
                          << left_beacon.m_cpid.ToString() << std::endl;

                std::cout << "reinit_beacon public key = " << HexStr(left_beacon.m_public_key)
                          << ", init_beacon public key = " << HexStr(right->second.m_public_key) << std::endl;

                std::cout << "reinit_beacon timestamp = " << left_beacon.m_timestamp
                          << ", init_beacon timestamp = " << right->second.m_timestamp << std::endl;

                std::cout << "reinit_beacon hash = " << left_beacon.m_hash.GetHex()
                          << ", init_beacon hash = " << right->second.m_hash.GetHex() << std::endl;

                std::cout << "reinit_beacon prev beacon hash = " << left_beacon.m_previous_hash.GetHex()
                          << ", init_beacon prev beacon hash = " << right->second.m_previous_hash.GetHex() << std::endl;

                std::cout << "reinit_beacon status = " << left_beacon.StatusToString()
                          << ", init_beacon status = " << right->second.StatusToString() << std::endl;
            }
        }

        BOOST_CHECK(beacon_comparison_success);
    };

    void BeaconDatabaseComparisonChecks_m_pending()
    {
        BOOST_TEST_CHECKPOINT("init_number_pending_beacons.size() = " << m_init_number_pending_beacons << ", "
                              << "reinit_number_pending_beacons.size() = " << m_reinit_number_pending_beacons);

        BOOST_CHECK_EQUAL(m_init_number_pending_beacons, m_reinit_number_pending_beacons);

        bool pending_beacon_comparison_success = true;

        // left join with init on the left
        for (const auto& left : m_pending_beacons_init)
        {
            GRC::Beacon left_beacon = left.second;
            auto right = m_pending_beacons_reinit.find(left.first);

            if (right == m_pending_beacons_reinit.end())
            {
                BOOST_TEST_CHECKPOINT("MISSING: pending beacon in init not found in reinit for CKeyID "
                                      << left.first.ToString());
                pending_beacon_comparison_success = false;

                std::cout << "MISSING: reinit pending beacon record missing for init pending beacon record: "
                          << "hash = " << left_beacon.m_hash.GetHex()
                          << ", cpid = " << left_beacon.m_cpid.ToString()
                          << ", public key = " << HexStr(left_beacon.m_public_key)
                          << ", address = " << left_beacon.GetAddress().ToString()
                          << ", timestamp = " << left_beacon.m_timestamp
                          << ", hash = " << left_beacon.m_hash.GetHex()
                          << ", prev beacon hash = " << left_beacon.m_previous_hash.GetHex()
                          << ", status = " << left_beacon.StatusToString()
                          << std::endl;
            }
            else if (left_beacon != right->second)
            {
                BOOST_TEST_CHECKPOINT("MISMATCH: beacon in reinit mismatches init for CKeyID "
                                      << left.first.ToString());
                pending_beacon_comparison_success = false;

                // This is for console output in case the test fails and you run test_gridcoin manually from the command line.
                // You should be in the src directory for that, so the command would be ./test/test_gridcoin.
                std::cout << "MISMATCH: beacon in reinit mismatches init for CKeyID "
                          << left.first.ToString() << std::endl;

                std::cout << "init_pending_beacon cpid = " << left_beacon.m_cpid.ToString()
                          << ", reinit_pending_beacon cpid = " << right->second.m_cpid.ToString() << std::endl;

                std::cout << "init_pending_beacon public key = " << HexStr(left_beacon.m_public_key)
                          << ", reinit_pending_beacon public key = " << HexStr(right->second.m_public_key) << std::endl;

                std::cout << "init_pending_beacon timestamp = " << left_beacon.m_timestamp
                          << ", reinit_pending_beacon timestamp = " << right->second.m_timestamp << std::endl;

                std::cout << "init_pending_beacon hash = " << left_beacon.m_hash.GetHex()
                          << ", reinit_pending_beacon hash = " << right->second.m_hash.GetHex() << std::endl;

                std::cout << "init_pending_beacon prev beacon hash = " << left_beacon.m_previous_hash.GetHex()
                          << ", reinit_pending_beacon prev beacon hash = " << right->second.m_previous_hash.GetHex()
                          << std::endl;

                std::cout << ", init_pending_beacon status = " << left_beacon.StatusToString()
                          << ", reinit_pending_beacon status = " << right->second.StatusToString() << std::endl;
            }
        }

        // left join with reinit on the left
        for (const auto& left : m_pending_beacons_reinit)
        {
            GRC::Beacon left_beacon = left.second;
            auto right = m_pending_beacons_init.find(left.first);

            if (right == m_pending_beacons_init.end())
            {
                BOOST_TEST_CHECKPOINT("MISSING: pending beacon in reinit not found in init for CKeyID "
                                      << left.first.ToString());
                pending_beacon_comparison_success = false;

                std::cout << "MISSING: init pending beacon record missing for reinit pending beacon record: "
                          << "hash = " << left.second.m_hash.GetHex()
                          << ", cpid = " << left.second.m_cpid.ToString()
                          << ", public key = " << HexStr(left.second.m_public_key)
                          << ", address = " << left.second.GetAddress().ToString()
                          << ", timestamp = " << left.second.m_timestamp
                          << ", hash = " << left.second.m_hash.GetHex()
                          << ", prev beacon hash = " << left.second.m_previous_hash.GetHex()
                          << ", status = " << left.second.StatusToString()
                          << std::endl;
            }
            else if (left_beacon != right->second)
            {
                BOOST_TEST_CHECKPOINT("MISMATCH: beacon in reinit mismatches init for CKeyID "
                                      << left.first.ToString());
                pending_beacon_comparison_success = false;

                // This is for console output in case the test fails and you run test_gridcoin manually from the command line.
                // You should be in the src directory for that, so the command would be ./test/test_gridcoin.
                std::cout << "MISMATCH: beacon in reinit mismatches init for CKeyID "
                          << left.first.ToString() << std::endl;

                std::cout << "init_pending_beacon cpid = " << left_beacon.m_cpid.ToString()
                          << ", reinit_pending_beacon cpid = " << right->second.m_cpid.ToString() << std::endl;

                std::cout << "init_pending_beacon public key = " << HexStr(left_beacon.m_public_key)
                          << ", reinit_pending_beacon public key = " << HexStr(right->second.m_public_key) << std::endl;

                std::cout << "init_pending_beacon timestamp = " << left_beacon.m_timestamp
                          << ", reinit_pending_beacon timestamp = " << right->second.m_timestamp << std::endl;

                std::cout << "init_pending_beacon hash = " << left_beacon.m_hash.GetHex()
                          << ", reinit_pending_beacon hash = " << right->second.m_hash.GetHex() << std::endl;

                std::cout << "init_pending_beacon prev beacon hash = " << left_beacon.m_previous_hash.GetHex()
                          << ", reinit_pending_beacon prev beacon hash = " << right->second.m_previous_hash.GetHex()
                          << std::endl;

                std::cout << ", init_pending_beacon status = " << left_beacon.StatusToString()
                          << ", reinit_pending_beacon status = " << right->second.StatusToString() << std::endl;
            }
        }

        BOOST_CHECK(pending_beacon_comparison_success);
    };

    int64_t m_high_height_time = 0;
    int m_low_height = 0;
    int m_high_height = 0;
    int m_num_blocks = 0;

    int64_t m_high_height_time_check = 0;
    int m_low_height_check = 0;
    int m_high_height_check = 0;
    int m_num_blocks_check = 0;

    // Record the map of beacons and pending beacons after the contract replay. We have to have independent storage
    // of these, not pointers, because the maps are going to get reset for the second run (reinit).
    typedef std::unordered_map<GRC::Cpid, GRC::Beacon> LocalBeaconMap;
    typedef std::map<CKeyID, GRC::Beacon> LocalPendingBeaconMap;

    LocalBeaconMap m_beacons_init;
    LocalPendingBeaconMap m_pending_beacons_init;
    GRC::BeaconRegistry::HistoricalBeaconMap m_local_historical_beacon_map_init;
    size_t m_init_number_beacons = 0;
    size_t m_init_number_pending_beacons = 0;
    size_t m_init_beacon_db_size = 0;

    LocalBeaconMap m_beacons_reinit;
    LocalPendingBeaconMap m_pending_beacons_reinit;
    GRC::BeaconRegistry::HistoricalBeaconMap m_local_historical_beacon_map_reinit;
    size_t m_reinit_number_beacons = 0;
    size_t m_reinit_number_pending_beacons = 0;
    size_t m_reinit_beacon_db_size = 0;
};

} // anonymous namespace

// -----------------------------------------------------------------------------
// Beacon
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Beacon)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_empty_invalid_beacon)
{
    const GRC::Beacon beacon;

    BOOST_CHECK(!beacon.m_public_key.size());
    BOOST_CHECK_EQUAL(beacon.m_timestamp, 0);

    BOOST_CHECK(beacon.WellFormed() == false);
}

BOOST_AUTO_TEST_CASE(it_initializes_with_a_public_key)
{
    const GRC::Beacon beacon(TestKey::Public());

    BOOST_CHECK(beacon.m_public_key == TestKey::Public());
    BOOST_CHECK_EQUAL(beacon.m_timestamp, 0);

    BOOST_CHECK(beacon.WellFormed() == true);
}

BOOST_AUTO_TEST_CASE(it_parses_a_beacon_from_a_legacy_contract_value)
{
    const std::string legacy = EncodeBase64(
        "Unused CPID field;"
        "Unused random hex field;"
        "Unused rain address field;"
        + HexStr(TestKey::Public()));

    const GRC::Beacon beacon = GRC::Beacon::Parse(legacy);

    BOOST_CHECK(beacon.m_public_key == TestKey::Public());
    BOOST_CHECK_EQUAL(beacon.m_timestamp, 0);

    BOOST_CHECK(beacon.WellFormed() == true);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_the_beacon_is_well_formed)
{
    const GRC::Beacon valid(TestKey::Public());
    BOOST_CHECK(valid.WellFormed() == true);

    const GRC::Beacon invalid_empty;
    BOOST_CHECK(invalid_empty.WellFormed() == false);

    const GRC::Beacon invalid_bad_key(CPubKey(ParseHex("12345")));
    BOOST_CHECK(invalid_bad_key.WellFormed() == false);
}

BOOST_AUTO_TEST_CASE(it_calculates_the_age_of_the_beacon)
{
    const int64_t now = 100;
    const GRC::Beacon beacon(CPubKey(), 99, uint256 {});

    BOOST_CHECK_EQUAL(beacon.Age(now), 1);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_a_beacon_expired)
{
    const GRC::Beacon valid(CPubKey(), GRC::Beacon::MAX_AGE, uint256 {});
    BOOST_CHECK(valid.Expired(GRC::Beacon::MAX_AGE) == false);

    const GRC::Beacon almost(CPubKey(), 1, uint256 {});
    BOOST_CHECK(almost.Expired(GRC::Beacon::MAX_AGE + 1) == false);

    const GRC::Beacon expired(CPubKey(), 1, uint256 {});
    BOOST_CHECK(expired.Expired(GRC::Beacon::MAX_AGE + 2) == true);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_a_beacon_is_renewable)
{
    const GRC::Beacon not_needed(CPubKey(), GRC::Beacon::RENEWAL_AGE, uint256 {});
    BOOST_CHECK(not_needed.Renewable(GRC::Beacon::RENEWAL_AGE) == false);

    const GRC::Beacon almost(CPubKey(), 1, uint256 {});
    BOOST_CHECK(almost.Renewable(GRC::Beacon::RENEWAL_AGE) == false);

    const GRC::Beacon renewable(CPubKey(), 1, uint256 {});
    BOOST_CHECK(renewable.Renewable(GRC::Beacon::RENEWAL_AGE + 2) == true);
}

BOOST_AUTO_TEST_CASE(it_produces_a_key_id)
{
    const GRC::Beacon beacon(TestKey::Public());

    BOOST_CHECK(beacon.GetId() == TestKey::KeyId());
}

BOOST_AUTO_TEST_CASE(it_produces_a_rain_address)
{
    const GRC::Beacon beacon(TestKey::Public());

    BOOST_CHECK(beacon.GetAddress() == TestKey::Address());
}

BOOST_AUTO_TEST_CASE(it_produces_a_verification_code)
{
    const GRC::Beacon beacon(TestKey::Public());

    BOOST_CHECK(beacon.GetVerificationCode() == TestKey::VerificationCode());
}

BOOST_AUTO_TEST_CASE(it_represents_itself_as_a_legacy_string)
{
    const GRC::Beacon beacon(TestKey::Public());

    const std::string expected = EncodeBase64(
        "0;0;"
        + TestKey::Address().ToString()
        + ";"
        + HexStr(TestKey::Public()));

    BOOST_CHECK_EQUAL(beacon.ToString(), expected);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream)
{
    const GRC::Beacon beacon(TestKey::Public());

    const CDataStream expected = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << TestKey::Public();

    const CDataStream stream = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << beacon;

    BOOST_CHECK(std::equal(stream.begin(), stream.end(), expected.begin(), expected.end()));
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream)
{
    CDataStream stream = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << TestKey::Public();

    GRC::Beacon beacon;
    stream >> beacon;

    BOOST_CHECK(beacon.m_public_key == TestKey::Public());
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------------
// BeaconPayload
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(BeaconPayload)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_empty_invalid_payload)
{
    const GRC::BeaconPayload payload;

    BOOST_CHECK_EQUAL(payload.m_version, GRC::BeaconPayload::CURRENT_VERSION);
    BOOST_CHECK(payload.m_cpid.IsZero() == true);
    BOOST_CHECK(!payload.m_beacon.m_public_key.size());
    BOOST_CHECK_EQUAL(payload.m_beacon.m_timestamp, 0);
}

BOOST_AUTO_TEST_CASE(it_initializes_with_beacon_contract_data)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    const GRC::BeaconPayload payload(cpid, GRC::Beacon(TestKey::Public()));

    BOOST_CHECK_EQUAL(payload.m_version, GRC::BeaconPayload::CURRENT_VERSION);
    BOOST_CHECK(payload.m_cpid == cpid);
    BOOST_CHECK(payload.m_beacon.m_public_key == TestKey::Public());
    BOOST_CHECK_EQUAL(payload.m_beacon.m_timestamp, 0);
}

BOOST_AUTO_TEST_CASE(it_parses_a_payload_from_a_legacy_contract_key_and_value)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");

    const std::string key = cpid.ToString();
    const std::string value = EncodeBase64(
        "Unused CPID field;"
        "Unused random hex field;"
        "Unused rain address field;"
        + HexStr(TestKey::Public()));

    const GRC::BeaconPayload payload = GRC::BeaconPayload::Parse(key, value);

    // Legacy beacon payloads always parse to version 1:
    BOOST_CHECK_EQUAL(payload.m_version, (uint32_t) 1);
    BOOST_CHECK(payload.m_cpid == cpid);
    BOOST_CHECK(payload.m_beacon.m_public_key == TestKey::Public());
    BOOST_CHECK_EQUAL(payload.m_beacon.m_timestamp, 0);
}

BOOST_AUTO_TEST_CASE(it_behaves_like_a_contract_payload)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    GRC::BeaconPayload payload(cpid, GRC::Beacon(TestKey::Public()));
    payload.m_signature = TestKey::Signature();

    BOOST_CHECK(payload.ContractType() == GRC::ContractType::BEACON);
    BOOST_CHECK(payload.WellFormed(GRC::ContractAction::ADD) == true);
    BOOST_CHECK(payload.LegacyKeyString() == cpid.ToString());
    BOOST_CHECK(payload.LegacyValueString() == payload.m_beacon.ToString());
    BOOST_CHECK(payload.RequiredBurnAmount() > 0);
}

BOOST_AUTO_TEST_CASE(it_checks_whether_the_payload_is_well_formed)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    GRC::BeaconPayload valid(cpid, GRC::Beacon(TestKey::Public()));
    valid.m_signature = TestKey::Signature();

    BOOST_CHECK(valid.WellFormed(GRC::ContractAction::ADD) == true);
    BOOST_CHECK(valid.WellFormed(GRC::ContractAction::REMOVE) == true);

    GRC::BeaconPayload zero_cpid{GRC::Cpid(), GRC::Beacon(TestKey::Public())};
    zero_cpid.m_signature = TestKey::Signature();

    // A zero CPID is technically valid...
    BOOST_CHECK(zero_cpid.WellFormed(GRC::ContractAction::ADD) == true);
    BOOST_CHECK(zero_cpid.WellFormed(GRC::ContractAction::REMOVE) == true);

    GRC::BeaconPayload missing_key(cpid, GRC::Beacon());
    missing_key.m_signature = TestKey::Signature();

    BOOST_CHECK(missing_key.WellFormed(GRC::ContractAction::ADD) == false);
    BOOST_CHECK(missing_key.WellFormed(GRC::ContractAction::REMOVE) == false);
}

BOOST_AUTO_TEST_CASE(it_checks_whether_a_legacy_v1_payload_is_well_formed)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    const GRC::Beacon beacon(TestKey::Public());

    const GRC::BeaconPayload add = GRC::BeaconPayload(1, cpid, beacon);

    BOOST_CHECK(add.WellFormed(GRC::ContractAction::ADD) == true);
    // Legacy beacon deletion contracts ignore the value:
    BOOST_CHECK(add.WellFormed(GRC::ContractAction::REMOVE) == true);

    const GRC::BeaconPayload remove = GRC::BeaconPayload(1, cpid, GRC::Beacon());

    BOOST_CHECK(remove.WellFormed(GRC::ContractAction::ADD) == false);
    BOOST_CHECK(remove.WellFormed(GRC::ContractAction::REMOVE) == true);
}

BOOST_AUTO_TEST_CASE(it_signs_the_payload)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    GRC::BeaconPayload payload(cpid, GRC::Beacon(TestKey::Public()));

    CKey private_key = TestKey::Private();

    BOOST_CHECK(payload.Sign(private_key));

    CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);
    payload.Serialize(hasher, GRC::ContractAction::UNKNOWN);

    BOOST_CHECK(TestKey::Public().Verify(hasher.GetHash(), payload.m_signature));
}

BOOST_AUTO_TEST_CASE(it_verifies_the_payload_signature)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    GRC::BeaconPayload payload(cpid, GRC::Beacon(TestKey::Public()));

    CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);
    payload.Serialize(hasher, GRC::ContractAction::UNKNOWN);

    CKey private_key = TestKey::Private();

    BOOST_CHECK(private_key.Sign(hasher.GetHash(), payload.m_signature));
    BOOST_CHECK(payload.VerifySignature());
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    const GRC::Beacon beacon(TestKey::Public());
    GRC::BeaconPayload payload(cpid, beacon);
    payload.m_signature = TestKey::Signature();

    const CDataStream expected = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << GRC::BeaconPayload::CURRENT_VERSION
        << cpid
        << beacon
        << payload.m_signature;

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    payload.Serialize(stream, GRC::ContractAction::ADD);

    BOOST_CHECK(std::equal(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end()));
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    const GRC::Beacon beacon(TestKey::Public());
    const std::vector<uint8_t> signature = TestKey::Signature();

    CDataStream stream_add = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << GRC::BeaconPayload::CURRENT_VERSION
        << cpid
        << beacon
        << signature;

    CDataStream stream_remove = stream_add;

    GRC::BeaconPayload payload;
    payload.Unserialize(stream_add, GRC::ContractAction::ADD);

    BOOST_CHECK_EQUAL(payload.m_version, GRC::BeaconPayload::CURRENT_VERSION);
    BOOST_CHECK(payload.m_cpid == cpid);
    BOOST_CHECK(payload.m_beacon.m_public_key == TestKey::Public());
    BOOST_CHECK_EQUAL(payload.m_beacon.m_timestamp, 0);

    BOOST_CHECK_EQUAL_COLLECTIONS(
        payload.m_signature.begin(),
        payload.m_signature.end(),
        signature.begin(),
        signature.end());

    BOOST_CHECK(payload.WellFormed(GRC::ContractAction::ADD) == true);

    payload = GRC::BeaconPayload();
    payload.Unserialize(stream_remove, GRC::ContractAction::REMOVE);

    BOOST_CHECK_EQUAL(payload.m_version, GRC::BeaconPayload::CURRENT_VERSION);
    BOOST_CHECK(payload.m_cpid == cpid);
    BOOST_CHECK(payload.m_beacon.m_public_key == TestKey::Public());
    BOOST_CHECK_EQUAL(payload.m_beacon.m_timestamp, 0);

    BOOST_CHECK_EQUAL_COLLECTIONS(
        payload.m_signature.begin(),
        payload.m_signature.end(),
        signature.begin(),
        signature.end());

    BOOST_CHECK(payload.WellFormed(GRC::ContractAction::REMOVE) == true);
}

BOOST_AUTO_TEST_CASE(beaconstorage_testnet_test)
{
    // These should be set to correspond to the dumpcontracts run used to create testnet_beacon.bin
    int64_t high_height_time = 1613880656;
    int low_height = 1301500;
    int high_height = 1497976;
    int num_blocks = 271;

    CDataStream data(SER_DISK, PROTOCOL_VERSION);

    data << testnet_beacon_bin;

    BeaconRegistryTest beacon_registry_test(data,
                                            high_height_time,
                                            low_height,
                                            high_height,
                                            num_blocks);

    beacon_registry_test.RunBasicChecks();

    beacon_registry_test.BeaconDatabaseComparisonChecks_m_historical();

    beacon_registry_test.BeaconDatabaseComparisonChecks_m_beacons();

    beacon_registry_test.BeaconDatabaseComparisonChecks_m_pending();
}


BOOST_AUTO_TEST_CASE(beaconstorage_mainnet_test)
{
    // These should be set to correspond to the dumpcontracts run used to create mainnet_beacon.bin
    int64_t high_height_time = 1613904992;
    int low_height = 2053000;
    int high_height = 2177791;
    int num_blocks = 2370;

    CDataStream data(SER_DISK, PROTOCOL_VERSION);

    data << mainnet_beacon_bin;

    BeaconRegistryTest beacon_registry_test(data,
                                            high_height_time,
                                            low_height,
                                            high_height,
                                            num_blocks);

    beacon_registry_test.RunBasicChecks();

    beacon_registry_test.BeaconDatabaseComparisonChecks_m_historical();

    beacon_registry_test.BeaconDatabaseComparisonChecks_m_beacons();

    beacon_registry_test.BeaconDatabaseComparisonChecks_m_pending();
}

BOOST_AUTO_TEST_CASE(beacon_registry_GetBeaconChainletRoot_test)
{
    LogInstance().EnableCategory(BCLog::LogFlags::BEACON);
    LogInstance().EnableCategory(BCLog::LogFlags::ACCRUAL);

    FastRandomContext rng(uint256 {0});

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();

    // Make sure the registry is reset.
    registry.Reset();

    // This is a trivial initial pending beacon, activation, and two renewals. The typical type of beacon chainlet.

    // Pending beacon
    CTransaction tx1 {};
    tx1.nTime = int64_t {1};
    uint256 tx1_hash = tx1.GetHash();

    CBlockIndex pindex1 {};
    pindex1.nVersion = 13;
    pindex1.nHeight = 1;
    pindex1.nTime = tx1.nTime;

    GRC::Beacon beacon1 {TestKey::Public(), tx1.nTime, tx1_hash};
    beacon1.m_cpid = TestKey::Cpid();
    beacon1.m_status = GRC::Beacon::Status {GRC::BeaconStatusForStorage::PENDING};
    GRC::BeaconPayload beacon_payload1 {2, TestKey::Cpid(), beacon1};
    beacon_payload1.m_signature = TestKey::Signature(beacon_payload1);

    GRC::Contract contract1 = GRC::MakeContract<GRC::BeaconPayload>(3, GRC::ContractAction::ADD, beacon_payload1);
    GRC::ContractContext ctx1 {contract1, tx1, &pindex1};

    BOOST_CHECK(ctx1.m_contract.CopyPayloadAs<GRC::BeaconPayload>().m_cpid == TestKey::Cpid());
    BOOST_CHECK(ctx1.m_contract.CopyPayloadAs<GRC::BeaconPayload>().m_beacon.m_status == GRC::BeaconStatusForStorage::PENDING);
    BOOST_CHECK(ctx1.m_contract.CopyPayloadAs<GRC::BeaconPayload>().m_beacon.m_hash == tx1_hash);
    BOOST_CHECK(ctx1.m_tx.GetHash() == tx1_hash);

    registry.Add(ctx1);

    BOOST_CHECK(registry.GetBeaconDB().size() == 1);

    std::vector<GRC::Beacon_ptr> pending_beacons = registry.FindPending(TestKey::Cpid());

    BOOST_CHECK(pending_beacons.size() == 1);

    if (pending_beacons.size() == 1) {
        BOOST_CHECK(pending_beacons[0]->m_cpid == TestKey::Cpid());
        BOOST_CHECK(pending_beacons[0]->m_hash == tx1_hash);
        BOOST_CHECK(pending_beacons[0]->m_previous_hash == uint256 {});
        BOOST_CHECK(pending_beacons[0]->m_public_key == TestKey::Public());
        BOOST_CHECK(pending_beacons[0]->m_status == GRC::BeaconStatusForStorage::PENDING);
        BOOST_CHECK(pending_beacons[0]->m_timestamp == tx1.nTime);
    }

    // Activation
    CBlockIndex pindex2 {};
    pindex2.nVersion = 13;
    pindex2.nHeight = 2;
    pindex2.nTime = int64_t {2};
    uint256 block2_phash = rng.rand256();
    pindex2.phashBlock = &block2_phash;

    std::vector<uint160> beacon_ids {TestKey::Public().GetID()};

    registry.ActivatePending(beacon_ids, pindex2.nTime, *pindex2.phashBlock, pindex2.nHeight);

    uint256 activated_beacon_hash = Hash(block2_phash, pending_beacons[0]->m_hash);

    BOOST_CHECK(registry.GetBeaconDB().size() == 2);

    GRC::Beacon_ptr chainlet_head = registry.Try(TestKey::Cpid());

    BOOST_CHECK(chainlet_head != nullptr);

    if (chainlet_head != nullptr) {
        BOOST_CHECK(chainlet_head->m_hash == activated_beacon_hash);
        BOOST_CHECK(chainlet_head->m_status == GRC::BeaconStatusForStorage::ACTIVE);
        // Note that the activated beacon's timestamp is actually the same as the timestamp of the PENDING beacon. (Here
        // t = 1;
        BOOST_CHECK(chainlet_head->m_timestamp == 1);

        std::vector<std::pair<uint256, int64_t>> beacon_chain_out {};

        std::shared_ptr<std::vector<std::pair<uint256, int64_t>>> beacon_chain_out_ptr
            = std::make_shared<std::vector<std::pair<uint256, int64_t>>>(beacon_chain_out);

        GRC::Beacon_ptr chainlet_root = registry.GetBeaconChainletRoot(chainlet_head, beacon_chain_out_ptr);

        // There is only one entry in the chainlet.. so the head and root are the same.
        BOOST_CHECK(chainlet_root->m_hash == chainlet_head->m_hash);
        BOOST_CHECK_EQUAL(beacon_chain_out_ptr->size(), 1);
    }

    // Renewal
    CTransaction tx3 {};
    tx3.nTime = int64_t {3};
    uint256 tx3_hash = tx3.GetHash();
    CBlockIndex index3 {};
    index3.nVersion = 13;
    index3.nHeight = 3;
    index3.nTime = tx3.nTime;

    GRC::Beacon beacon3 {TestKey::Public(), tx3.nTime, tx3_hash};
    beacon3.m_cpid = TestKey::Cpid();
    GRC::BeaconPayload beacon_payload3 {2, TestKey::Cpid(), beacon3};
    beacon_payload3.m_signature = TestKey::Signature(beacon_payload3);

    GRC::Contract contract3 = GRC::MakeContract<GRC::BeaconPayload>(3, GRC::ContractAction::ADD, beacon_payload3);
    GRC::ContractContext ctx3 {contract3, tx3, &index3};

    registry.Add(ctx3);

    chainlet_head = registry.Try(TestKey::Cpid());

    BOOST_CHECK(chainlet_head != nullptr);

    if (chainlet_head != nullptr) {
        BOOST_CHECK(chainlet_head->m_hash == tx3_hash);
        BOOST_CHECK(chainlet_head->m_status == GRC::BeaconStatusForStorage::RENEWAL);

        std::vector<std::pair<uint256, int64_t>> beacon_chain_out {};

        std::shared_ptr<std::vector<std::pair<uint256, int64_t>>> beacon_chain_out_ptr
            = std::make_shared<std::vector<std::pair<uint256, int64_t>>>(beacon_chain_out);

        GRC::Beacon_ptr chainlet_root = registry.GetBeaconChainletRoot(chainlet_head, beacon_chain_out_ptr);

        BOOST_CHECK(chainlet_root->m_hash == activated_beacon_hash);
        BOOST_CHECK_EQUAL(beacon_chain_out_ptr->size(), 2);
    }

    // Second renewal
    CTransaction tx4 {};
    tx4.nTime = int64_t {4};
    uint256 tx4_hash = tx4.GetHash();
    CBlockIndex index4 = {};
    index4.nVersion = 13;
    index4.nHeight = 2;
    index4.nTime = tx4.nTime;

    GRC::Beacon beacon4 {TestKey::Public(), tx4.nTime, tx4_hash};
    beacon4.m_cpid = TestKey::Cpid();
    GRC::BeaconPayload beacon_payload4 {2, TestKey::Cpid(), beacon4};
    beacon_payload4.m_signature = TestKey::Signature(beacon_payload4);

    GRC::Contract contract4 = GRC::MakeContract<GRC::BeaconPayload>(3, GRC::ContractAction::ADD, beacon_payload4);
    GRC::ContractContext ctx4 {contract4, tx4, &index4};
    registry.Add(ctx4);

    chainlet_head = registry.Try(TestKey::Cpid());

    BOOST_CHECK(chainlet_head != nullptr);

    if (chainlet_head != nullptr) {

        std::vector<std::pair<uint256, int64_t>> beacon_chain_out {};

        std::shared_ptr<std::vector<std::pair<uint256, int64_t>>> beacon_chain_out_ptr
         = std::make_shared<std::vector<std::pair<uint256, int64_t>>>(beacon_chain_out);
        BOOST_CHECK(chainlet_head->m_hash == tx4_hash);
        BOOST_CHECK(chainlet_head->m_status == GRC::BeaconStatusForStorage::RENEWAL);

        GRC::Beacon_ptr chainlet_root = registry.GetBeaconChainletRoot(chainlet_head, beacon_chain_out_ptr);

        BOOST_CHECK(chainlet_root->m_hash == activated_beacon_hash);
        BOOST_CHECK_EQUAL(beacon_chain_out_ptr->size(), 3);
    }

    // Let's corrupt the activation beacon to have a previous beacon hash that refers circularly back to the chain head...
    bool original_activated_beacon_found = true;
    bool circular_corruption_detected = false;

    if (GRC::Beacon_ptr first_active = registry.FindHistorical(activated_beacon_hash)) {
        // The original activated beacon m_previous_hash should be the pending beacon hash (beacon1).
        BOOST_CHECK(first_active->m_previous_hash == beacon1.m_hash);
        BOOST_CHECK(first_active->m_status == GRC::BeaconStatusForStorage::ACTIVE);

        std::vector<std::pair<uint256, int64_t>> beacon_chain_out {};

        std::shared_ptr<std::vector<std::pair<uint256, int64_t>>> beacon_chain_out_ptr
            = std::make_shared<std::vector<std::pair<uint256, int64_t>>>(beacon_chain_out);

        // This creates a circular chainlet.
        first_active->m_previous_hash = chainlet_head->m_hash;

        beacon_chain_out_ptr->clear();

        try {
            GRC::Beacon_ptr chainlet_root = registry.GetBeaconChainletRoot(chainlet_head, beacon_chain_out_ptr);
        } catch (std::runtime_error& e) {
            circular_corruption_detected = true;
        }
    } else {
        original_activated_beacon_found = false;
    }

    BOOST_CHECK_EQUAL(original_activated_beacon_found, true);

    if (original_activated_beacon_found) {
        BOOST_CHECK_EQUAL(circular_corruption_detected, true);
    }
}

BOOST_AUTO_TEST_CASE(beacon_registry_GetBeaconChainletRoot_test_2)
{
    // For right now we will just cut and paste from above, given that the circularity detection causes the registry
    // to get reset.

    LogInstance().EnableCategory(BCLog::LogFlags::BEACON);
    LogInstance().EnableCategory(BCLog::LogFlags::ACCRUAL);

    FastRandomContext rng(uint256 {0});

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();

    // Make sure the registry is reset.
    registry.Reset();

    // This is a trivial initial pending beacon, activation, and two renewals. The typical type of beacon chainlet.

    // Pending beacon
    CTransaction tx1 {};
    tx1.nTime = int64_t {1};
    uint256 tx1_hash = tx1.GetHash();

    CBlockIndex pindex1 {};
    pindex1.nVersion = 13;
    pindex1.nHeight = 1;
    pindex1.nTime = tx1.nTime;

    GRC::Beacon beacon1 {TestKey::Public(), tx1.nTime, tx1_hash};
    beacon1.m_cpid = TestKey::Cpid();
    beacon1.m_status = GRC::Beacon::Status {GRC::BeaconStatusForStorage::PENDING};
    GRC::BeaconPayload beacon_payload1 {2, TestKey::Cpid(), beacon1};
    beacon_payload1.m_signature = TestKey::Signature(beacon_payload1);

    GRC::Contract contract1 = GRC::MakeContract<GRC::BeaconPayload>(3, GRC::ContractAction::ADD, beacon_payload1);
    GRC::ContractContext ctx1 {contract1, tx1, &pindex1};

    BOOST_CHECK(ctx1.m_contract.CopyPayloadAs<GRC::BeaconPayload>().m_cpid == TestKey::Cpid());
    BOOST_CHECK(ctx1.m_contract.CopyPayloadAs<GRC::BeaconPayload>().m_beacon.m_status == GRC::BeaconStatusForStorage::PENDING);
    BOOST_CHECK(ctx1.m_contract.CopyPayloadAs<GRC::BeaconPayload>().m_beacon.m_hash == tx1_hash);
    BOOST_CHECK(ctx1.m_tx.GetHash() == tx1_hash);

    registry.Add(ctx1);

    BOOST_CHECK(registry.GetBeaconDB().size() == 1);

    std::vector<GRC::Beacon_ptr> pending_beacons = registry.FindPending(TestKey::Cpid());

    BOOST_CHECK(pending_beacons.size() == 1);

    if (pending_beacons.size() == 1) {
        BOOST_CHECK(pending_beacons[0]->m_cpid == TestKey::Cpid());
        BOOST_CHECK(pending_beacons[0]->m_hash == tx1_hash);
        BOOST_CHECK(pending_beacons[0]->m_previous_hash == uint256 {});
        BOOST_CHECK(pending_beacons[0]->m_public_key == TestKey::Public());
        BOOST_CHECK(pending_beacons[0]->m_status == GRC::BeaconStatusForStorage::PENDING);
        BOOST_CHECK(pending_beacons[0]->m_timestamp == tx1.nTime);
    }

    // Activation
    CBlockIndex pindex2 {};
    pindex2.nVersion = 13;
    pindex2.nHeight = 2;
    pindex2.nTime = int64_t {2};
    uint256 block2_phash = rng.rand256();
    pindex2.phashBlock = &block2_phash;

    std::vector<uint160> beacon_ids {TestKey::Public().GetID()};

    registry.ActivatePending(beacon_ids, pindex2.nTime, *pindex2.phashBlock, pindex2.nHeight);

    uint256 activated_beacon_hash = Hash(block2_phash, pending_beacons[0]->m_hash);

    BOOST_CHECK(registry.GetBeaconDB().size() == 2);

    GRC::Beacon_ptr chainlet_head = registry.Try(TestKey::Cpid());

    BOOST_CHECK(chainlet_head != nullptr);

    if (chainlet_head != nullptr) {
        BOOST_CHECK(chainlet_head->m_hash == activated_beacon_hash);
        BOOST_CHECK(chainlet_head->m_status == GRC::BeaconStatusForStorage::ACTIVE);
        // Note that the activated beacon's timestamp is actually the same as the timestamp of the PENDING beacon. (Here
        // t = 1;
        BOOST_CHECK(chainlet_head->m_timestamp == 1);

        std::vector<std::pair<uint256, int64_t>> beacon_chain_out {};

        std::shared_ptr<std::vector<std::pair<uint256, int64_t>>> beacon_chain_out_ptr
            = std::make_shared<std::vector<std::pair<uint256, int64_t>>>(beacon_chain_out);

        GRC::Beacon_ptr chainlet_root = registry.GetBeaconChainletRoot(chainlet_head, beacon_chain_out_ptr);

        // There is only one entry in the chainlet.. so the head and root are the same.
        BOOST_CHECK(chainlet_root->m_hash == chainlet_head->m_hash);
        BOOST_CHECK_EQUAL(beacon_chain_out_ptr->size(), 1);
    }

    // Renewal
    CTransaction tx3 {};
    tx3.nTime = int64_t {3};
    uint256 tx3_hash = tx3.GetHash();
    CBlockIndex index3 {};
    index3.nVersion = 13;
    index3.nHeight = 3;
    index3.nTime = tx3.nTime;

    GRC::Beacon beacon3 {TestKey::Public(), tx3.nTime, tx3_hash};
    beacon3.m_cpid = TestKey::Cpid();
    GRC::BeaconPayload beacon_payload3 {2, TestKey::Cpid(), beacon3};
    beacon_payload3.m_signature = TestKey::Signature(beacon_payload3);

    GRC::Contract contract3 = GRC::MakeContract<GRC::BeaconPayload>(3, GRC::ContractAction::ADD, beacon_payload3);
    GRC::ContractContext ctx3 {contract3, tx3, &index3};

    registry.Add(ctx3);

    chainlet_head = registry.Try(TestKey::Cpid());

    BOOST_CHECK(chainlet_head != nullptr);

    if (chainlet_head != nullptr) {
        BOOST_CHECK(chainlet_head->m_hash == tx3_hash);
        BOOST_CHECK(chainlet_head->m_status == GRC::BeaconStatusForStorage::RENEWAL);

        std::vector<std::pair<uint256, int64_t>> beacon_chain_out {};

        std::shared_ptr<std::vector<std::pair<uint256, int64_t>>> beacon_chain_out_ptr
            = std::make_shared<std::vector<std::pair<uint256, int64_t>>>(beacon_chain_out);

        GRC::Beacon_ptr chainlet_root = registry.GetBeaconChainletRoot(chainlet_head, beacon_chain_out_ptr);

        BOOST_CHECK(chainlet_root->m_hash == activated_beacon_hash);
        BOOST_CHECK_EQUAL(beacon_chain_out_ptr->size(), 2);
    }

    // Second renewal
    CTransaction tx4 {};
    tx4.nTime = int64_t {4};
    uint256 tx4_hash = tx4.GetHash();
    CBlockIndex index4 = {};
    index4.nVersion = 13;
    index4.nHeight = 2;
    index4.nTime = tx4.nTime;

    GRC::Beacon beacon4 {TestKey::Public(), tx4.nTime, tx4_hash};
    beacon4.m_cpid = TestKey::Cpid();
    GRC::BeaconPayload beacon_payload4 {2, TestKey::Cpid(), beacon4};
    beacon_payload4.m_signature = TestKey::Signature(beacon_payload4);

    GRC::Contract contract4 = GRC::MakeContract<GRC::BeaconPayload>(3, GRC::ContractAction::ADD, beacon_payload4);
    GRC::ContractContext ctx4 {contract4, tx4, &index4};
    registry.Add(ctx4);

    chainlet_head = registry.Try(TestKey::Cpid());

    BOOST_CHECK(chainlet_head != nullptr);

    if (chainlet_head != nullptr) {

        std::vector<std::pair<uint256, int64_t>> beacon_chain_out {};

        std::shared_ptr<std::vector<std::pair<uint256, int64_t>>> beacon_chain_out_ptr
            = std::make_shared<std::vector<std::pair<uint256, int64_t>>>(beacon_chain_out);
        BOOST_CHECK(chainlet_head->m_hash == tx4_hash);
        BOOST_CHECK(chainlet_head->m_status == GRC::BeaconStatusForStorage::RENEWAL);

        GRC::Beacon_ptr chainlet_root = registry.GetBeaconChainletRoot(chainlet_head, beacon_chain_out_ptr);

        BOOST_CHECK(chainlet_root->m_hash == activated_beacon_hash);
        BOOST_CHECK_EQUAL(beacon_chain_out_ptr->size(), 3);
    }

    // Let's corrupt the activation beacon to have a previous beacon hash that is the same as its hash...
    bool original_activated_beacon_found = true;
    bool circular_corruption_detected = false;

    if (GRC::Beacon_ptr first_active = registry.FindHistorical(activated_beacon_hash)) {
        // The original activated beacon m_previous_hash should be the pending beacon hash (beacon1).
        BOOST_CHECK(first_active->m_previous_hash == beacon1.m_hash);
        BOOST_CHECK(first_active->m_status == GRC::BeaconStatusForStorage::ACTIVE);

        std::vector<std::pair<uint256, int64_t>> beacon_chain_out {};

        std::shared_ptr<std::vector<std::pair<uint256, int64_t>>> beacon_chain_out_ptr
            = std::make_shared<std::vector<std::pair<uint256, int64_t>>>(beacon_chain_out);

        // This creates a immediately circular chainlet of one.
        first_active->m_previous_hash = first_active->m_hash;

        beacon_chain_out_ptr->clear();

        try {
            GRC::Beacon_ptr chainlet_root = registry.GetBeaconChainletRoot(chainlet_head, beacon_chain_out_ptr);
        } catch (std::runtime_error& e) {
            circular_corruption_detected = true;
        }
    } else {
        original_activated_beacon_found = false;
    }

    BOOST_CHECK_EQUAL(original_activated_beacon_found, true);

    if (original_activated_beacon_found) {
        BOOST_CHECK_EQUAL(circular_corruption_detected, true);
    }
}

BOOST_AUTO_TEST_SUITE_END()
