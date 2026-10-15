// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <base58.h>
#include "dbwrapper.h"
#include "gridcoin/beacon.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/contract/registry.h"
#include "gridcoin/claim.h"
#include "gridcoin/superblock.h"
#include <key_io.h>
#include "rpc/blockchain.h"
#include "test/data/testnet_beacon.bin.h"
#include "test/data/mainnet_beacon.bin.h"
#include <util/string.h>

#include <boost/test/unit_test.hpp>

#include "test/state_guard.h"
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
    static CTxDestination Address()
    {
        return CTxDestination(KeyId());
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
                                  << ", address = " << EncodeDestination(beacon->GetAddress())
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
                                  << ", address = " << EncodeDestination(beacon->GetAddress())
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
                                  << ", address = " << EncodeDestination(activated_beacon->GetAddress())
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
                                  << ", address = " << EncodeDestination(iter->GetAddress())
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
                      << ", address = " << EncodeDestination(beacon_ptr->GetAddress())
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
                      << ", address = " << EncodeDestination(beacon_ptr->GetAddress())
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
                          << ", address = " << EncodeDestination(left.second->GetAddress())
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

                std::cout << "init_beacon address = " << EncodeDestination(left_beacon_ptr->GetAddress())
                          << ", reinit_beacon address = " << EncodeDestination(right_beacon_iter->second->GetAddress()) << std::endl;

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
                          << ", address = " << EncodeDestination(left.second->GetAddress())
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

                std::cout << "reinit_beacon address = " << EncodeDestination(left_beacon_ptr->GetAddress())
                          << ", init_beacon address = " << EncodeDestination(right_beacon_iter->second->GetAddress()) << std::endl;

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
                          << ", address = " << EncodeDestination(beacon.GetAddress())
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
                          << ", address = " << EncodeDestination(beacon.GetAddress())
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
                          << ", address = " << EncodeDestination(left.second.GetAddress())
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
                          << ", address = " << EncodeDestination(left.second.GetAddress())
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
                          << ", address = " << EncodeDestination(left_beacon.GetAddress())
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
                          << ", address = " << EncodeDestination(left.second.GetAddress())
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
        + EncodeDestination(TestKey::Address())
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

// Suite-level guards: BeaconRegistryTest inserts every block of the fixture
// file into mapBlockIndex and advances hashBestChain without ever erasing
// or restoring them, and resets the beacon registry on entry only.
BOOST_AUTO_TEST_SUITE(BeaconPayload,
                      *boost::unit_test::fixture<grc_test::StateGuard>()
                      * boost::unit_test::fixture<grc_test::RegistryResetFor<GRC::ContractType::BEACON>>())

// BeaconRegistry methods (Reset, Add, ActivatePending, Delete, Try,
// FindPending, FindHistorical, Deactivate) are EXCLUSIVE_LOCKS_REQUIRED(cs_main).
// These tests directly mutate the registry without acquiring cs_main (the
// test fixtures are single-threaded). Suppress thread-safety warnings for
// the test suite rather than wrapping every call.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"
#endif

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

    // The two-arg constructor produces a v2 payload (not CURRENT_VERSION which is now 3).
    BOOST_CHECK_EQUAL(payload.m_version, (uint32_t)2);
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

    // BeaconPayload(cpid, beacon) produces a v2 payload.
    const uint32_t expected_version = 2;

    const CDataStream expected = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << expected_version
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

    // Test v2 payload deserialization (v2 does not include ownership proof).
    const uint32_t v2_version = 2;

    CDataStream stream_add = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << v2_version
        << cpid
        << beacon
        << signature;

    CDataStream stream_remove = stream_add;

    GRC::BeaconPayload payload;
    payload.Unserialize(stream_add, GRC::ContractAction::ADD);

    BOOST_CHECK_EQUAL(payload.m_version, v2_version);
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

    BOOST_CHECK_EQUAL(payload.m_version, v2_version);
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

BOOST_AUTO_TEST_CASE(ownership_proof_serialization_roundtrip)
{
    GRC::OwnershipProof proof;
    proof.m_master_url = "https://minecraftathome.com/minecrafthome/";
    proof.m_account_id = 19100117;
    proof.m_rsa_signature = {0x01, 0x02, 0x03, 0x04, 0x05};

    BOOST_CHECK(!proof.Empty());

    // Serialize
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << proof;

    // Deserialize
    GRC::OwnershipProof deserialized;
    stream >> deserialized;

    BOOST_CHECK_EQUAL(deserialized.m_master_url, proof.m_master_url);
    BOOST_CHECK_EQUAL(deserialized.m_account_id, proof.m_account_id);
    BOOST_CHECK_EQUAL_COLLECTIONS(
        deserialized.m_rsa_signature.begin(),
        deserialized.m_rsa_signature.end(),
        proof.m_rsa_signature.begin(),
        proof.m_rsa_signature.end());
}

BOOST_AUTO_TEST_CASE(ownership_proof_empty_check)
{
    GRC::OwnershipProof empty_proof;
    BOOST_CHECK(empty_proof.Empty());

    // A proof with any field set is not empty (Empty() uses AND logic).
    GRC::OwnershipProof partial_proof;
    partial_proof.m_master_url = "https://example.com/";
    BOOST_CHECK(!partial_proof.Empty());

    GRC::OwnershipProof full_proof;
    full_proof.m_master_url = "https://example.com/";
    full_proof.m_account_id = 1;
    full_proof.m_rsa_signature = {0x01};
    BOOST_CHECK(!full_proof.Empty());
}

BOOST_AUTO_TEST_CASE(it_serializes_v3_payload_to_a_stream)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    const GRC::Beacon beacon(TestKey::Public());

    GRC::OwnershipProof proof;
    proof.m_master_url = "https://example.com/project/";
    proof.m_account_id = 42;
    proof.m_rsa_signature = {0xAA, 0xBB, 0xCC};

    GRC::BeaconPayload payload(cpid, beacon, std::move(proof));
    payload.m_signature = TestKey::Signature();

    BOOST_CHECK_EQUAL(payload.m_version, (uint32_t)3);
    BOOST_CHECK(!payload.m_ownership_proof.Empty());

    // Serialize
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    payload.Serialize(stream, GRC::ContractAction::ADD);

    // Deserialize
    GRC::BeaconPayload deserialized;
    deserialized.Unserialize(stream, GRC::ContractAction::ADD);

    BOOST_CHECK_EQUAL(deserialized.m_version, (uint32_t)3);
    BOOST_CHECK(deserialized.m_cpid == cpid);
    BOOST_CHECK(deserialized.m_beacon.m_public_key == TestKey::Public());
    BOOST_CHECK_EQUAL(deserialized.m_ownership_proof.m_master_url, "https://example.com/project/");
    BOOST_CHECK_EQUAL(deserialized.m_ownership_proof.m_account_id, (uint32_t)42);
    BOOST_CHECK_EQUAL(deserialized.m_ownership_proof.m_rsa_signature.size(), (size_t)3);
}

BOOST_AUTO_TEST_CASE(it_checks_v3_payload_well_formed_for_add)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");

    GRC::OwnershipProof proof;
    proof.m_master_url = "https://example.com/project/";
    proof.m_account_id = 42;
    // RSA signature must be between 64 and 1024 bytes for WellFormed()
    proof.m_rsa_signature.resize(256, 0xAA);

    GRC::BeaconPayload payload(cpid, GRC::Beacon(TestKey::Public()), GRC::OwnershipProof(proof));
    payload.m_signature = TestKey::Signature();

    // v3 ADD with valid proof should be well-formed.
    BOOST_CHECK(payload.WellFormed(GRC::ContractAction::ADD) == true);

    // v3 REMOVE should also work (proof is not required for removal).
    BOOST_CHECK(payload.WellFormed(GRC::ContractAction::REMOVE) == true);
}

BOOST_AUTO_TEST_CASE(it_rejects_v3_add_with_empty_proof)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");

    // Create v3 payload with empty proof — should fail WellFormed for ADD.
    GRC::OwnershipProof empty_proof;
    GRC::BeaconPayload payload(cpid, GRC::Beacon(TestKey::Public()), std::move(empty_proof));
    payload.m_signature = TestKey::Signature();

    BOOST_CHECK(payload.WellFormed(GRC::ContractAction::ADD) == false);
}

BOOST_AUTO_TEST_CASE(it_rejects_v3_add_with_short_rsa_signature)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");

    GRC::OwnershipProof proof;
    proof.m_master_url = "https://example.com/project/";
    proof.m_account_id = 42;
    proof.m_rsa_signature.resize(10, 0xAA); // Too short (< 64 bytes)

    GRC::BeaconPayload payload(cpid, GRC::Beacon(TestKey::Public()), std::move(proof));
    payload.m_signature = TestKey::Signature();

    BOOST_CHECK(payload.WellFormed(GRC::ContractAction::ADD) == false);
}

