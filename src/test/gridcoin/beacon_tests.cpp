// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "gridcoin/beacon.h"
#include "rpc/blockchain.h"
#include "test/data/testnet_beacon.bin.h"
#include "test/data/mainnet_beacon.bin.h"
#include "txdb-leveldb.h"

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
        key.SetPrivKey(CPrivKey(private_key.begin(), private_key.end()));

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

    //!
    //! \brief Create a beacon payload signature signed by this private key.
    //!
    static std::vector<uint8_t> Signature()
    {
        CHashWriter hasher(SER_NETWORK, PROTOCOL_VERSION);

        hasher
            << GRC::BeaconPayload::CURRENT_VERSION
            << GRC::Beacon(Public())
            << GRC::Cpid::Parse("00010203040506070809101112131415");

        std::vector<uint8_t> signature;
        CKey private_key = Private();

        private_key.Sign(hasher.GetHash(), signature);

        return signature;
    }
}; // TestKey
} // anonymous namespace

// -----------------------------------------------------------------------------
// Beacon
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Beacon)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_empty_invalid_beacon)
{
    const GRC::Beacon beacon;

    BOOST_CHECK(beacon.m_public_key.Raw().empty() == true);
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
        + TestKey::Public().ToString());

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
        + TestKey::Public().ToString());

    BOOST_CHECK_EQUAL(beacon.ToString(), expected);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream)
{
    const GRC::Beacon beacon(TestKey::Public());

    const CDataStream expected = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << TestKey::Public();

    const CDataStream stream = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << beacon;

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
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
    BOOST_CHECK(payload.m_beacon.m_public_key.Raw().empty() == true);
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
        + TestKey::Public().ToString());

    const GRC::BeaconPayload payload = GRC::BeaconPayload::Parse(key, value);

    // Legacy beacon payloads always parse to version 1:
    BOOST_CHECK_EQUAL(payload.m_version, 1);
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

    CKey key;

    BOOST_CHECK(key.SetPubKey(TestKey::Public()));
    BOOST_CHECK(key.Verify(hasher.GetHash(), payload.m_signature));
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

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
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
    CDataStream data(SER_DISK, PROTOCOL_VERSION);

    data << testnet_beacon_bin;

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();

    // Make sure the registry is reset.
    registry.Reset();

    int64_t high_height_time = 0;
    int low_height = 0;
    int high_height = 0;
    int num_blocks = 0;

    data >> high_height_time;
    data >> low_height;
    data >> high_height;
    data >> num_blocks;

    // These should be set to correspond to the dumpcontracts run used to create testnet_beacon.bin
    BOOST_CHECK(high_height_time == 1613880656);
    BOOST_CHECK(low_height == 1301500);
    BOOST_CHECK(high_height == 1497976);
    BOOST_CHECK(num_blocks == 271);


    // Import the blocks in the file and replay the relevant contracts.
    for (int i = 0; i < num_blocks; ++i)
    {
        BOOST_TEST_CHECKPOINT("Processing block = " << i);

        GRC::ExportContractElement element;

        data >> element;

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
            }

            if (ctx->m_action == GRC::ContractAction::REMOVE)
            {
                registry.Delete(ctx);
            }
        }

        // Activate the pending beacons that are now verified, and also mark expired pending beacons expired.
        if (pindex->IsSuperblock())
        {
            registry.ActivatePending(element.m_verified_beacons,
                                     pindex->nTime,
                                     block_hash,
                                     pindex->nHeight);
        }
    }

    // Record the map of beacons and pending beacons after the contract replay. We have to have independent storage
    // of these, not pointers, because the maps are going to get reset for the second run (reinit).
    typedef std::unordered_map<GRC::Cpid, GRC::Beacon> LocalBeaconMap;
    typedef std::map<CKeyID, GRC::Beacon> LocalPendingBeaconMap;

    LocalBeaconMap beacons_init;

    for (const auto& iter : registry.Beacons())
    {
        beacons_init[iter.first] = *iter.second;
    }

    size_t init_number_beacons = beacons_init.size();

    LocalPendingBeaconMap pending_beacons_init;

    for (const auto& iter : registry.PendingBeacons())
    {
        pending_beacons_init[iter.first] = *iter.second;
    }

    size_t init_number_pending_beacons = pending_beacons_init.size();


    GRC::BeaconRegistry::HistoricalBeaconMap local_historical_beacon_map_init;

    size_t init_beacon_db_size = registry.GetBeaconDB().size();

    auto& init_beacon_db = registry.GetBeaconDB();

    auto init_beacon_db_iter = init_beacon_db.begin();
    while (init_beacon_db_iter != init_beacon_db.end())
    {
        const uint256& hash = init_beacon_db_iter->first;
        const GRC::Beacon& beacon = init_beacon_db_iter->second;

        local_historical_beacon_map_init[hash] = beacon;

        init_beacon_db_iter = init_beacon_db.advance(init_beacon_db_iter);
    }

    // Reset in memory structures only (which leaves leveldb undisturbed).
    registry.ResetInMemoryOnly();



    // (Re)initialize the registry from leveldb.
    registry.Initialize();

    LocalBeaconMap beacons_reinit;

    for (const auto& iter : registry.Beacons())
    {
        beacons_reinit[iter.first] = *iter.second;
    }

    size_t reinit_number_beacons = beacons_reinit.size();

    LocalPendingBeaconMap pending_beacons_reinit;

    for (const auto& iter : registry.PendingBeacons())
    {
        pending_beacons_reinit[iter.first] = *iter.second;
    }

    size_t reinit_number_pending_beacons = pending_beacons_reinit.size();


    GRC::BeaconRegistry::HistoricalBeaconMap local_historical_beacon_map_reinit;

    size_t reinit_beacon_db_size = registry.GetBeaconDB().size();

    auto& reinit_beacon_db = registry.GetBeaconDB();

    auto reinit_beacon_db_iter = reinit_beacon_db.begin();
    while (reinit_beacon_db_iter != reinit_beacon_db.end())
    {
        const uint256& hash = reinit_beacon_db_iter->first;
        const GRC::Beacon& beacon = reinit_beacon_db_iter->second;

        local_historical_beacon_map_reinit[hash] = beacon;

        reinit_beacon_db_iter = reinit_beacon_db.advance(reinit_beacon_db_iter);
    }


    BOOST_TEST_CHECKPOINT("init_beacon_db_size = " << init_beacon_db_size << ", "
                          << "reinit_beacon_db_size = " << reinit_beacon_db_size);

    BOOST_CHECK_EQUAL(init_beacon_db_size, reinit_beacon_db_size);


    bool beacon_db_comparison_success = true;

    // left join with init on the left
    for (const auto& left : local_historical_beacon_map_init)
    {
        uint256 hash = left.first;
        GRC::Beacon left_beacon = left.second;

        auto right = local_historical_beacon_map_reinit.find(hash);

        if (right == local_historical_beacon_map_reinit.end())
        {
            BOOST_TEST_CHECKPOINT("beacon in init beacon db not found in reinit beacon db for cpid "
                                  << left_beacon.m_cpid.ToString());

            beacon_db_comparison_success = false;

            std::cout << "MISSING: Reinit record missing for init record: "
                      << "hash = " << hash.GetHex()
                      << ", cpid = " << left.second.m_cpid.ToString()
                      << ", public key = " << left.second.m_public_key.ToString()
                      << ", address = " << left.second.GetAddress().ToString()
                      << ", timestamp = " << left.second.m_timestamp
                      << ", hash = " << left.second.m_hash.GetHex()
                      << ", prev beacon hash = " << left.second.m_prev_beacon_hash.GetHex()
                      << ", status = " << std::to_string(left.second.m_status.Raw())
                      << std::endl;

        }
        else if (left_beacon != right->second)
        {
            BOOST_TEST_CHECKPOINT("beacon in init beacon db does not match corresponding beacon"
                                  " in reinit beacon db for cpid "
                                  << left_beacon.m_cpid.ToString());

            beacon_db_comparison_success = false;

            // This is for console output in case the test fails and you run test_gridcoin manually from the command line.
            // You should be in the src directory for that, so the command would be ./test/test_gridcoin.
            std::cout << "MISMATCH: beacon in reinit beacon db does not match corresponding beacon"
                         " in init beacon db for hash = " << hash.GetHex() << std::endl;

            std::cout << "cpid = " << left_beacon.m_cpid.ToString() << std::endl;

            std::cout << "init_beacon public key = " << left_beacon.m_public_key.ToString()
                      << ", reinit_beacon public key = " << right->second.m_public_key.ToString() << std::endl;

            std::cout << "init_beacon address = " << left_beacon.GetAddress().ToString()
                      << ", reinit_beacon address = " << right->second.GetAddress().ToString() << std::endl;

            std::cout << "init_beacon timestamp = " << left_beacon.m_timestamp
                      << ", reinit_beacon timestamp = " << right->second.m_timestamp << std::endl;

            std::cout << "init_beacon hash = " << left_beacon.m_hash.GetHex()
                      << ", reinit_beacon hash = " << right->second.m_hash.GetHex() << std::endl;

            std::cout << "init_beacon prev beacon hash = " << left_beacon.m_prev_beacon_hash.GetHex()
                      << ", reinit_beacon prev beacon hash = " << right->second.m_prev_beacon_hash.GetHex() << std::endl;

            std::cout << "init_beacon status = " << std::to_string(left_beacon.m_status.Raw())
                      << ", reinit_beacon status = " << std::to_string(right->second.m_status.Raw()) << std::endl;
        }
    }


    // left join with reinit on the left
    for (const auto& left : local_historical_beacon_map_reinit)
    {
        uint256 hash = left.first;
        GRC::Beacon left_beacon = left.second;

        auto right = local_historical_beacon_map_init.find(hash);

        if (right == local_historical_beacon_map_init.end())
        {
            BOOST_TEST_CHECKPOINT("beacon in reinit beacon db not found in init beacon db for cpid "
                                  << left_beacon.m_cpid.ToString());

            beacon_db_comparison_success = false;

            std::cout << "MISSING: init record missing for reinit record: "
                      << "hash = " << hash.GetHex()
                      << ", cpid = " << left.second.m_cpid.ToString()
                      << ", public key = " << left.second.m_public_key.ToString()
                      << ", address = " << left.second.GetAddress().ToString()
                      << ", timestamp = " << left.second.m_timestamp
                      << ", hash = " << left.second.m_hash.GetHex()
                      << ", prev beacon hash = " << left.second.m_prev_beacon_hash.GetHex()
                      << ", status = " << std::to_string(left.second.m_status.Raw())
                      << std::endl;

        }
        else if (left_beacon != right->second)
        {
            BOOST_TEST_CHECKPOINT("beacon in init beacon db does not match corresponding beacon"
                                  " in reinit beacon db for cpid "
                                  << left_beacon.m_cpid.ToString());

            beacon_db_comparison_success = false;

            // This is for console output in case the test fails and you run test_gridcoin manually from the command line.
            // You should be in the src directory for that, so the command would be ./test/test_gridcoin.
            std::cout << "MISMATCH: beacon in init beacon db does not match corresponding beacon"
                         " in reinit beacon db for hash = " << hash.GetHex() << std::endl;

            std::cout << "cpid = " << left_beacon.m_cpid.ToString() << std::endl;

            std::cout << "reinit_beacon public key = " << left_beacon.m_public_key.ToString()
                      << ", init_beacon public key = " << right->second.m_public_key.ToString() << std::endl;

            std::cout << "reinit_beacon address = " << left_beacon.GetAddress().ToString()
                      << ", init_beacon address = " << right->second.GetAddress().ToString() << std::endl;

            std::cout << "reinit_beacon timestamp = " << left_beacon.m_timestamp
                      << ", init_beacon timestamp = " << right->second.m_timestamp << std::endl;

            std::cout << "reinit_beacon hash = " << left_beacon.m_hash.GetHex()
                      << ", init_beacon hash = " << right->second.m_hash.GetHex() << std::endl;

            std::cout << "reinit_beacon prev beacon hash = " << left_beacon.m_prev_beacon_hash.GetHex()
                      << ", init_beacon prev beacon hash = " << right->second.m_prev_beacon_hash.GetHex() << std::endl;

            std::cout << "reinit_beacon status = " << std::to_string(left_beacon.m_status.Raw())
                      << ", init_beacon status = " << std::to_string(right->second.m_status.Raw()) << std::endl;
        }
    }

    BOOST_CHECK(beacon_db_comparison_success);

    BOOST_CHECK_EQUAL(local_historical_beacon_map_init.size(), local_historical_beacon_map_reinit.size());




    BOOST_TEST_CHECKPOINT("init_number_beacons = " << init_number_beacons << ", "
                          << "reinit_number_beacons = " << reinit_number_beacons);

    bool number_beacons_equal = (init_number_beacons == reinit_number_beacons);

    if (!number_beacons_equal)
    {
        for (const auto& iter : beacons_init)
        {
            const GRC::Cpid& cpid = iter.first;
            const GRC::Beacon& beacon = iter.second;

            // This is for console output in case the test fails and you run test_gridcoin manually from the command line.
            // You should be in the src directory for that, so the command would be ./test/test_gridcoin.
            std::cout << "init_beacon cpid = " << cpid.ToString()
                      << ", public key = " << beacon.m_public_key.ToString()
                      << ", address = " << beacon.GetAddress().ToString()
                      << ", timestamp = " << beacon.m_timestamp
                      << ", hash = " << beacon.m_hash.GetHex()
                      << ", prev beacon hash = " << beacon.m_prev_beacon_hash.GetHex()
                      << ", status = " << std::to_string(beacon.m_status.Raw())
                      << std::endl;
        }

        for (const auto& iter : beacons_reinit)
        {
            const GRC::Cpid& cpid = iter.first;
            const GRC::Beacon& beacon = iter.second;

            // This is for console output in case the test fails and you run test_gridcoin manually from the command line.
            // You should be in the src directory for that, so the command would be ./test/test_gridcoin.
            std::cout << "reinit beacon cpid = " << cpid.ToString()
                      << ", public key = " << beacon.m_public_key.ToString()
                      << ", address = " << beacon.GetAddress().ToString()
                      << ", timestamp = " << beacon.m_timestamp
                      << ", hash = " << beacon.m_hash.GetHex()
                      << ", prev beacon hash = " << beacon.m_prev_beacon_hash.GetHex()
                      << ", status = " << std::to_string(beacon.m_status.Raw())
                      << std::endl;
        }
    }

    BOOST_CHECK_EQUAL(init_number_beacons, reinit_number_beacons);



    BOOST_TEST_CHECKPOINT("init_number_pending_beacons.size() = " << init_number_pending_beacons << ", "
                          << "reinit_number_pending_beacons.size() = " << reinit_number_pending_beacons);

    BOOST_CHECK_EQUAL(init_number_pending_beacons, reinit_number_pending_beacons);



    bool beacon_comparison_success = true;

    // left join with init on the left
    for (const auto& left : beacons_init)
    {
        GRC::Beacon left_beacon = left.second;
        auto right = beacons_reinit.find(left.first);

        if (right == beacons_reinit.end())
        {
            BOOST_TEST_CHECKPOINT("MISSING: beacon in init not found in reinit for cpid "
                                  << left.first.ToString());
            beacon_comparison_success = false;

            std::cout << "MISSING: reinit beacon record missing for init beacon record: "
                      << "hash = " << left.second.m_hash.GetHex()
                      << ", cpid = " << left.second.m_cpid.ToString()
                      << ", public key = " << left.second.m_public_key.ToString()
                      << ", address = " << left.second.GetAddress().ToString()
                      << ", timestamp = " << left.second.m_timestamp
                      << ", hash = " << left.second.m_hash.GetHex()
                      << ", prev beacon hash = " << left.second.m_prev_beacon_hash.GetHex()
                      << ", status = " << std::to_string(left.second.m_status.Raw())
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

            std::cout << "init_beacon public key = " << left_beacon.m_public_key.ToString()
                      << ", reinit_beacon public key = " << right->second.m_public_key.ToString() << std::endl;

            std::cout << "init_beacon timestamp = " << left_beacon.m_timestamp
                      << ", reinit_beacon timestamp = " << right->second.m_timestamp << std::endl;

            std::cout << "init_beacon hash = " << left_beacon.m_hash.GetHex()
                      << ", reinit_beacon hash = " << right->second.m_hash.GetHex() << std::endl;

            std::cout << "init_beacon prev beacon hash = " << left_beacon.m_prev_beacon_hash.GetHex()
                      << ", reinit_beacon prev beacon hash = " << right->second.m_prev_beacon_hash.GetHex() << std::endl;

            std::cout << "init_beacon status = " << std::to_string(left_beacon.m_status.Raw())
                      << ", reinit_beacon status = " << std::to_string(right->second.m_status.Raw()) << std::endl;
        }
    }


    // left join with reinit on the left
    for (const auto& left : beacons_reinit)
    {
        GRC::Beacon left_beacon = left.second;

        auto right = beacons_init.find(left.first);

        if (right == beacons_reinit.end())
        {
            BOOST_TEST_CHECKPOINT("MISSING: beacon in reinit not found in init for cpid "
                                  << left.first.ToString());
            beacon_comparison_success = false;

            std::cout << "MISSING: init beacon record missing for reinit beacon record: "
                      << "hash = " << left.second.m_hash.GetHex()
                      << ", cpid = " << left.second.m_cpid.ToString()
                      << ", public key = " << left.second.m_public_key.ToString()
                      << ", address = " << left.second.GetAddress().ToString()
                      << ", timestamp = " << left.second.m_timestamp
                      << ", hash = " << left.second.m_hash.GetHex()
                      << ", prev beacon hash = " << left.second.m_prev_beacon_hash.GetHex()
                      << ", status = " << std::to_string(left.second.m_status.Raw())
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

            std::cout << "reinit_beacon public key = " << left_beacon.m_public_key.ToString()
                      << ", init_beacon public key = " << right->second.m_public_key.ToString() << std::endl;

            std::cout << "reinit_beacon timestamp = " << left_beacon.m_timestamp
                      << ", init_beacon timestamp = " << right->second.m_timestamp << std::endl;

            std::cout << "reinit_beacon hash = " << left_beacon.m_hash.GetHex()
                      << ", init_beacon hash = " << right->second.m_hash.GetHex() << std::endl;

            std::cout << "reinit_beacon prev beacon hash = " << left_beacon.m_prev_beacon_hash.GetHex()
                      << ", init_beacon prev beacon hash = " << right->second.m_prev_beacon_hash.GetHex() << std::endl;

            std::cout << "reinit_beacon status = " << std::to_string(left_beacon.m_status.Raw())
                      << ", init_beacon status = " << std::to_string(right->second.m_status.Raw()) << std::endl;
        }
    }

    BOOST_CHECK(beacon_comparison_success);


    bool pending_beacon_comparison_success = true;

    // left join with init on the left
    for (const auto& left : pending_beacons_init)
    {
        GRC::Beacon left_beacon = left.second;
        auto right = pending_beacons_reinit.find(left.first);

        if (right == pending_beacons_reinit.end())
        {
            BOOST_TEST_CHECKPOINT("MISSING: pending beacon in init not found in reinit for CKeyID "
                                  << left.first.ToString());
            pending_beacon_comparison_success = false;

            std::cout << "MISSING: reinit pending beacon record missing for init pending beacon record: "
                      << "hash = " << left_beacon.m_hash.GetHex()
                      << ", cpid = " << left_beacon.m_cpid.ToString()
                      << ", public key = " << left_beacon.m_public_key.ToString()
                      << ", address = " << left_beacon.GetAddress().ToString()
                      << ", timestamp = " << left_beacon.m_timestamp
                      << ", hash = " << left_beacon.m_hash.GetHex()
                      << ", prev beacon hash = " << left_beacon.m_prev_beacon_hash.GetHex()
                      << ", status = " << std::to_string(left_beacon.m_status.Raw())
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

            std::cout << "init_pending_beacon public key = " << left_beacon.m_public_key.ToString()
                      << ", reinit_pending_beacon public key = " << right->second.m_public_key.ToString() << std::endl;

            std::cout << "init_pending_beacon timestamp = " << left_beacon.m_timestamp
                      << ", reinit_pending_beacon timestamp = " << right->second.m_timestamp << std::endl;

            std::cout << "init_pending_beacon hash = " << left_beacon.m_hash.GetHex()
                      << ", reinit_pending_beacon hash = " << right->second.m_hash.GetHex() << std::endl;

            std::cout << "init_pending_beacon prev beacon hash = " << left_beacon.m_prev_beacon_hash.GetHex()
                      << ", reinit_pending_beacon prev beacon hash = " << right->second.m_prev_beacon_hash.GetHex()
                      << std::endl;

            std::cout << ", init_pending_beacon status = " << std::to_string(left_beacon.m_status.Raw())
                      << ", reinit_pending_beacon status = " << std::to_string(right->second.m_status.Raw()) << std::endl;
        }
    }

    // left join with reinit on the left
    for (const auto& left : pending_beacons_reinit)
    {
        GRC::Beacon left_beacon = left.second;
        auto right = pending_beacons_init.find(left.first);

        if (right == pending_beacons_init.end())
        {
            BOOST_TEST_CHECKPOINT("MISSING: pending beacon in reinit not found in init for CKeyID "
                                  << left.first.ToString());
            pending_beacon_comparison_success = false;

            std::cout << "MISSING: init pending beacon record missing for reinit pending beacon record: "
                      << "hash = " << left.second.m_hash.GetHex()
                      << ", cpid = " << left.second.m_cpid.ToString()
                      << ", public key = " << left.second.m_public_key.ToString()
                      << ", address = " << left.second.GetAddress().ToString()
                      << ", timestamp = " << left.second.m_timestamp
                      << ", hash = " << left.second.m_hash.GetHex()
                      << ", prev beacon hash = " << left.second.m_prev_beacon_hash.GetHex()
                      << ", status = " << std::to_string(left.second.m_status.Raw())
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

            std::cout << "init_pending_beacon public key = " << left_beacon.m_public_key.ToString()
                      << ", reinit_pending_beacon public key = " << right->second.m_public_key.ToString() << std::endl;

            std::cout << "init_pending_beacon timestamp = " << left_beacon.m_timestamp
                      << ", reinit_pending_beacon timestamp = " << right->second.m_timestamp << std::endl;

            std::cout << "init_pending_beacon hash = " << left_beacon.m_hash.GetHex()
                      << ", reinit_pending_beacon hash = " << right->second.m_hash.GetHex() << std::endl;

            std::cout << "init_pending_beacon prev beacon hash = " << left_beacon.m_prev_beacon_hash.GetHex()
                      << ", reinit_pending_beacon prev beacon hash = " << right->second.m_prev_beacon_hash.GetHex()
                      << std::endl;

            std::cout << ", init_pending_beacon status = " << std::to_string(left_beacon.m_status.Raw())
                      << ", reinit_pending_beacon status = " << std::to_string(right->second.m_status.Raw()) << std::endl;
        }
    }

    BOOST_CHECK(pending_beacon_comparison_success);
}