BOOST_AUTO_TEST_CASE(it_includes_ownership_proof_in_v3_hash)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    const GRC::Beacon beacon(TestKey::Public());

    GRC::OwnershipProof proof1;
    proof1.m_master_url = "https://example.com/project/";
    proof1.m_account_id = 42;
    proof1.m_rsa_signature.resize(256, 0xAA);

    GRC::OwnershipProof proof2;
    proof2.m_master_url = "https://example.com/project/";
    proof2.m_account_id = 43; // Different account ID
    proof2.m_rsa_signature.resize(256, 0xAA);

    GRC::BeaconPayload payload1(cpid, beacon, std::move(proof1));
    GRC::BeaconPayload payload2(cpid, beacon, std::move(proof2));

    // Hash the payloads using SER_GETHASH (which includes the ownership proof).
    CHashWriter hasher1(SER_GETHASH, PROTOCOL_VERSION);
    payload1.Serialize(hasher1, GRC::ContractAction::UNKNOWN);

    CHashWriter hasher2(SER_GETHASH, PROTOCOL_VERSION);
    payload2.Serialize(hasher2, GRC::ContractAction::UNKNOWN);

    // Different ownership proofs should produce different hashes.
    BOOST_CHECK(hasher1.GetHash() != hasher2.GetHash());
}

BOOST_AUTO_TEST_CASE(it_gets_the_ownership_proof_from_a_v3_advertisement)
{
    GRC::OwnershipProof proof;
    proof.m_master_url = "https://example.com/project/";
    proof.m_account_id = 42;
    proof.m_rsa_signature.resize(256, 0xAA);

    GRC::BeaconPayload payload(TestKey::Cpid(), GRC::Beacon(TestKey::Public()), GRC::OwnershipProof(proof));
    payload.m_signature = TestKey::Signature(payload);

    CMutableTransaction advertisement;
    advertisement.vContracts.push_back(GRC::MakeContract<GRC::BeaconPayload>(3, GRC::ContractAction::ADD, payload));
    const CTransaction tx(advertisement);

    const std::optional<GRC::OwnershipProof> found = GRC::GetOwnershipProofFromTx(tx, TestKey::KeyId());

    BOOST_REQUIRE(found.has_value());
    BOOST_CHECK_EQUAL(found->m_master_url, proof.m_master_url);
    BOOST_CHECK_EQUAL(found->m_account_id, proof.m_account_id);
    BOOST_CHECK(found->m_rsa_signature == proof.m_rsa_signature);

    // The transaction advertises no beacon under any other key.
    CKey other_key;
    other_key.MakeNewKey(true);

    BOOST_CHECK(!GRC::GetOwnershipProofFromTx(tx, other_key.GetPubKey().GetID()).has_value());
}

BOOST_AUTO_TEST_CASE(it_gets_no_ownership_proof_without_a_v3_advertisement)
{
    // A v2 advertisement carries no proof.
    GRC::BeaconPayload v2_payload {2, TestKey::Cpid(), GRC::Beacon(TestKey::Public())};
    v2_payload.m_signature = TestKey::Signature(v2_payload);

    CMutableTransaction v2_advertisement;
    v2_advertisement.vContracts.push_back(
        GRC::MakeContract<GRC::BeaconPayload>(3, GRC::ContractAction::ADD, v2_payload));

    BOOST_CHECK(!GRC::GetOwnershipProofFromTx(CTransaction(v2_advertisement), TestKey::KeyId()).has_value());

    // A v3 payload in a removal is not an advertisement.
    GRC::OwnershipProof proof;
    proof.m_master_url = "https://example.com/project/";
    proof.m_account_id = 42;
    proof.m_rsa_signature.resize(256, 0xAA);

    GRC::BeaconPayload v3_payload(TestKey::Cpid(), GRC::Beacon(TestKey::Public()), std::move(proof));
    v3_payload.m_signature = TestKey::Signature(v3_payload);

    CMutableTransaction removal;
    removal.vContracts.push_back(GRC::MakeContract<GRC::BeaconPayload>(3, GRC::ContractAction::REMOVE, v3_payload));

    BOOST_CHECK(!GRC::GetOwnershipProofFromTx(CTransaction(removal), TestKey::KeyId()).has_value());

    // Nor is a transaction without contracts.
    const CMutableTransaction no_contracts;

    BOOST_CHECK(!GRC::GetOwnershipProofFromTx(CTransaction(no_contracts), TestKey::KeyId()).has_value());
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
    CMutableTransaction tx1 {};
    tx1.nTime = int64_t {1};
    CTransaction ctx_tx1(tx1);
    uint256 tx1_hash = ctx_tx1.GetHash();

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
    GRC::ContractContext ctx1 {contract1, ctx_tx1, &pindex1};

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
        // sign-compare checked: size() is size_t; 1U is a non-negative literal, so both sides are unsigned.
        BOOST_CHECK_EQUAL(beacon_chain_out_ptr->size(), 1U);
    }

    // Renewal
    CMutableTransaction tx3 {};
    tx3.nTime = int64_t {3};
    CTransaction ctx_tx3(tx3);
    uint256 tx3_hash = ctx_tx3.GetHash();
    CBlockIndex index3 {};
    index3.nVersion = 13;
    index3.nHeight = 3;
    index3.nTime = tx3.nTime;

    GRC::Beacon beacon3 {TestKey::Public(), tx3.nTime, tx3_hash};
    beacon3.m_cpid = TestKey::Cpid();
    GRC::BeaconPayload beacon_payload3 {2, TestKey::Cpid(), beacon3};
    beacon_payload3.m_signature = TestKey::Signature(beacon_payload3);

    GRC::Contract contract3 = GRC::MakeContract<GRC::BeaconPayload>(3, GRC::ContractAction::ADD, beacon_payload3);
    GRC::ContractContext ctx3 {contract3, ctx_tx3, &index3};

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
        // sign-compare checked: size() is size_t; 2U is a non-negative literal, so both sides are unsigned.
        BOOST_CHECK_EQUAL(beacon_chain_out_ptr->size(), 2U);
    }

    // Second renewal
    CMutableTransaction tx4 {};
    tx4.nTime = int64_t {4};
    CTransaction ctx_tx4(tx4);
    uint256 tx4_hash = ctx_tx4.GetHash();
    CBlockIndex index4 = {};
    index4.nVersion = 13;
    index4.nHeight = 2;
    index4.nTime = tx4.nTime;

    GRC::Beacon beacon4 {TestKey::Public(), tx4.nTime, tx4_hash};
    beacon4.m_cpid = TestKey::Cpid();
    GRC::BeaconPayload beacon_payload4 {2, TestKey::Cpid(), beacon4};
    beacon_payload4.m_signature = TestKey::Signature(beacon_payload4);

    GRC::Contract contract4 = GRC::MakeContract<GRC::BeaconPayload>(3, GRC::ContractAction::ADD, beacon_payload4);
    GRC::ContractContext ctx4 {contract4, ctx_tx4, &index4};
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
        // sign-compare checked: size() is size_t; 3U is a non-negative literal, so both sides are unsigned.
        BOOST_CHECK_EQUAL(beacon_chain_out_ptr->size(), 3U);
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
    CMutableTransaction tx1 {};
    tx1.nTime = int64_t {1};
    CTransaction ctx_tx1(tx1);
    uint256 tx1_hash = ctx_tx1.GetHash();

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
    GRC::ContractContext ctx1 {contract1, ctx_tx1, &pindex1};

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
        // sign-compare checked: size() is size_t; 1U is a non-negative literal, so both sides are unsigned.
        BOOST_CHECK_EQUAL(beacon_chain_out_ptr->size(), 1U);
    }

    // Renewal
    CMutableTransaction tx3 {};
    tx3.nTime = int64_t {3};
    CTransaction ctx_tx3(tx3);
    uint256 tx3_hash = ctx_tx3.GetHash();
    CBlockIndex index3 {};
    index3.nVersion = 13;
    index3.nHeight = 3;
    index3.nTime = tx3.nTime;

    GRC::Beacon beacon3 {TestKey::Public(), tx3.nTime, tx3_hash};
    beacon3.m_cpid = TestKey::Cpid();
    GRC::BeaconPayload beacon_payload3 {2, TestKey::Cpid(), beacon3};
    beacon_payload3.m_signature = TestKey::Signature(beacon_payload3);

    GRC::Contract contract3 = GRC::MakeContract<GRC::BeaconPayload>(3, GRC::ContractAction::ADD, beacon_payload3);
    GRC::ContractContext ctx3 {contract3, ctx_tx3, &index3};

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
        // sign-compare checked: size() is size_t; 2U is a non-negative literal, so both sides are unsigned.
        BOOST_CHECK_EQUAL(beacon_chain_out_ptr->size(), 2U);
    }

    // Second renewal
    CMutableTransaction tx4 {};
    tx4.nTime = int64_t {4};
    CTransaction ctx_tx4(tx4);
    uint256 tx4_hash = ctx_tx4.GetHash();
    CBlockIndex index4 = {};
    index4.nVersion = 13;
    index4.nHeight = 2;
    index4.nTime = tx4.nTime;

    GRC::Beacon beacon4 {TestKey::Public(), tx4.nTime, tx4_hash};
    beacon4.m_cpid = TestKey::Cpid();
    GRC::BeaconPayload beacon_payload4 {2, TestKey::Cpid(), beacon4};
    beacon_payload4.m_signature = TestKey::Signature(beacon_payload4);

    GRC::Contract contract4 = GRC::MakeContract<GRC::BeaconPayload>(3, GRC::ContractAction::ADD, beacon_payload4);
    GRC::ContractContext ctx4 {contract4, ctx_tx4, &index4};
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
        // sign-compare checked: size() is size_t; 3U is a non-negative literal, so both sides are unsigned.
        BOOST_CHECK_EQUAL(beacon_chain_out_ptr->size(), 3U);
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