BOOST_AUTO_TEST_CASE(beaconstorage_mainnet_test)
{
    CDataStream data(SER_DISK, PROTOCOL_VERSION);

    data << mainnet_beacon_bin;

    GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();

    // Make sure the registry is reset.
    registry.Reset();

    int64_t high_height_time = 0;
    int low_height = 0;
    int high_height = 0;
    int num_blocks = 0;

    data >> high_height_time;
    data >> low_height;
    data >> high_height;
    data >> num_blocks;

    // These should be set to correspond to the dumpcontracts run used to create mainnet_beacon.bin
    BOOST_CHECK(high_height_time == 1613904992);
    BOOST_CHECK(low_height == 2053000);
    BOOST_CHECK(high_height == 2177791);
    BOOST_CHECK(num_blocks == 2370);

    // Import the blocks in the file and replay the relevant contracts.
    for (int i = 0; i < num_blocks; ++i)
    {
        BOOST_TEST_CHECKPOINT("Processing block = " << i);

        GRC::ExportContractElement element;

        data >> element;

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
            }

            if (ctx->m_action == GRC::ContractAction::REMOVE)
            {
                registry.Delete(ctx);
            }
        }

        // Activate the pending beacons that are now verified, and also mark expired pending beacons expired.
        if (pindex->IsSuperblock())
        {
            registry.ActivatePending(element.m_verified_beacons,
                                     pindex->nTime,
                                     block_hash,
                                     pindex->nHeight);
        }
    }

    // Record the map of beacons and pending beacons after the contract replay. We have to have independent storage
    // of these, not pointers, because the maps are going to get reset for the second run (reinit).
    typedef std::unordered_map<GRC::Cpid, GRC::Beacon> LocalBeaconMap;
    typedef std::map<CKeyID, GRC::Beacon> LocalPendingBeaconMap;

    LocalBeaconMap beacons_init;

    for (const auto& iter : registry.Beacons())
    {
        beacons_init[iter.first] = *iter.second;
    }

    size_t init_number_beacons = beacons_init.size();

    LocalPendingBeaconMap pending_beacons_init;

    for (const auto& iter : registry.PendingBeacons())
    {
        pending_beacons_init[iter.first] = *iter.second;
    }

    size_t init_number_pending_beacons = pending_beacons_init.size();


    GRC::BeaconRegistry::HistoricalBeaconMap local_historical_beacon_map_init;

    size_t init_beacon_db_size = registry.GetBeaconDB().size();

    auto& init_beacon_db = registry.GetBeaconDB();

    auto init_beacon_db_iter = init_beacon_db.begin();
    while (init_beacon_db_iter != init_beacon_db.end())
    {
        const uint256& hash = init_beacon_db_iter->first;
        const GRC::Beacon& beacon = init_beacon_db_iter->second;

        local_historical_beacon_map_init[hash] = beacon;

        init_beacon_db_iter = init_beacon_db.advance(init_beacon_db_iter);
    }

    // Reset in memory structures only (which leaves leveldb undisturbed).
    registry.ResetInMemoryOnly();



    // (Re)initialize the registry from leveldb.
    registry.Initialize();

    LocalBeaconMap beacons_reinit;

    for (const auto& iter : registry.Beacons())
    {
        beacons_reinit[iter.first] = *iter.second;
    }

    size_t reinit_number_beacons = beacons_reinit.size();

    LocalPendingBeaconMap pending_beacons_reinit;

    for (const auto& iter : registry.PendingBeacons())
    {
        pending_beacons_reinit[iter.first] = *iter.second;
    }

    size_t reinit_number_pending_beacons = pending_beacons_reinit.size();


    GRC::BeaconRegistry::HistoricalBeaconMap local_historical_beacon_map_reinit;

    size_t reinit_beacon_db_size = registry.GetBeaconDB().size();

    auto& reinit_beacon_db = registry.GetBeaconDB();

    auto reinit_beacon_db_iter = reinit_beacon_db.begin();
    while (reinit_beacon_db_iter != reinit_beacon_db.end())
    {
        const uint256& hash = reinit_beacon_db_iter->first;
        const GRC::Beacon& beacon = reinit_beacon_db_iter->second;

        local_historical_beacon_map_reinit[hash] = beacon;

        reinit_beacon_db_iter = reinit_beacon_db.advance(reinit_beacon_db_iter);
    }


    BOOST_TEST_CHECKPOINT("init_beacon_db_size = " << init_beacon_db_size << ", "
                          << "reinit_beacon_db_size = " << reinit_beacon_db_size);

    BOOST_CHECK_EQUAL(init_beacon_db_size, reinit_beacon_db_size);


    bool beacon_db_comparison_success = true;

    // left join with init on the left
    for (const auto& left : local_historical_beacon_map_init)
    {
        uint256 hash = left.first;
        GRC::Beacon left_beacon = left.second;

        auto right = local_historical_beacon_map_reinit.find(hash);

        if (right == local_historical_beacon_map_reinit.end())
        {
            BOOST_TEST_CHECKPOINT("beacon in init beacon db not found in reinit beacon db for cpid "
                                  << left_beacon.m_cpid.ToString());

            beacon_db_comparison_success = false;

            std::cout << "MISSING: Reinit record missing for init record: "
                      << "hash = " << hash.GetHex()
                      << ", cpid = " << left.second.m_cpid.ToString()
                      << ", public key = " << left.second.m_public_key.ToString()
                      << ", address = " << left.second.GetAddress().ToString()
                      << ", timestamp = " << left.second.m_timestamp
                      << ", hash = " << left.second.m_hash.GetHex()
                      << ", prev beacon hash = " << left.second.m_prev_beacon_hash.GetHex()
                      << ", status = " << std::to_string(left.second.m_status.Raw())
                      << std::endl;

        }
        else if (left_beacon != right->second)
        {
            BOOST_TEST_CHECKPOINT("beacon in init beacon db does not match corresponding beacon"
                                  " in reinit beacon db for cpid "
                                  << left_beacon.m_cpid.ToString());

            beacon_db_comparison_success = false;

            // This is for console output in case the test fails and you run test_gridcoin manually from the command line.
            // You should be in the src directory for that, so the command would be ./test/test_gridcoin.
            std::cout << "MISMATCH: beacon in reinit beacon db does not match corresponding beacon"
                         " in init beacon db for hash = " << hash.GetHex() << std::endl;

            std::cout << "cpid = " << left_beacon.m_cpid.ToString() << std::endl;

            std::cout << "init_beacon public key = " << left_beacon.m_public_key.ToString()
                      << ", reinit_beacon public key = " << right->second.m_public_key.ToString() << std::endl;

            std::cout << "init_beacon address = " << left_beacon.GetAddress().ToString()
                      << ", reinit_beacon address = " << right->second.GetAddress().ToString() << std::endl;

            std::cout << "init_beacon timestamp = " << left_beacon.m_timestamp
                      << ", reinit_beacon timestamp = " << right->second.m_timestamp << std::endl;

            std::cout << "init_beacon hash = " << left_beacon.m_hash.GetHex()
                      << ", reinit_beacon hash = " << right->second.m_hash.GetHex() << std::endl;

            std::cout << "init_beacon prev beacon hash = " << left_beacon.m_prev_beacon_hash.GetHex()
                      << ", reinit_beacon prev beacon hash = " << right->second.m_prev_beacon_hash.GetHex() << std::endl;

            std::cout << "init_beacon status = " << std::to_string(left_beacon.m_status.Raw())
                      << ", reinit_beacon status = " << std::to_string(right->second.m_status.Raw()) << std::endl;
        }
    }


    // left join with reinit on the left
    for (const auto& left : local_historical_beacon_map_reinit)
    {
        uint256 hash = left.first;
        GRC::Beacon left_beacon = left.second;

        auto right = local_historical_beacon_map_init.find(hash);

        if (right == local_historical_beacon_map_init.end())
        {
            BOOST_TEST_CHECKPOINT("beacon in reinit beacon db not found in init beacon db for cpid "
                                  << left_beacon.m_cpid.ToString());

            beacon_db_comparison_success = false;

            std::cout << "MISSING: init record missing for reinit record: "
                      << "hash = " << hash.GetHex()
                      << ", cpid = " << left.second.m_cpid.ToString()
                      << ", public key = " << left.second.m_public_key.ToString()
                      << ", address = " << left.second.GetAddress().ToString()
                      << ", timestamp = " << left.second.m_timestamp
                      << ", hash = " << left.second.m_hash.GetHex()
                      << ", prev beacon hash = " << left.second.m_prev_beacon_hash.GetHex()
                      << ", status = " << std::to_string(left.second.m_status.Raw())
                      << std::endl;

        }
        else if (left_beacon != right->second)
        {
            BOOST_TEST_CHECKPOINT("beacon in init beacon db does not match corresponding beacon"
                                  " in reinit beacon db for cpid "
                                  << left_beacon.m_cpid.ToString());

            beacon_db_comparison_success = false;

            // This is for console output in case the test fails and you run test_gridcoin manually from the command line.
            // You should be in the src directory for that, so the command would be ./test/test_gridcoin.
            std::cout << "MISMATCH: beacon in init beacon db does not match corresponding beacon"
                         " in reinit beacon db for hash = " << hash.GetHex() << std::endl;

            std::cout << "cpid = " << left_beacon.m_cpid.ToString() << std::endl;

            std::cout << "reinit_beacon public key = " << left_beacon.m_public_key.ToString()
                      << ", init_beacon public key = " << right->second.m_public_key.ToString() << std::endl;

            std::cout << "reinit_beacon address = " << left_beacon.GetAddress().ToString()
                      << ", init_beacon address = " << right->second.GetAddress().ToString() << std::endl;

            std::cout << "reinit_beacon timestamp = " << left_beacon.m_timestamp
                      << ", init_beacon timestamp = " << right->second.m_timestamp << std::endl;

            std::cout << "reinit_beacon hash = " << left_beacon.m_hash.GetHex()
                      << ", init_beacon hash = " << right->second.m_hash.GetHex() << std::endl;

            std::cout << "reinit_beacon prev beacon hash = " << left_beacon.m_prev_beacon_hash.GetHex()
                      << ", init_beacon prev beacon hash = " << right->second.m_prev_beacon_hash.GetHex() << std::endl;

            std::cout << "reinit_beacon status = " << std::to_string(left_beacon.m_status.Raw())
                      << ", init_beacon status = " << std::to_string(right->second.m_status.Raw()) << std::endl;
        }
    }

    BOOST_CHECK(beacon_db_comparison_success);

    BOOST_CHECK_EQUAL(local_historical_beacon_map_init.size(), local_historical_beacon_map_reinit.size());




    BOOST_TEST_CHECKPOINT("init_number_beacons = " << init_number_beacons << ", "
                          << "reinit_number_beacons = " << reinit_number_beacons);

    bool number_beacons_equal = (init_number_beacons == reinit_number_beacons);

    if (!number_beacons_equal)
    {
        for (const auto& iter : beacons_init)
        {
            const GRC::Cpid& cpid = iter.first;
            const GRC::Beacon& beacon = iter.second;

            // This is for console output in case the test fails and you run test_gridcoin manually from the command line.
            // You should be in the src directory for that, so the command would be ./test/test_gridcoin.
            std::cout << "init_beacon cpid = " << cpid.ToString()
                      << ", public key = " << beacon.m_public_key.ToString()
                      << ", address = " << beacon.GetAddress().ToString()
                      << ", timestamp = " << beacon.m_timestamp
                      << ", hash = " << beacon.m_hash.GetHex()
                      << ", prev beacon hash = " << beacon.m_prev_beacon_hash.GetHex()
                      << ", status = " << std::to_string(beacon.m_status.Raw())
                      << std::endl;
        }

        for (const auto& iter : beacons_reinit)
        {
            const GRC::Cpid& cpid = iter.first;
            const GRC::Beacon& beacon = iter.second;

            // This is for console output in case the test fails and you run test_gridcoin manually from the command line.
            // You should be in the src directory for that, so the command would be ./test/test_gridcoin.
            std::cout << "reinit beacon cpid = " << cpid.ToString()
                      << ", public key = " << beacon.m_public_key.ToString()
                      << ", address = " << beacon.GetAddress().ToString()
                      << ", timestamp = " << beacon.m_timestamp
                      << ", hash = " << beacon.m_hash.GetHex()
                      << ", prev beacon hash = " << beacon.m_prev_beacon_hash.GetHex()
                      << ", status = " << std::to_string(beacon.m_status.Raw())
                      << std::endl;
        }
    }

    BOOST_CHECK_EQUAL(init_number_beacons, reinit_number_beacons);



    BOOST_TEST_CHECKPOINT("init_number_pending_beacons.size() = " << init_number_pending_beacons << ", "
                          << "reinit_number_pending_beacons.size() = " << reinit_number_pending_beacons);

    BOOST_CHECK_EQUAL(init_number_pending_beacons, reinit_number_pending_beacons);



    bool beacon_comparison_success = true;

    // left join with init on the left
    for (const auto& left : beacons_init)
    {
        GRC::Beacon left_beacon = left.second;
        auto right = beacons_reinit.find(left.first);

        if (right == beacons_reinit.end())
        {
            BOOST_TEST_CHECKPOINT("MISSING: beacon in init not found in reinit for cpid "
                                  << left.first.ToString());
            beacon_comparison_success = false;

            std::cout << "MISSING: reinit beacon record missing for init beacon record: "
                      << "hash = " << left.second.m_hash.GetHex()
                      << ", cpid = " << left.second.m_cpid.ToString()
                      << ", public key = " << left.second.m_public_key.ToString()
                      << ", address = " << left.second.GetAddress().ToString()
                      << ", timestamp = " << left.second.m_timestamp
                      << ", hash = " << left.second.m_hash.GetHex()
                      << ", prev beacon hash = " << left.second.m_prev_beacon_hash.GetHex()
                      << ", status = " << std::to_string(left.second.m_status.Raw())
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

            std::cout << "init_beacon public key = " << left_beacon.m_public_key.ToString()
                      << ", reinit_beacon public key = " << right->second.m_public_key.ToString() << std::endl;

            std::cout << "init_beacon timestamp = " << left_beacon.m_timestamp
                      << ", reinit_beacon timestamp = " << right->second.m_timestamp << std::endl;

            std::cout << "init_beacon hash = " << left_beacon.m_hash.GetHex()
                      << ", reinit_beacon hash = " << right->second.m_hash.GetHex() << std::endl;

            std::cout << "init_beacon prev beacon hash = " << left_beacon.m_prev_beacon_hash.GetHex()
                      << ", reinit_beacon prev beacon hash = " << right->second.m_prev_beacon_hash.GetHex() << std::endl;

            std::cout << "init_beacon status = " << std::to_string(left_beacon.m_status.Raw())
                      << ", reinit_beacon status = " << std::to_string(right->second.m_status.Raw()) << std::endl;
        }
    }


    // left join with reinit on the left
    for (const auto& left : beacons_reinit)
    {
        GRC::Beacon left_beacon = left.second;

        auto right = beacons_init.find(left.first);

        if (right == beacons_reinit.end())
        {
            BOOST_TEST_CHECKPOINT("MISSING: beacon in reinit not found in init for cpid "
                                  << left.first.ToString());
            beacon_comparison_success = false;

            std::cout << "MISSING: init beacon record missing for reinit beacon record: "
                      << "hash = " << left.second.m_hash.GetHex()
                      << ", cpid = " << left.second.m_cpid.ToString()
                      << ", public key = " << left.second.m_public_key.ToString()
                      << ", address = " << left.second.GetAddress().ToString()
                      << ", timestamp = " << left.second.m_timestamp
                      << ", hash = " << left.second.m_hash.GetHex()
                      << ", prev beacon hash = " << left.second.m_prev_beacon_hash.GetHex()
                      << ", status = " << std::to_string(left.second.m_status.Raw())
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

            std::cout << "reinit_beacon public key = " << left_beacon.m_public_key.ToString()
                      << ", init_beacon public key = " << right->second.m_public_key.ToString() << std::endl;

            std::cout << "reinit_beacon timestamp = " << left_beacon.m_timestamp
                      << ", init_beacon timestamp = " << right->second.m_timestamp << std::endl;

            std::cout << "reinit_beacon hash = " << left_beacon.m_hash.GetHex()
                      << ", init_beacon hash = " << right->second.m_hash.GetHex() << std::endl;

            std::cout << "reinit_beacon prev beacon hash = " << left_beacon.m_prev_beacon_hash.GetHex()
                      << ", init_beacon prev beacon hash = " << right->second.m_prev_beacon_hash.GetHex() << std::endl;

            std::cout << "reinit_beacon status = " << std::to_string(left_beacon.m_status.Raw())
                      << ", init_beacon status = " << std::to_string(right->second.m_status.Raw()) << std::endl;
        }
    }

    BOOST_CHECK(beacon_comparison_success);


    bool pending_beacon_comparison_success = true;

    // left join with init on the left
    for (const auto& left : pending_beacons_init)
    {
        GRC::Beacon left_beacon = left.second;
        auto right = pending_beacons_reinit.find(left.first);

        if (right == pending_beacons_reinit.end())
        {
            BOOST_TEST_CHECKPOINT("MISSING: pending beacon in init not found in reinit for CKeyID "
                                  << left.first.ToString());
            pending_beacon_comparison_success = false;

            std::cout << "MISSING: reinit pending beacon record missing for init pending beacon record: "
                      << "hash = " << left_beacon.m_hash.GetHex()
                      << ", cpid = " << left_beacon.m_cpid.ToString()
                      << ", public key = " << left_beacon.m_public_key.ToString()
                      << ", address = " << left_beacon.GetAddress().ToString()
                      << ", timestamp = " << left_beacon.m_timestamp
                      << ", hash = " << left_beacon.m_hash.GetHex()
                      << ", prev beacon hash = " << left_beacon.m_prev_beacon_hash.GetHex()
                      << ", status = " << std::to_string(left_beacon.m_status.Raw())
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

            std::cout << "init_pending_beacon public key = " << left_beacon.m_public_key.ToString()
                      << ", reinit_pending_beacon public key = " << right->second.m_public_key.ToString() << std::endl;

            std::cout << "init_pending_beacon timestamp = " << left_beacon.m_timestamp
                      << ", reinit_pending_beacon timestamp = " << right->second.m_timestamp << std::endl;

            std::cout << "init_pending_beacon hash = " << left_beacon.m_hash.GetHex()
                      << ", reinit_pending_beacon hash = " << right->second.m_hash.GetHex() << std::endl;

            std::cout << "init_pending_beacon prev beacon hash = " << left_beacon.m_prev_beacon_hash.GetHex()
                      << ", reinit_pending_beacon prev beacon hash = " << right->second.m_prev_beacon_hash.GetHex()
                      << std::endl;

            std::cout << ", init_pending_beacon status = " << std::to_string(left_beacon.m_status.Raw())
                      << ", reinit_pending_beacon status = " << std::to_string(right->second.m_status.Raw()) << std::endl;
        }
    }

    // left join with reinit on the left
    for (const auto& left : pending_beacons_reinit)
    {
        GRC::Beacon left_beacon = left.second;
        auto right = pending_beacons_init.find(left.first);

        if (right == pending_beacons_init.end())
        {
            BOOST_TEST_CHECKPOINT("MISSING: pending beacon in reinit not found in init for CKeyID "
                                  << left.first.ToString());
            pending_beacon_comparison_success = false;

            std::cout << "MISSING: init pending beacon record missing for reinit pending beacon record: "
                      << "hash = " << left.second.m_hash.GetHex()
                      << ", cpid = " << left.second.m_cpid.ToString()
                      << ", public key = " << left.second.m_public_key.ToString()
                      << ", address = " << left.second.GetAddress().ToString()
                      << ", timestamp = " << left.second.m_timestamp
                      << ", hash = " << left.second.m_hash.GetHex()
                      << ", prev beacon hash = " << left.second.m_prev_beacon_hash.GetHex()
                      << ", status = " << std::to_string(left.second.m_status.Raw())
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

            std::cout << "init_pending_beacon public key = " << left_beacon.m_public_key.ToString()
                      << ", reinit_pending_beacon public key = " << right->second.m_public_key.ToString() << std::endl;

            std::cout << "init_pending_beacon timestamp = " << left_beacon.m_timestamp
                      << ", reinit_pending_beacon timestamp = " << right->second.m_timestamp << std::endl;

            std::cout << "init_pending_beacon hash = " << left_beacon.m_hash.GetHex()
                      << ", reinit_pending_beacon hash = " << right->second.m_hash.GetHex() << std::endl;

            std::cout << "init_pending_beacon prev beacon hash = " << left_beacon.m_prev_beacon_hash.GetHex()
                      << ", reinit_pending_beacon prev beacon hash = " << right->second.m_prev_beacon_hash.GetHex()
                      << std::endl;

            std::cout << ", init_pending_beacon status = " << std::to_string(left_beacon.m_status.Raw())
                      << ", reinit_pending_beacon status = " << std::to_string(right->second.m_status.Raw()) << std::endl;
        }
    }

    BOOST_CHECK(pending_beacon_comparison_success);
}

BOOST_AUTO_TEST_SUITE_END()