BOOST_AUTO_TEST_CASE(beacon_registry_readvertisement_without_a_proof_drops_the_old_proof)
{
    LOCK(cs_main);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();

    registry.Reset();

    // A v3 advertisement leaves a pending beacon with its proof.
    CMutableTransaction tx1 {};
    tx1.nTime = int64_t {1};
    const CTransaction ctx_tx1(tx1);

    CBlockIndex index1 {};
    index1.nVersion = 13;
    index1.nHeight = 1;
    index1.nTime = tx1.nTime;

    GRC::OwnershipProof proof;
    proof.m_master_url = "https://example.com/project/";
    proof.m_account_id = 42;
    proof.m_rsa_signature.resize(256, 0xAA);

    GRC::BeaconPayload payload1(TestKey::Cpid(), GRC::Beacon(TestKey::Public()), std::move(proof));
    payload1.m_signature = TestKey::Signature(payload1);

    GRC::Contract contract1 = GRC::MakeContract<GRC::BeaconPayload>(3, GRC::ContractAction::ADD, payload1);
    registry.Add({contract1, ctx_tx1, &index1});

    BOOST_CHECK_EQUAL(registry.PendingOwnershipProofs().count(TestKey::KeyId()), 1U);

    // A v2 advertisement of the same key replaces the pending beacon, and takes the proof with it.
    CMutableTransaction tx2 {};
    tx2.nTime = int64_t {2};
    const CTransaction ctx_tx2(tx2);

    CBlockIndex index2 {};
    index2.nVersion = 13;
    index2.nHeight = 2;
    index2.nTime = tx2.nTime;

    GRC::BeaconPayload payload2 {2, TestKey::Cpid(), GRC::Beacon(TestKey::Public())};
    payload2.m_signature = TestKey::Signature(payload2);

    GRC::Contract contract2 = GRC::MakeContract<GRC::BeaconPayload>(3, GRC::ContractAction::ADD, payload2);
    registry.Add({contract2, ctx_tx2, &index2});

    BOOST_CHECK_EQUAL(registry.PendingBeacons().count(TestKey::KeyId()), 1U);
    BOOST_CHECK_EQUAL(registry.PendingOwnershipProofs().count(TestKey::KeyId()), 0U);

    registry.Reset();
}

namespace {
//!
//! \brief Apply a v2 beacon advertisement to the registry, as a block at the given height would.
//!
//! \return The hash of the advertisement transaction, which is also the key of the pending beacon's record.
//!
uint256 AddPendingBeacon(GRC::BeaconRegistry& registry, const GRC::Cpid& cpid, const CPubKey& key,
                         const int64_t time, const int height)
{
    CMutableTransaction mtx;
    mtx.nTime = time;
    mtx.nLockTime = static_cast<uint32_t>(height);
    const CTransaction tx(mtx);

    CBlockIndex index;
    index.nVersion = 13;
    index.nHeight = height;
    index.nTime = time;

    GRC::BeaconPayload payload {2, cpid, GRC::Beacon(key)};
    const GRC::Contract contract = GRC::MakeContract<GRC::BeaconPayload>(3, GRC::ContractAction::ADD, payload);

    registry.Add({contract, tx, &index});

    return tx.GetHash();
}

//!
//! \brief Reload the registry from the beacon db, as a restarted node does.
//!
void ReloadRegistry(GRC::BeaconRegistry& registry)
{
    registry.ResetInMemoryOnly();
    registry.Initialize();
}

//!
//! \brief Read the beacon entry db version stored in LevelDB, or zero if it cannot be read (none is stored, or the
//! read fails), as the registry's own version check treats it.
//!
uint32_t StoredBeaconDBVersion()
{
    CTxDB txdb("r");

    std::pair<std::string, std::string> key = std::make_pair(std::string("beacon_db"), std::string("version"));
    uint32_t version = 0;

    if (!txdb.ReadGenericSerializable(key, version)) return 0;

    return version;
}

//!
//! \brief A block whose first two slots (coinbase and coinstake) carry no registry contracts.
//!
CBlock ContractBlock(const std::vector<CTransaction>& txs)
{
    CBlock block;
    block.vtx.resize(2);
    block.vtx.insert(block.vtx.end(), txs.begin(), txs.end());

    return block;
}

CBlockIndex ContractIndex(const int height, const int64_t time)
{
    CBlockIndex index;
    index.nVersion = 13;
    index.nHeight = height;
    index.nTime = time;

    return index;
}

//!
//! \brief Restart as a production node does: reload the registry from the beacon db, then run the startup replay
//! over the newest beacon block, whose contracts are at the db height and so are applied again.
//!
void ReloadAndReplay(GRC::BeaconRegistry& registry, const CBlock& block, const CBlockIndex& index)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    ReloadRegistry(registry);

    GRC::RegistryBookmarks db_heights;
    bool found = false;
    GRC::ReplayBlock(block, &index, db_heights, true, found);
}

//!
//! \brief A transaction carrying one v2 beacon contract, for a test block.
//!
CTransaction BeaconContractTx(const GRC::ContractAction action, const GRC::Cpid& cpid, const CKey& key,
                              const int64_t time)
{
    CMutableTransaction mtx;
    mtx.nTime = time;

    GRC::BeaconPayload payload {2, cpid, GRC::Beacon(key.GetPubKey())};
    mtx.vContracts.push_back(GRC::MakeContract<GRC::BeaconPayload>(3, action, payload));

    return CTransaction(mtx);
}

//!
//! \brief Revert the advertisement AddPendingBeacon applied with the same arguments, as a disconnect of its block
//! would.
//!
void RevertAdvertisement(GRC::BeaconRegistry& registry, const GRC::Cpid& cpid, const CPubKey& key,
                         const int64_t time, const int height)
{
    CMutableTransaction mtx;
    mtx.nTime = time;
    mtx.nLockTime = static_cast<uint32_t>(height);
    const CTransaction tx(mtx);

    CBlockIndex index;
    index.nVersion = 13;
    index.nHeight = height;
    index.nTime = time;

    GRC::BeaconPayload payload {2, cpid, GRC::Beacon(key)};
    const GRC::Contract contract = GRC::MakeContract<GRC::BeaconPayload>(3, GRC::ContractAction::ADD, payload);

    registry.Revert({contract, tx, &index});
}

//!
//! \brief Apply a v2 beacon deletion (revocation) to the registry, as a block at the given height would.
//!
uint256 DeleteBeacon(GRC::BeaconRegistry& registry, const GRC::Cpid& cpid, const CPubKey& key,
                     const int64_t time, const int height)
{
    CMutableTransaction mtx;
    mtx.nTime = time;
    mtx.nLockTime = static_cast<uint32_t>(height);
    const CTransaction tx(mtx);

    CBlockIndex index;
    index.nVersion = 13;
    index.nHeight = height;
    index.nTime = time;

    GRC::BeaconPayload payload {2, cpid, GRC::Beacon(key)};
    const GRC::Contract contract = GRC::MakeContract<GRC::BeaconPayload>(3, GRC::ContractAction::REMOVE, payload);

    registry.Delete({contract, tx, &index});

    return tx.GetHash();
}

//!
//! \brief Revert the deletion DeleteBeacon applied with the same arguments.
//!
void RevertDeletion(GRC::BeaconRegistry& registry, const GRC::Cpid& cpid, const CPubKey& key,
                    const int64_t time, const int height)
{
    CMutableTransaction mtx;
    mtx.nTime = time;
    mtx.nLockTime = static_cast<uint32_t>(height);
    const CTransaction tx(mtx);

    CBlockIndex index;
    index.nVersion = 13;
    index.nHeight = height;
    index.nTime = time;

    GRC::BeaconPayload payload {2, cpid, GRC::Beacon(key)};
    const GRC::Contract contract = GRC::MakeContract<GRC::BeaconPayload>(3, GRC::ContractAction::REMOVE, payload);

    registry.Revert({contract, tx, &index});
}

constexpr int64_t DAY = 60 * 60 * 24;
constexpr int64_t START_TIME = 1600000000;
} // anonymous namespace

BOOST_AUTO_TEST_CASE(beacon_registry_deactivate_skips_missing_pending_records)
{
    LOCK(cs_main);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();
    registry.Reset();

    CKey key_1;
    key_1.MakeNewKey(true);
    CKey key_2;
    key_2.MakeNewKey(true);

    const GRC::Cpid cpid_1 = GRC::Cpid::Parse("00010203040506070809101112131415");
    const GRC::Cpid cpid_2 = GRC::Cpid::Parse("15141312111009080706050403020100");

    const uint256 advertisement_1 = AddPendingBeacon(registry, cpid_1, key_1.GetPubKey(), START_TIME, 1);
    const uint256 advertisement_2 = AddPendingBeacon(registry, cpid_2, key_2.GetPubKey(), START_TIME, 2);

    // The superblock activates the first beacon and expires the second.
    const uint256 superblock = uint256S("0303030303030303030303030303030303030303030303030303030303030303");

    registry.ActivatePending({key_1.GetPubKey().GetID()},
                             START_TIME + GRC::PendingBeacon::RETENTION_AGE + DAY, superblock, 3);

    BOOST_REQUIRE(registry.Try(cpid_1) != nullptr);
    BOOST_REQUIRE(registry.PendingBeacons().empty());
    BOOST_REQUIRE_EQUAL(registry.ExpiredBeacons().size(), 1U);

    // Neither pending record is in the db any more.
    registry.GetBeaconDB().erase(advertisement_1);
    registry.GetBeaconDB().erase(advertisement_2);

    // Deactivate has nothing to restore either beacon to, and returns.
    registry.Deactivate(superblock, 3);

    BOOST_CHECK(registry.PendingBeacons().empty());
    BOOST_CHECK(registry.Try(cpid_1) != nullptr);

    // A restart computes the same state.
    ReloadRegistry(registry);

    BOOST_CHECK(registry.PendingBeacons().empty());
    BOOST_CHECK(registry.Try(cpid_1) != nullptr);

    registry.Reset();
}

// A node upgrading from a beacon entry db at version 3 discards its stored beacon history on the first start and
// rebuilds it by replaying every beacon contract, so that the history it keeps is the one a fresh sync produces.
BOOST_AUTO_TEST_CASE(beacon_registry_a_db_stored_at_version_3_is_rebuilt_on_the_next_start)
{
    LOCK(cs_main);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();
    registry.Reset();

    CKey key;
    key.MakeNewKey(true);
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");

    const uint256 advertisement = AddPendingBeacon(registry, cpid, key.GetPubKey(), START_TIME, 1);

    // At the current version a restart loads the stored history and replays from the stored height.
    registry.ResetInMemoryOnly();
    BOOST_REQUIRE_EQUAL(registry.Initialize(), 1);
    BOOST_REQUIRE_EQUAL(registry.PendingBeacons().size(), 1U);

    // Store version 3, as the nodes before version 4 did.
    {
        CTxDB txdb("rw");

        std::pair<std::string, std::string> version_key = std::make_pair(std::string("beacon_db"),
                                                                         std::string("version"));
        uint32_t version = 3;

        BOOST_REQUIRE(txdb.WriteGenericSerializable(version_key, version));
    }

    // The next start erases the stored history and stores the current version, and the replay starts from the
    // beginning.
    registry.ResetInMemoryOnly();
    BOOST_CHECK_EQUAL(registry.Initialize(), 0);
    BOOST_CHECK(registry.PendingBeacons().empty());
    BOOST_CHECK(registry.GetBeaconDB().find(advertisement) == registry.GetBeaconDB().end());
    BOOST_CHECK_EQUAL(StoredBeaconDBVersion(), 4U);

    registry.Reset();
}

BOOST_AUTO_TEST_CASE(beacon_registry_superblock_state_survives_a_restart)
{
    LOCK(cs_main);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();
    registry.Reset();

    const uint256 superblock = uint256S("0404040404040404040404040404040404040404040404040404040404040404");

    registry.ActivatePending({}, START_TIME, superblock, 3);
    BOOST_CHECK(registry.SuperblockStateCurrent(superblock));

    ReloadRegistry(registry);
    BOOST_CHECK(registry.SuperblockStateCurrent(superblock));

    // A disconnect consumes the state; a restart keeps it consumed.
    registry.Deactivate(superblock, 3);
    BOOST_CHECK(!registry.SuperblockStateCurrent(superblock));

    ReloadRegistry(registry);
    BOOST_CHECK(!registry.SuperblockStateCurrent(superblock));

    // The next connected superblock makes it current again.
    registry.ActivatePending({}, START_TIME + DAY, superblock, 3);
    BOOST_CHECK(registry.SuperblockStateCurrent(superblock));

    registry.Reset();
}

BOOST_AUTO_TEST_CASE(beacon_registry_missing_marker_counts_as_current)
{
    LOCK(cs_main);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();

    const uint256 superblock = uint256S("0404040404040404040404040404040404040404040404040404040404040404");

    // Reset erases the marker, as a db written before it existed has none.
    registry.Reset();
    BOOST_CHECK(registry.SuperblockStateCurrent(superblock));

    ReloadRegistry(registry);
    BOOST_CHECK(registry.SuperblockStateCurrent(superblock));

    registry.Reset();
}

namespace {
struct TwoExpiringSuperblocks
{
    CKey key_1;
    CKey key_2;
    GRC::Cpid cpid_1 = GRC::Cpid::Parse("00010203040506070809101112131415");
    GRC::Cpid cpid_2 = GRC::Cpid::Parse("15141312111009080706050403020100");
    uint256 advertisement_1;
    uint256 advertisement_2;
    uint256 superblock_1 = uint256S("0505050505050505050505050505050505050505050505050505050505050505");
    uint256 superblock_2 = uint256S("0606060606060606060606060606060606060606060606060606060606060606");

    //! Superblock 1 (height 2) expires the first pending beacon and activates nothing; superblock 2 (height 4)
    //! expires the second. The registry is then reloaded from the beacon db, and the expired set holds both.
    explicit TwoExpiringSuperblocks(GRC::BeaconRegistry& registry) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        key_1.MakeNewKey(true);
        key_2.MakeNewKey(true);

        advertisement_1 = AddPendingBeacon(registry, cpid_1, key_1.GetPubKey(), START_TIME, 1);
        registry.ActivatePending({}, START_TIME + GRC::PendingBeacon::RETENTION_AGE + DAY, superblock_1, 2);

        const int64_t time_2 = START_TIME + GRC::PendingBeacon::RETENTION_AGE + 2 * DAY;
        advertisement_2 = AddPendingBeacon(registry, cpid_2, key_2.GetPubKey(), time_2, 3);
        registry.ActivatePending({}, time_2 + GRC::PendingBeacon::RETENTION_AGE + DAY, superblock_2, 4);

        ReloadRegistry(registry);
    }
};
} // anonymous namespace

BOOST_AUTO_TEST_CASE(beacon_registry_deactivate_undoes_its_superblock_s_expirations_from_v15)
{
    LOCK(cs_main);
    grc_test::V15HeightGuard v15(1);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();
    registry.Reset();

    TwoExpiringSuperblocks chain(registry);
    BOOST_REQUIRE(registry.PendingBeacons().empty());
    BOOST_REQUIRE_EQUAL(registry.ExpiredBeacons().size(), 2U);

    registry.Deactivate(chain.superblock_2, 4);

    // Only superblock 2's expiration is undone, and its record goes with it.
    BOOST_CHECK_EQUAL(registry.PendingBeacons().size(), 1U);
    BOOST_CHECK_EQUAL(registry.PendingBeacons().count(chain.key_2.GetPubKey().GetID()), 1U);
    BOOST_CHECK(registry.GetBeaconDB().find(Hash(chain.superblock_2, chain.advertisement_2))
                == registry.GetBeaconDB().end());
    BOOST_CHECK(registry.GetBeaconDB().find(Hash(chain.superblock_1, chain.advertisement_1))
                != registry.GetBeaconDB().end());

    // A restart computes the same pending set.
    ReloadRegistry(registry);
    BOOST_CHECK_EQUAL(registry.PendingBeacons().size(), 1U);
    BOOST_CHECK_EQUAL(registry.PendingBeacons().count(chain.key_2.GetPubKey().GetID()), 1U);

    registry.Reset();
}

namespace {
struct ReplacementActivation
{
    CKey key_1;
    CKey key_2;
    GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    uint256 superblock_1 = uint256S("0707070707070707070707070707070707070707070707070707070707070707");
    uint256 superblock_2 = uint256S("0808080808080808080808080808080808080808080808080808080808080808");

    //! Superblock 1 (height 2) activates key 1; a forced re-advertisement of key 2 (height 3) is activated by
    //! superblock 2 (height 4), replacing key 1.
    explicit ReplacementActivation(GRC::BeaconRegistry& registry) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        key_1.MakeNewKey(true);
        key_2.MakeNewKey(true);

        AddPendingBeacon(registry, cpid, key_1.GetPubKey(), START_TIME, 1);
        registry.ActivatePending({key_1.GetPubKey().GetID()}, START_TIME + DAY, superblock_1, 2);

        AddPendingBeacon(registry, cpid, key_2.GetPubKey(), START_TIME + 2 * DAY, 3);
        registry.ActivatePending({key_2.GetPubKey().GetID()}, START_TIME + 3 * DAY, superblock_2, 4);
    }
};
} // anonymous namespace

BOOST_AUTO_TEST_CASE(beacon_registry_deactivate_restores_a_replaced_beacon_from_v15)
{
    LOCK(cs_main);
    grc_test::V15HeightGuard v15(1);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();

    for (const bool restart_first : {false, true}) {
        registry.Reset();

        ReplacementActivation chain(registry);
        BOOST_REQUIRE(registry.Try(chain.cpid));
        BOOST_REQUIRE(registry.Try(chain.cpid)->m_public_key == chain.key_2.GetPubKey());

        // A restart before the disconnect rebuilds the replaced-beacon map from the persisted superblock hash.
        if (restart_first) ReloadRegistry(registry);

        registry.Deactivate(chain.superblock_2, 4);

        BOOST_REQUIRE(registry.Try(chain.cpid));
        BOOST_CHECK(registry.Try(chain.cpid)->m_public_key == chain.key_1.GetPubKey());
        BOOST_CHECK_EQUAL(registry.PendingBeacons().count(chain.key_2.GetPubKey().GetID()), 1U);

        ReloadRegistry(registry);
        BOOST_REQUIRE(registry.Try(chain.cpid));
        BOOST_CHECK(registry.Try(chain.cpid)->m_public_key == chain.key_1.GetPubKey());
        BOOST_CHECK_EQUAL(registry.PendingBeacons().count(chain.key_2.GetPubKey().GetID()), 1U);
    }

    registry.Reset();
}

BOOST_AUTO_TEST_CASE(beacon_registry_revert_takes_an_advertisement_s_own_records_from_v15)
{
    LOCK(cs_main);
    grc_test::V15HeightGuard v15(1);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();
    registry.Reset();

    CKey key;
    key.MakeNewKey(true);
    const GRC::Cpid cpid_1 = GRC::Cpid::Parse("00010203040506070809101112131415");
    const GRC::Cpid cpid_2 = GRC::Cpid::Parse("15141312111009080706050403020100");
    const CKeyID id = key.GetPubKey().GetID();

    // Two advertisements of one key for one CPID: the pending entry under the key is the second.
    const uint256 first = AddPendingBeacon(registry, cpid_1, key.GetPubKey(), START_TIME, 1);
    const uint256 second = AddPendingBeacon(registry, cpid_1, key.GetPubKey(), START_TIME + DAY, 2);
    BOOST_REQUIRE(registry.PendingBeacons().at(id)->m_hash == second);

    RevertAdvertisement(registry, cpid_1, key.GetPubKey(), START_TIME + DAY, 2);
    BOOST_REQUIRE_EQUAL(registry.PendingBeacons().count(id), 1U);
    BOOST_CHECK(registry.PendingBeacons().at(id)->m_hash == first);
    BOOST_CHECK(registry.GetBeaconDB().find(second) == registry.GetBeaconDB().end());

    ReloadRegistry(registry);
    BOOST_REQUIRE_EQUAL(registry.PendingBeacons().count(id), 1U);
    BOOST_CHECK(registry.PendingBeacons().at(id)->m_hash == first);

    // A third advertisement of the key takes the pending entry; its revert restores the first.
    const uint256 other = AddPendingBeacon(registry, cpid_2, key.GetPubKey(), START_TIME + 2 * DAY, 3);
    BOOST_REQUIRE(registry.PendingBeacons().at(id)->m_hash == other);

    // A restart between the displacement and its revert: the load rebuilds the displacement link.
    ReloadRegistry(registry);
    BOOST_REQUIRE(registry.PendingBeacons().at(id)->m_hash == other);

    RevertAdvertisement(registry, cpid_2, key.GetPubKey(), START_TIME + 2 * DAY, 3);
    BOOST_REQUIRE_EQUAL(registry.PendingBeacons().count(id), 1U);
    BOOST_CHECK(registry.PendingBeacons().at(id)->m_hash == first);
    BOOST_CHECK(registry.PendingBeacons().at(id)->m_cpid == cpid_1);

    ReloadRegistry(registry);
    BOOST_REQUIRE_EQUAL(registry.PendingBeacons().count(id), 1U);
    BOOST_CHECK(registry.PendingBeacons().at(id)->m_hash == first);

    registry.Reset();
}

BOOST_AUTO_TEST_CASE(beacon_registry_revert_of_a_pending_beacon_deletion_restores_it_from_v15)
{
    LOCK(cs_main);
    grc_test::V15HeightGuard v15(1);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();
    registry.Reset();

    CKey key;
    key.MakeNewKey(true);
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    const CKeyID id = key.GetPubKey().GetID();

    const uint256 advertisement = AddPendingBeacon(registry, cpid, key.GetPubKey(), START_TIME, 1);
    const uint256 deletion = DeleteBeacon(registry, cpid, key.GetPubKey(), START_TIME + DAY, 2);
    BOOST_REQUIRE(registry.PendingBeacons().empty());
    BOOST_REQUIRE_EQUAL(registry.DeletedEntries(cpid).size(), 1U);

    // The revert finds its records even after they leave memory.
    registry.PassivateDB();
    RevertDeletion(registry, cpid, key.GetPubKey(), START_TIME + DAY, 2);

    BOOST_REQUIRE_EQUAL(registry.PendingBeacons().count(id), 1U);
    BOOST_CHECK(registry.PendingBeacons().at(id)->m_hash == advertisement);
    BOOST_CHECK(registry.GetBeaconDB().find(deletion) == registry.GetBeaconDB().end());
    BOOST_CHECK(registry.DeletedEntries(cpid).empty());

    ReloadRegistry(registry);
    BOOST_REQUIRE_EQUAL(registry.PendingBeacons().count(id), 1U);
    BOOST_CHECK(registry.PendingBeacons().at(id)->m_hash == advertisement);
    BOOST_CHECK(registry.DeletedEntries(cpid).empty());

    registry.Reset();
}

BOOST_AUTO_TEST_CASE(beacon_registry_revert_of_an_expired_beacon_deletion_restores_both_records_from_v15)
{
    LOCK(cs_main);
    grc_test::V15HeightGuard v15(1);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();
    registry.Reset();

    CKey key;
    key.MakeNewKey(true);
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    const CKeyID id = key.GetPubKey().GetID();
    const uint256 superblock = uint256S("0909090909090909090909090909090909090909090909090909090909090909");

    const uint256 advertisement = AddPendingBeacon(registry, cpid, key.GetPubKey(), START_TIME, 1);
    registry.ActivatePending({id}, START_TIME + DAY, superblock, 2);
    const uint256 activation = Hash(superblock, advertisement);

    // After the beacon expires, re-advertising the same key is not a renewal: it waits as a pending beacon.
    const int64_t later = START_TIME + GRC::Beacon::MAX_AGE + 2 * DAY;
    const uint256 readvertisement = AddPendingBeacon(registry, cpid, key.GetPubKey(), later, 3);
    BOOST_REQUIRE_EQUAL(registry.PendingBeacons().count(id), 1U);
    BOOST_REQUIRE(registry.PendingBeacons().at(id)->m_hash == readvertisement);

    // The deletion removes both the expired active beacon and the pending one.
    DeleteBeacon(registry, cpid, key.GetPubKey(), later + DAY, 4);
    BOOST_REQUIRE(registry.Try(cpid) == nullptr);
    BOOST_REQUIRE(registry.PendingBeacons().empty());

    RevertDeletion(registry, cpid, key.GetPubKey(), later + DAY, 4);
    BOOST_REQUIRE(registry.Try(cpid));
    BOOST_CHECK(registry.Try(cpid)->m_hash == activation);
    BOOST_REQUIRE_EQUAL(registry.PendingBeacons().count(id), 1U);
    BOOST_CHECK(registry.PendingBeacons().at(id)->m_hash == readvertisement);

    ReloadRegistry(registry);
    BOOST_REQUIRE(registry.Try(cpid));
    BOOST_CHECK(registry.Try(cpid)->m_hash == activation);
    BOOST_REQUIRE_EQUAL(registry.PendingBeacons().count(id), 1U);
    BOOST_CHECK(registry.PendingBeacons().at(id)->m_hash == readvertisement);

    registry.Reset();
}

BOOST_AUTO_TEST_CASE(beacon_registry_deleted_entries_are_in_chain_order)
{
    LOCK(cs_main);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();
    registry.Reset();

    CKey key_1;
    key_1.MakeNewKey(true);
    CKey key_2;
    key_2.MakeNewKey(true);
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    const uint256 superblock_1 = uint256S("0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a");
    const uint256 superblock_2 = uint256S("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b");

    const uint256 advertisement_1 = AddPendingBeacon(registry, cpid, key_1.GetPubKey(), START_TIME, 1);
    registry.ActivatePending({key_1.GetPubKey().GetID()}, START_TIME + DAY, superblock_1, 2);
    const uint256 deletion_1 = DeleteBeacon(registry, cpid, key_1.GetPubKey(), START_TIME + 2 * DAY, 3);

    const uint256 advertisement_2 = AddPendingBeacon(registry, cpid, key_2.GetPubKey(), START_TIME + 3 * DAY, 4);
    registry.ActivatePending({key_2.GetPubKey().GetID()}, START_TIME + 4 * DAY, superblock_2, 5);
    const uint256 deletion_2 = DeleteBeacon(registry, cpid, key_2.GetPubKey(), START_TIME + 5 * DAY, 6);

    for (const bool restart : {false, true}) {
        if (restart) ReloadRegistry(registry);

        const std::vector<GRC::Beacon_ptr> deleted = registry.DeletedEntries(cpid);
        BOOST_REQUIRE_EQUAL(deleted.size(), 2U);
        BOOST_CHECK(deleted[0]->m_hash == deletion_1);
        BOOST_CHECK(deleted[0]->m_previous_hash == Hash(superblock_1, advertisement_1));
        BOOST_CHECK(deleted[1]->m_hash == deletion_2);
        BOOST_CHECK(deleted[1]->m_previous_hash == Hash(superblock_2, advertisement_2));
    }

    registry.Reset();
}

BOOST_AUTO_TEST_CASE(beacon_registry_remove_carries_current_key)
{
    LOCK(cs_main);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();
    registry.Reset();

    CKey owner;
    owner.MakeNewKey(true);
    CKey other;
    other.MakeNewKey(true);
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    const GRC::Cpid unknown = GRC::Cpid::Parse("15141312111009080706050403020100");

    AddPendingBeacon(registry, cpid, owner.GetPubKey(), START_TIME, 1);
    registry.ActivatePending({owner.GetPubKey().GetID()}, START_TIME + DAY,
                             uint256S("0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c"), 2);

    const auto contract = [](const GRC::ContractAction action, const GRC::Cpid& for_cpid, const CKey& key) {
        GRC::BeaconPayload payload {2, for_cpid, GRC::Beacon(key.GetPubKey())};
        return GRC::MakeContract<GRC::BeaconPayload>(3, action, payload);
    };

    BOOST_CHECK(registry.RemoveCarriesCurrentKey(contract(GRC::ContractAction::REMOVE, cpid, owner)));
    BOOST_CHECK(!registry.RemoveCarriesCurrentKey(contract(GRC::ContractAction::REMOVE, cpid, other)));

    // A CPID with no beacon has nothing to remove.
    BOOST_CHECK(!registry.RemoveCarriesCurrentKey(contract(GRC::ContractAction::REMOVE, unknown, other)));

    // An advertisement is not a removal.
    BOOST_CHECK(registry.RemoveCarriesCurrentKey(contract(GRC::ContractAction::ADD, cpid, other)));

    registry.Reset();
}

BOOST_AUTO_TEST_CASE(beacon_registry_a_block_s_renewal_and_removal_revert_in_inverse_order_from_v15)
{
    LOCK(cs_main);

    // The block under test is at the v15 height itself.
    grc_test::V15HeightGuard v15(3);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();
    registry.Reset();

    CKey key;
    key.MakeNewKey(true);
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    const uint256 superblock = uint256S("0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e");

    const uint256 advertisement = AddPendingBeacon(registry, cpid, key.GetPubKey(), START_TIME, 1);
    registry.ActivatePending({key.GetPubKey().GetID()}, START_TIME + DAY, superblock, 2);
    const uint256 activation = Hash(superblock, advertisement);

    // Block 3 renews the beacon and then revokes it.
    const int64_t time_3 = START_TIME + 2 * DAY;
    CBlock block_3;
    block_3.vtx.resize(2);
    block_3.vtx.push_back(BeaconContractTx(GRC::ContractAction::ADD, cpid, key, time_3));
    block_3.vtx.push_back(BeaconContractTx(GRC::ContractAction::REMOVE, cpid, key, time_3));

    CBlockIndex index_3;
    index_3.nVersion = 13;
    index_3.nHeight = 3;
    index_3.nTime = time_3;

    GRC::RegistryBookmarks db_heights;
    bool found = false;
    GRC::ApplyContracts(block_3, &index_3, db_heights, found);
    BOOST_REQUIRE(registry.Try(cpid) == nullptr);

    GRC::RevertContracts(block_3, &index_3);

    BOOST_REQUIRE(registry.Try(cpid));
    BOOST_CHECK(registry.Try(cpid)->m_hash == activation);
    BOOST_CHECK(registry.GetBeaconDB().find(block_3.vtx[2].GetHash()) == registry.GetBeaconDB().end());
    BOOST_CHECK(registry.GetBeaconDB().find(block_3.vtx[3].GetHash()) == registry.GetBeaconDB().end());

    ReloadRegistry(registry);
    BOOST_REQUIRE(registry.Try(cpid));
    BOOST_CHECK(registry.Try(cpid)->m_hash == activation);

    registry.Reset();
}

BOOST_AUTO_TEST_CASE(beacon_registry_a_block_s_two_advertisements_revert_in_inverse_order_from_v15)
{
    LOCK(cs_main);
    grc_test::V15HeightGuard v15(1);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();
    registry.Reset();

    CKey key;
    key.MakeNewKey(true);
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    const CKeyID id = key.GetPubKey().GetID();

    // Block 1 carries two advertisements of one new key for one CPID. The pending entry under the key is the second.
    CBlock block_1;
    block_1.vtx.resize(2);
    block_1.vtx.push_back(BeaconContractTx(GRC::ContractAction::ADD, cpid, key, START_TIME));
    block_1.vtx.push_back(BeaconContractTx(GRC::ContractAction::ADD, cpid, key, START_TIME + 1));

    CBlockIndex index_1;
    index_1.nVersion = 13;
    index_1.nHeight = 1;
    index_1.nTime = START_TIME + 1;

    GRC::RegistryBookmarks db_heights;
    bool found = false;
    GRC::ApplyContracts(block_1, &index_1, db_heights, found);
    BOOST_REQUIRE_EQUAL(registry.PendingBeacons().count(id), 1U);
    BOOST_REQUIRE(registry.PendingBeacons().at(id)->m_hash == block_1.vtx[3].GetHash());

    GRC::RevertContracts(block_1, &index_1);

    // Nothing of the block survives, on the running node or after a restart.
    BOOST_CHECK(registry.PendingBeacons().empty());
    BOOST_CHECK(registry.GetBeaconDB().find(block_1.vtx[2].GetHash()) == registry.GetBeaconDB().end());
    BOOST_CHECK(registry.GetBeaconDB().find(block_1.vtx[3].GetHash()) == registry.GetBeaconDB().end());

    ReloadRegistry(registry);
    BOOST_CHECK(registry.PendingBeacons().empty());

    registry.Reset();
}

BOOST_AUTO_TEST_CASE(beacon_registry_replay_follows_connection_order)
{
    LOCK(cs_main);
    grc_test::V15HeightGuard v15(1);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();
    registry.Reset();

    CKey key;
    key.MakeNewKey(true);
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");

    const uint256 advertisement = AddPendingBeacon(registry, cpid, key.GetPubKey(), START_TIME, 1);

    // Block 2 carries a superblock that verifies the beacon, and a beacon advertisement.
    const int64_t time_2 = START_TIME + DAY;

    CBlockIndex index_2;
    index_2.nVersion = 13;
    index_2.nHeight = 2;
    index_2.nTime = time_2;
    index_2.MarkAsSuperblock();

    GRC::Superblock superblock;
    superblock.m_verified_beacons.m_verified.push_back(key.GetPubKey().GetID());

    GRC::Claim claim;
    claim.m_superblock = GRC::SuperblockPtr::BindShared(std::move(superblock), &index_2);

    CMutableTransaction coinbase;
    coinbase.vContracts.push_back(
        GRC::MakeContract<GRC::Claim>(uint32_t {3}, GRC::ContractAction::ADD, std::move(claim)));

    CBlock block_2;
    block_2.nVersion = 13;
    block_2.nTime = time_2;
    block_2.vtx.push_back(CTransaction(coinbase));
    block_2.vtx.emplace_back();
    block_2.vtx.push_back(BeaconContractTx(GRC::ContractAction::ADD, cpid, key, time_2));

    const uint256 block_2_hash = block_2.GetHash();
    index_2.phashBlock = &block_2_hash;

    GRC::RegistryBookmarks db_heights;
    bool found = false;
    GRC::ReplayBlock(block_2, &index_2, db_heights, true, found);

    // As block connection does: the superblock activates the advertisement, then the block's advertisement renews it.
    const GRC::BeaconOption beacon = registry.Try(cpid);
    BOOST_REQUIRE(beacon);
    BOOST_CHECK(beacon->m_status == GRC::BeaconStatusForStorage::RENEWAL);
    BOOST_CHECK(beacon->m_previous_hash == Hash(block_2_hash, advertisement));

    registry.Reset();
}

BOOST_AUTO_TEST_CASE(beacon_registry_restart_replay_of_two_advertisements_from_v15)
{
    LOCK(cs_main);
    grc_test::V15HeightGuard v15(1);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();
    registry.Reset();

    CKey key;
    key.MakeNewKey(true);
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    const CKeyID id = key.GetPubKey().GetID();

    const uint256 first = AddPendingBeacon(registry, cpid, key.GetPubKey(), START_TIME, 1);

    // Block 2, the newest beacon block, carries two more advertisements of the key.
    const CBlock block_2 = ContractBlock({BeaconContractTx(GRC::ContractAction::ADD, cpid, key, START_TIME + DAY),
                                          BeaconContractTx(GRC::ContractAction::ADD, cpid, key, START_TIME + DAY + 1)});
    const CBlockIndex index_2 = ContractIndex(2, START_TIME + DAY + 1);

    GRC::RegistryBookmarks db_heights;
    bool found = false;
    GRC::ApplyContracts(block_2, &index_2, db_heights, found);

    ReloadAndReplay(registry, block_2, index_2);
    BOOST_REQUIRE_EQUAL(registry.PendingBeacons().count(id), 1U);
    BOOST_REQUIRE(registry.PendingBeacons().at(id)->m_hash == block_2.vtx[3].GetHash());

    GRC::RevertContracts(block_2, &index_2);
    BOOST_REQUIRE_EQUAL(registry.PendingBeacons().count(id), 1U);
    BOOST_CHECK(registry.PendingBeacons().at(id)->m_hash == first);

    registry.Reset();
}

BOOST_AUTO_TEST_CASE(beacon_registry_restart_replay_of_a_deletion_and_advertisement_from_v15)
{
    LOCK(cs_main);
    grc_test::V15HeightGuard v15(1);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();
    registry.Reset();

    CKey key;
    key.MakeNewKey(true);
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    const CKeyID id = key.GetPubKey().GetID();

    const uint256 first = AddPendingBeacon(registry, cpid, key.GetPubKey(), START_TIME, 1);

    // Block 2, the newest beacon block, deletes the pending beacon and advertises the key again.
    const CBlock block_2 = ContractBlock({BeaconContractTx(GRC::ContractAction::REMOVE, cpid, key, START_TIME + DAY),
                                          BeaconContractTx(GRC::ContractAction::ADD, cpid, key, START_TIME + DAY + 1)});
    const CBlockIndex index_2 = ContractIndex(2, START_TIME + DAY + 1);

    GRC::RegistryBookmarks db_heights;
    bool found = false;
    GRC::ApplyContracts(block_2, &index_2, db_heights, found);

    ReloadAndReplay(registry, block_2, index_2);

    GRC::RevertContracts(block_2, &index_2);
    BOOST_REQUIRE_EQUAL(registry.PendingBeacons().count(id), 1U);
    BOOST_CHECK(registry.PendingBeacons().at(id)->m_hash == first);

    registry.Reset();
}

BOOST_AUTO_TEST_CASE(beacon_registry_restart_replay_of_a_renewal_and_revocation_from_v15)
{
    LOCK(cs_main);
    grc_test::V15HeightGuard v15(3);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();
    registry.Reset();

    CKey key;
    key.MakeNewKey(true);
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    const uint256 superblock = uint256S("0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f");

    const uint256 advertisement = AddPendingBeacon(registry, cpid, key.GetPubKey(), START_TIME, 1);
    registry.ActivatePending({key.GetPubKey().GetID()}, START_TIME + DAY, superblock, 2);
    const uint256 activation = Hash(superblock, advertisement);

    // Block 3, the newest beacon block, renews the beacon and then revokes it.
    const int64_t time_3 = START_TIME + 2 * DAY;
    const CBlock block_3 = ContractBlock({BeaconContractTx(GRC::ContractAction::ADD, cpid, key, time_3),
                                          BeaconContractTx(GRC::ContractAction::REMOVE, cpid, key, time_3)});
    const CBlockIndex index_3 = ContractIndex(3, time_3);

    GRC::RegistryBookmarks db_heights;
    bool found = false;
    GRC::ApplyContracts(block_3, &index_3, db_heights, found);

    // The restart's replay of block 3 must not change what the db holds.
    ReloadAndReplay(registry, block_3, index_3);
    BOOST_CHECK(registry.Try(cpid) == nullptr);
    BOOST_CHECK(registry.PendingBeacons().empty());

    ReloadAndReplay(registry, block_3, index_3);
    BOOST_CHECK(registry.PendingBeacons().empty());

    GRC::RevertContracts(block_3, &index_3);
    BOOST_REQUIRE(registry.Try(cpid));
    BOOST_CHECK(registry.Try(cpid)->m_hash == activation);
    BOOST_CHECK(registry.PendingBeacons().empty());

    ReloadRegistry(registry);
    BOOST_REQUIRE(registry.Try(cpid));
    BOOST_CHECK(registry.Try(cpid)->m_hash == activation);

    registry.Reset();
}

BOOST_AUTO_TEST_CASE(beacon_registry_restart_replay_of_two_renewals)
{
    LOCK(cs_main);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();
    registry.Reset();

    CKey key;
    key.MakeNewKey(true);
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    const uint256 superblock = uint256S("1010101010101010101010101010101010101010101010101010101010101010");

    const uint256 advertisement = AddPendingBeacon(registry, cpid, key.GetPubKey(), START_TIME, 1);
    registry.ActivatePending({key.GetPubKey().GetID()}, START_TIME + DAY, superblock, 2);
    const uint256 activation = Hash(superblock, advertisement);

    // Block 3, the newest beacon block, carries two renewals of the beacon.
    const int64_t time_3 = START_TIME + 2 * DAY;
    const CBlock block_3 = ContractBlock({BeaconContractTx(GRC::ContractAction::ADD, cpid, key, time_3),
                                          BeaconContractTx(GRC::ContractAction::ADD, cpid, key, time_3 + 1)});
    const CBlockIndex index_3 = ContractIndex(3, time_3 + 1);
    const uint256 renewal_1 = block_3.vtx[2].GetHash();
    const uint256 renewal_2 = block_3.vtx[3].GetHash();

    GRC::RegistryBookmarks db_heights;
    bool found = false;
    GRC::ApplyContracts(block_3, &index_3, db_heights, found);
    BOOST_REQUIRE(registry.Try(cpid) && registry.Try(cpid)->m_hash == renewal_2);

    for (int restart = 0; restart < 2; ++restart) {
        ReloadAndReplay(registry, block_3, index_3);

        BOOST_REQUIRE(registry.Try(cpid));
        BOOST_CHECK(registry.Try(cpid)->m_hash == renewal_2);

        const auto stored_renewal_1 = registry.GetBeaconDB().find(renewal_1);
        BOOST_REQUIRE(stored_renewal_1 != registry.GetBeaconDB().end());
        BOOST_CHECK(stored_renewal_1->second->m_previous_hash == activation);

        // The chain from the current beacon back to its root is intact.
        BOOST_CHECK_NO_THROW(registry.GetBeaconChainletRoot(registry.Try(cpid)));
    }

    registry.Reset();
}

BOOST_AUTO_TEST_CASE(beacon_registry_load_leaves_pending_beacons_at_a_renewal_record)
{
    LOCK(cs_main);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();
    registry.Reset();

    CKey key;
    key.MakeNewKey(true);
    const GRC::Cpid cpid_x = GRC::Cpid::Parse("00010203040506070809101112131415");
    const GRC::Cpid cpid_y = GRC::Cpid::Parse("15141312111009080706050403020100");
    const CKeyID id = key.GetPubKey().GetID();

    AddPendingBeacon(registry, cpid_x, key.GetPubKey(), START_TIME, 1);
    registry.ActivatePending({id}, START_TIME + DAY,
                             uint256S("1111111111111111111111111111111111111111111111111111111111111111"), 2);

    // A further advertisement of the key is pending when the CPID renews its beacon.
    const uint256 other = AddPendingBeacon(registry, cpid_y, key.GetPubKey(), START_TIME + 2 * DAY, 3);
    AddPendingBeacon(registry, cpid_x, key.GetPubKey(), START_TIME + 3 * DAY, 4);
    BOOST_REQUIRE_EQUAL(registry.PendingBeacons().count(id), 1U);
    BOOST_REQUIRE(registry.PendingBeacons().at(id)->m_hash == other);

    // A renewal does not touch the pending beacons, on a running node or after a restart.
    ReloadRegistry(registry);
    BOOST_REQUIRE_EQUAL(registry.PendingBeacons().count(id), 1U);
    BOOST_CHECK(registry.PendingBeacons().at(id)->m_hash == other);

    registry.Reset();
}

BOOST_AUTO_TEST_CASE(beacon_registry_superblock_state_is_current_only_for_its_own_superblock)
{
    LOCK(cs_main);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();
    registry.Reset();

    const uint256 superblock_1 = uint256S("1212121212121212121212121212121212121212121212121212121212121212");
    const uint256 superblock_2 = uint256S("1313131313131313131313131313131313131313131313131313131313131313");

    // A db with no marker counts as current for any superblock.
    BOOST_CHECK(registry.SuperblockStateCurrent(superblock_1));

    registry.ActivatePending({}, START_TIME, superblock_1, 3);
    BOOST_CHECK(registry.SuperblockStateCurrent(superblock_1));
    BOOST_CHECK(!registry.SuperblockStateCurrent(superblock_2));

    registry.Reset();
}

BOOST_AUTO_TEST_CASE(beacon_registry_reads_of_passivated_records_leave_them_passivated)
{
    LOCK(cs_main);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();
    registry.Reset();

    CKey key;
    key.MakeNewKey(true);
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");

    const uint256 advertisement = AddPendingBeacon(registry, cpid, key.GetPubKey(), START_TIME, 1);
    const CBlock block_2 = ContractBlock({BeaconContractTx(GRC::ContractAction::REMOVE, cpid, key, START_TIME + DAY)});
    const CBlockIndex index_2 = ContractIndex(2, START_TIME + DAY);

    GRC::RegistryBookmarks db_heights;
    bool found = false;
    GRC::ApplyContracts(block_2, &index_2, db_heights, found);
    BOOST_REQUIRE(registry.PendingBeacons().empty());

    registry.PassivateDB();
    const size_t in_memory = registry.GetBeaconDB().size();

    // Both records are stored, and neither test loads one.
    BOOST_CHECK(registry.GetBeaconDB().contains(advertisement));
    BOOST_CHECK(registry.GetBeaconDB().contains(block_2.vtx[2].GetHash()));
    BOOST_CHECK(!registry.GetBeaconDB().contains(uint256S("1515151515151515151515151515151515151515151515151515151515151515")));
    BOOST_CHECK_EQUAL(registry.GetBeaconDB().size(), in_memory);

    BOOST_CHECK_EQUAL(registry.DeletedEntries(cpid).size(), 1U);
    BOOST_CHECK_EQUAL(registry.GetBeaconDB().size(), in_memory);

    registry.Reset();
}

namespace {
//!
//! \brief Connect a superblock block as GridcoinConnectBlock does: activate the verified beacons, then apply the
//! block's contracts.
//!
void ConnectSuperblockBlock(GRC::BeaconRegistry& registry, const std::vector<uint160>& verified,
                            const uint256& superblock, const CBlock& block, const CBlockIndex& index)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    registry.ActivatePending(verified, index.nTime, superblock, index.nHeight);

    GRC::RegistryBookmarks db_heights;
    bool found = false;
    GRC::ApplyContracts(block, &index, db_heights, found);
}
} // anonymous namespace

BOOST_AUTO_TEST_CASE(beacon_registry_removal_s_key_is_judged_when_applied_from_v15)
{
    LOCK(cs_main);
    grc_test::V15HeightGuard v15(1);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();

    CKey key_0, key_1, key_3;
    key_0.MakeNewKey(true);
    key_1.MakeNewKey(true);
    key_3.MakeNewKey(true);
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");
    const uint256 superblock = uint256S("1616161616161616161616161616161616161616161616161616161616161616");

    // The CPID's first beacon is pending, with key 0. Block 2 carries the superblock that activates it, and a removal
    // that carries key 3. The removal is not applied.
    {
        registry.Reset();
        AddPendingBeacon(registry, cpid, key_0.GetPubKey(), START_TIME, 1);

        const CBlock block_2 = ContractBlock({BeaconContractTx(GRC::ContractAction::REMOVE, cpid, key_3,
                                                               START_TIME + DAY)});
        const CBlockIndex index_2 = ContractIndex(2, START_TIME + DAY);
        ConnectSuperblockBlock(registry, {key_0.GetPubKey().GetID()}, superblock, block_2, index_2);

        BOOST_REQUIRE(registry.Try(cpid));
        BOOST_CHECK(registry.Try(cpid)->m_public_key == key_0.GetPubKey());
        BOOST_CHECK(registry.DeletedEntries(cpid).empty());

        ReloadRegistry(registry);
        BOOST_REQUIRE(registry.Try(cpid));
        BOOST_CHECK(registry.Try(cpid)->m_public_key == key_0.GetPubKey());

        // Disconnected in the inverse order, the block leaves the beacon pending again, on a running node and after a
        // restart.
        GRC::RevertContracts(block_2, &index_2);
        registry.Deactivate(superblock, 2);
        BOOST_CHECK(registry.Try(cpid) == nullptr);
        BOOST_CHECK_EQUAL(registry.PendingBeacons().count(key_0.GetPubKey().GetID()), 1U);

        ReloadRegistry(registry);
        BOOST_CHECK(registry.Try(cpid) == nullptr);
        BOOST_CHECK_EQUAL(registry.PendingBeacons().count(key_0.GetPubKey().GetID()), 1U);
    }

    // The CPID is active with key 0 and advertised key 1. Block 4 carries the superblock that activates key 1, and a
    // removal that carries key 0. The removal is not applied.
    {
        registry.Reset();
        AddPendingBeacon(registry, cpid, key_0.GetPubKey(), START_TIME, 1);
        registry.ActivatePending({key_0.GetPubKey().GetID()}, START_TIME + DAY, superblock, 2);
        AddPendingBeacon(registry, cpid, key_1.GetPubKey(), START_TIME + 2 * DAY, 3);

        const CBlock block_4 = ContractBlock({BeaconContractTx(GRC::ContractAction::REMOVE, cpid, key_0,
                                                               START_TIME + 3 * DAY)});
        ConnectSuperblockBlock(registry, {key_1.GetPubKey().GetID()},
                               uint256S("1717171717171717171717171717171717171717171717171717171717171717"),
                               block_4, ContractIndex(4, START_TIME + 3 * DAY));

        BOOST_REQUIRE(registry.Try(cpid));
        BOOST_CHECK(registry.Try(cpid)->m_public_key == key_1.GetPubKey());

        ReloadRegistry(registry);
        BOOST_REQUIRE(registry.Try(cpid));
        BOOST_CHECK(registry.Try(cpid)->m_public_key == key_1.GetPubKey());
    }

    // A removal carrying the key the block activates removes that beacon.
    {
        registry.Reset();
        AddPendingBeacon(registry, cpid, key_0.GetPubKey(), START_TIME, 1);

        const CBlock block_2 = ContractBlock({BeaconContractTx(GRC::ContractAction::REMOVE, cpid, key_0,
                                                               START_TIME + DAY)});
        ConnectSuperblockBlock(registry, {key_0.GetPubKey().GetID()}, superblock, block_2,
                               ContractIndex(2, START_TIME + DAY));

        BOOST_CHECK(registry.Try(cpid) == nullptr);
        BOOST_CHECK_EQUAL(registry.DeletedEntries(cpid).size(), 1U);

        ReloadRegistry(registry);
        BOOST_CHECK(registry.Try(cpid) == nullptr);
    }

    registry.Reset();
}

BOOST_AUTO_TEST_CASE(beacon_registry_removal_needs_the_cpid_s_current_beacon_from_v15)
{
    LOCK(cs_main);
    grc_test::V15HeightGuard v15(1);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();
    registry.Reset();

    CKey key_1, key_3;
    key_1.MakeNewKey(true);
    key_3.MakeNewKey(true);
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");

    // The CPID's beacon is only pending, so the CPID has no current beacon. Block validation refuses a removal for
    // it, with key 3 and with the pending beacon's key.
    AddPendingBeacon(registry, cpid, key_1.GetPubKey(), START_TIME, 1);

    const CBlockIndex index_2 = ContractIndex(2, START_TIME + DAY);

    for (CKey* key : {&key_3, &key_1}) {
        // Signed, so that only the key rule can refuse it.
        GRC::BeaconPayload payload {2, cpid, GRC::Beacon(key->GetPubKey())};
        BOOST_REQUIRE(payload.Sign(*key));

        CMutableTransaction mtx;
        mtx.nTime = START_TIME + DAY;
        mtx.vContracts.push_back(GRC::MakeContract<GRC::BeaconPayload>(3, GRC::ContractAction::REMOVE, payload));
        const CTransaction removal(mtx);

        int DoS = 0;
        BOOST_REQUIRE(registry.Validate(removal.GetContracts().front(), removal, DoS));
        BOOST_CHECK(!registry.RemoveCarriesCurrentKey(removal.GetContracts().front()));
        BOOST_CHECK(!registry.BlockValidate({removal.GetContracts().front(), removal, &index_2}, DoS));
        BOOST_CHECK_EQUAL(DoS, 25);
    }

    registry.Reset();
}

BOOST_AUTO_TEST_CASE(beacon_registry_restart_replay_judges_a_removal_s_key_from_v15)
{
    LOCK(cs_main);
    grc_test::V15HeightGuard v15(1);

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();
    registry.Reset();

    CKey key_0, key_1;
    key_0.MakeNewKey(true);
    key_1.MakeNewKey(true);
    const GRC::Cpid cpid = GRC::Cpid::Parse("00010203040506070809101112131415");

    AddPendingBeacon(registry, cpid, key_0.GetPubKey(), START_TIME, 1);
    registry.ActivatePending({key_0.GetPubKey().GetID()}, START_TIME + DAY,
                             uint256S("1919191919191919191919191919191919191919191919191919191919191919"), 2);
    AddPendingBeacon(registry, cpid, key_1.GetPubKey(), START_TIME + 2 * DAY, 3);

    // Block 4 carries the superblock that activates key 1, and two removals that carry key 0.
    const CBlock block_4 = ContractBlock({BeaconContractTx(GRC::ContractAction::REMOVE, cpid, key_0, START_TIME + 3 * DAY),
                                          BeaconContractTx(GRC::ContractAction::REMOVE, cpid, key_0,
                                                           START_TIME + 3 * DAY + 1)});
    const CBlockIndex index_4 = ContractIndex(4, START_TIME + 3 * DAY + 1);

    for (const CTransaction& tx : {block_4.vtx[2], block_4.vtx[3]}) {
        BOOST_REQUIRE(registry.RemoveCarriesCurrentKey(tx.GetContracts().front()));
    }

    ConnectSuperblockBlock(registry, {key_1.GetPubKey().GetID()},
                           uint256S("2020202020202020202020202020202020202020202020202020202020202020"),
                           block_4, index_4);
    BOOST_REQUIRE(registry.Try(cpid) && registry.Try(cpid)->m_public_key == key_1.GetPubKey());
    BOOST_REQUIRE(registry.DeletedEntries(cpid).empty());

    // A restart while block 4 is the beacon db height replays its removals. The registry is unchanged.
    ReloadAndReplay(registry, block_4, index_4);
    BOOST_REQUIRE(registry.Try(cpid));
    BOOST_CHECK(registry.Try(cpid)->m_public_key == key_1.GetPubKey());
    BOOST_CHECK(registry.DeletedEntries(cpid).empty());
    BOOST_CHECK(registry.PendingBeacons().empty());

    registry.Reset();
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

BOOST_AUTO_TEST_SUITE_END()
