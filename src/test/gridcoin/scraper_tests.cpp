// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/project.h"
#include "gridcoin/scraper/scraper.h"
#include "gridcoin/scraper/scraper_net.h"
#include "gridcoin/scraper/scraper_registry.h"
#include "gridcoin/superblock.h"
#include "hash.h"
#include "key.h"
#include "key_io.h"
#include "node/ui_interface.h"
#include "primitives/transaction.h"
#include "rpc/server.h"
#include "streams.h"
#include "test/gridcoin/rsa_test_vectors.h"
#include "test/staking_test_peers.h"
#include "test/state_guard.h"
#include <util/strencodings.h>
#include <util/string.h>
#include <util/system.h>
#include <util/time.h>
#include <version.h>

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/test/unit_test.hpp>

#include <boost/signals2/connection.hpp>

#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// Defined in scraper.cpp, which declares them there rather than in a header.
extern fs::path pathDataDir;
extern ConvergedScraperStats ConvergedScraperStatsCache GUARDED_BY(cs_ConvergedScraperStatsCache);
extern bool ScraperConstructConvergedManifestByProject(const GRC::WhitelistSnapshot& projectWhitelist,
                                                       mmCSManifestsBinnedByScraper& mMapCSManifestsBinnedByScraper,
                                                       ConvergedManifest& StructConvergedManifest);

namespace {

const std::string PROJECT_A = "https://project-a.example.org/";
const std::string PROJECT_B = "https://project-b.example.org/";
const std::string CPID = "00010203040506070809101112131415";

// The account ID and beacon key that rsa_test_vectors::g_test_message is made of.
const uint32_t ACCOUNT_ID = 19100117;
const std::string BEACON_KEY_HEX = "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890ab";
const std::string OTHER_BEACON_KEY_HEX = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef01";

ScraperPendingBeaconEntry MakeCandidate(const std::string& master_url,
                                        uint32_t account_id,
                                        const std::string& beacon_key_hex,
                                        const std::vector<uint8_t>& signature)
{
    ScraperPendingBeaconEntry entry;

    entry.cpid = CPID;
    entry.timestamp = 0;
    entry.has_ownership_proof = true;
    entry.ownership_master_url = master_url;
    entry.ownership_account_id = account_id;
    entry.ownership_rsa_signature = signature;
    entry.beacon_public_key_hex = beacon_key_hex;

    return entry;
}

std::vector<const ScraperPendingBeaconMap::value_type*> Candidates(const ScraperPendingBeaconMap& pending)
{
    std::vector<const ScraperPendingBeaconMap::value_type*> candidates;

    for (const auto& entry : pending) {
        candidates.push_back(&entry);
    }

    return candidates;
}

std::vector<uint8_t> ValidSignature()
{
    return ParseHex(rsa_test_vectors::g_test_signature_hex);
}

GRC::WhitelistSnapshot OneProjectWhitelist()
{
    return GRC::WhitelistSnapshot(std::make_shared<GRC::ProjectList>(GRC::ProjectList {
        GRC::Project("project_a", PROJECT_A, 1234567),
    }));
}

//! Points the scraper logger at the suite's data directory. The scraper threads set pathDataDir, and the unit tests
//! do not start them, so without this the logger would create scraper.log in the working directory the first time a
//! case logs a warning.
class ScraperLogInDataDir
{
public:
    ScraperLogInDataDir() : m_saved(pathDataDir)
    {
        if (pathDataDir.empty()) pathDataDir = GetDataDir();
    }

    ~ScraperLogInDataDir()
    {
        pathDataDir = m_saved;
    }

private:
    const fs::path m_saved;
};

//! Sets CONVERGENCE_BY_PROJECT_RATIO for the life of the object.
class ConvergenceByProjectRatio
{
public:
    explicit ConvergenceByProjectRatio(double ratio)
    {
        LOCK(cs_ScraperGlobals);

        m_saved = CONVERGENCE_BY_PROJECT_RATIO;
        CONVERGENCE_BY_PROJECT_RATIO = ratio;
    }

    ~ConvergenceByProjectRatio()
    {
        LOCK(cs_ScraperGlobals);

        CONVERGENCE_BY_PROJECT_RATIO = m_saved;
    }

private:
    double m_saved{0.0};
};

//! Gzip of the text, as a scraper compresses the files it publishes.
std::string Gzip(const std::string& text)
{
    std::istringstream in(text);
    std::ostringstream out;

    boost::iostreams::filtering_istream filter;
    filter.push(boost::iostreams::gzip_compressor());
    filter.push(in);
    boost::iostreams::copy(filter, out);

    return out.str();
}

//! New keys, each registered as an authorized scraper. The scraper registry is emptied when the object goes.
class AuthorizedScrapers
{
public:
    explicit AuthorizedScrapers(const size_t count) : m_keys(count), m_txs(count)
    {
        for (size_t n = 0; n < count; ++n) {
            m_keys[n].MakeNewKey(true);

            CMutableTransaction mtx;
            mtx.vContracts.push_back(GRC::MakeContract<GRC::ScraperEntryPayload>(
                uint32_t {3}, GRC::ContractAction::ADD, uint32_t {2}, m_keys[n].GetPubKey().GetID(),
                GRC::ScraperEntryStatus::AUTHORIZED));
            m_txs[n] = CTransaction(mtx);

            LOCK(cs_main);
            GRC::GetScraperRegistry().Add({m_txs[n].vContracts[0], m_txs[n], &m_index});
        }
    }

    AuthorizedScrapers(const AuthorizedScrapers&) = delete;
    AuthorizedScrapers& operator=(const AuthorizedScrapers&) = delete;

    CKey& Key(const size_t n) { return m_keys[n]; }

private:
    grc_test::RegistryReset m_scrapers{GRC::ContractType::SCRAPER};
    std::vector<CKey> m_keys;
    std::vector<CTransaction> m_txs;
    CBlockIndex m_index{};
};

//! The statistics of the one project in every manifest a case publishes, one record of total credit, the time of
//! the average credit, the average credit and the CPID.
const std::string PROJECT_STATISTICS = "100,1700000000,10,00010203040506070809101112131415\n";

//! A manifest published as a scraper publishes one, with a beacon list as its first part and the statistics of one
//! project as its second, and the cache of the converged statistics marked as ScraperSendFileManifestContents()
//! marks it. The project part is the same in every one of these manifests. The manifest is taken out of the manifest
//! map when the object goes. The constructor checks nothing, so that it always completes and the destructor always
//! runs; a case checks Published() first.
class PublishedBeaconList
{
public:
    PublishedBeaconList(CKey& key, const std::string& beacon_list, const int64_t time)
        : m_part(Gzip(beacon_list))
        , m_manifest(std::make_shared<CScraperManifest>())
    {
        CDataStream part(MakeUCharSpan(m_part), SER_NETWORK, 1);

        const std::string project_statistics = Gzip(PROJECT_STATISTICS);
        CDataStream project_part(MakeUCharSpan(project_statistics), SER_NETWORK, 1);

        const bool parts_added = m_manifest->addPartData(std::move(part), true) == 0
            && m_manifest->addPartData(std::move(project_part), true) == 1;

        {
            LOCK(m_manifest->cs_manifest);

            m_manifest->sCManifestName = EncodeDestination(CTxDestination(key.GetPubKey().GetID()));
            m_manifest->nTime = time;
            m_manifest->BeaconList = 0;
            m_manifest->BeaconList_c = 0;

            CScraperManifest::dentry project;

            project.project = "project";
            project.LastModified = static_cast<unsigned int>(time);
            project.part1 = 1;
            project.partc = 0;
            project.current = true;
            project.last = true;

            m_manifest->projects.push_back(project);
        }

        LOCK(CScraperManifest::cs_mapManifest);

        {
            LOCK(CSplitBlob::cs_mapParts);

            m_published = parts_added && CScraperManifest::addManifest(m_manifest, key);
        }

        if (!m_published) return;

        m_hash = WITH_LOCK(m_manifest->cs_manifest, return *m_manifest->phash);

        LOCK(cs_ConvergedScraperStatsCache);

        ConvergedScraperStatsCache.bClean = false;
    }

    ~PublishedBeaconList()
    {
        if (!m_published) return;

        LOCK(CScraperManifest::cs_mapManifest);
        CScraperManifest::DeleteManifest(m_hash, true);
    }

    PublishedBeaconList(const PublishedBeaconList&) = delete;
    PublishedBeaconList& operator=(const PublishedBeaconList&) = delete;

    bool Published() const { return m_published; }
    //! The hash the manifest map holds the manifest under.
    const uint256& Hash() const { return m_hash; }

    //! The message a peer is sent for the manifest.
    CDataStream Message() const
    {
        LOCK2(CScraperManifest::cs_mapManifest, CSplitBlob::cs_mapParts);

        return CScraperManifest::SerializeForSend(m_manifest);
    }

    //! The message a peer is sent for the manifest's beacon list part.
    CDataStream PartMessage() const
    {
        return CDataStream(MakeUCharSpan(m_part), SER_NETWORK, PROTOCOL_VERSION);
    }

private:
    const std::string m_part;
    std::shared_ptr<CScraperManifest> m_manifest;
    bool m_published{false};
    uint256 m_hash;
};

//! Takes a received manifest out of the manifest map when the object goes.
class ReceivedManifestRemoval
{
public:
    explicit ReceivedManifestRemoval(const uint256& hash) : m_hash(hash) {}

    ~ReceivedManifestRemoval()
    {
        LOCK(CScraperManifest::cs_mapManifest);
        CScraperManifest::DeleteManifest(m_hash, true);
    }

private:
    const uint256 m_hash;
};

//! Gives a case an empty cache of the converged statistics, and the time of the last block it found. Both are put
//! back when the object goes.
class ConvergedStatsCacheReset
{
public:
    ConvergedStatsCacheReset() : m_previous_block_time(g_previous_block_time.load())
    {
        LOCK(cs_ConvergedScraperStatsCache);

        m_cache = ConvergedScraperStatsCache;
        ConvergedScraperStatsCache = ConvergedScraperStats();
    }

    ~ConvergedStatsCacheReset()
    {
        g_previous_block_time.store(m_previous_block_time);

        LOCK(cs_ConvergedScraperStatsCache);

        ConvergedScraperStatsCache = m_cache;
    }

private:
    const int64_t m_previous_block_time;
    ConvergedScraperStats m_cache;
};

//! The count of scrapers in the convergence the cache holds.
size_t ScrapersInCachedConvergence()
{
    LOCK(cs_ConvergedScraperStatsCache);

    return ConvergedScraperStatsCache.Convergence.vIncludedScrapers.size();
}

//! Moves the clock on past the time for which the cache is used whatever has changed, with the node still in sync.
int64_t PastTheScraperSleep(const int64_t now)
{
    const int64_t later = now + WITH_LOCK(cs_ScraperGlobals, return nScraperSleep) / 1000 + 1;

    SetMockTime(later);
    g_previous_block_time.store(later);

    return later;
}

//! Counts the passes that set out to form a convergence from the manifests, and the passes that formed one. The
//! scraper event for a convergence is sent after the manifests were binned and before the result goes into the
//! cache, with no lock held. \p in_first_pass runs there in the first pass.
class ConvergencePasses
{
public:
    explicit ConvergencePasses(std::function<void()> in_first_pass = {})
        : m_in_first_pass(std::move(in_first_pass))
        , m_connection(uiInterface.NotifyScraperEvent_connect(
              [this](const scrapereventtypes& type, ChangeType status, const std::string&) {
                  if (type != scrapereventtypes::Convergence) return;

                  ++m_tried;

                  if (status == CT_NEW) ++m_formed;

                  if (m_tried == 1 && m_in_first_pass) m_in_first_pass();
              }))
    {
    }

    ConvergencePasses(const ConvergencePasses&) = delete;
    ConvergencePasses& operator=(const ConvergencePasses&) = delete;

    int Tried() const { return m_tried; }
    int Formed() const { return m_formed; }

private:
    const std::function<void()> m_in_first_pass;
    int m_tried{0};
    int m_formed{0};
    const boost::signals2::scoped_connection m_connection;
};

const std::string BEACON_LIST_HEADER = "CPID,time,key\n";

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(scraper_tests)

BOOST_AUTO_TEST_CASE(test_vector_is_an_ownership_proof_message)
{
    // The cases below depend on the shared signature covering exactly this account ID and beacon key.
    BOOST_CHECK_EQUAL(ToString(ACCOUNT_ID) + " " + BEACON_KEY_HEX, rsa_test_vectors::g_test_message);
}

BOOST_AUTO_TEST_CASE(it_verifies_a_proof_for_the_project_being_processed)
{
    const ScraperPendingBeaconMap pending {
        {"code_a", MakeCandidate(PROJECT_A, ACCOUNT_ID, BEACON_KEY_HEX, ValidSignature())},
    };
    const std::map<std::string, std::string> keys {{PROJECT_A, rsa_test_vectors::g_test_pem_pubkey}};

    unsigned int signature_failures = 0;
    const auto* verified = FindOwnershipProofVerifiedBeacon(Candidates(pending), PROJECT_A, ACCOUNT_ID, keys,
                                                            signature_failures);

    BOOST_REQUIRE(verified != nullptr);
    BOOST_CHECK_EQUAL(verified->first, "code_a");
    BOOST_CHECK_EQUAL(signature_failures, 0U);
}

BOOST_AUTO_TEST_CASE(it_matches_the_proof_url_to_the_project_being_processed)
{
    const ScraperPendingBeaconMap pending {
        {"code_a", MakeCandidate(PROJECT_A, ACCOUNT_ID, BEACON_KEY_HEX, ValidSignature())},
    };

    // The stats being processed are project B's.
    const std::map<std::string, std::string> keys {{PROJECT_A, rsa_test_vectors::g_test_pem_pubkey}};

    unsigned int signature_failures = 0;
    BOOST_CHECK(FindOwnershipProofVerifiedBeacon(Candidates(pending), PROJECT_B, ACCOUNT_ID, keys,
                                                 signature_failures) == nullptr);
    BOOST_CHECK_EQUAL(signature_failures, 0U);

    // The proof names project A, whichever keys are known.
    const std::map<std::string, std::string> both_keys {
        {PROJECT_A, rsa_test_vectors::g_test_pem_pubkey},
        {PROJECT_B, rsa_test_vectors::g_test_pem_pubkey},
    };

    BOOST_CHECK(FindOwnershipProofVerifiedBeacon(Candidates(pending), PROJECT_B, ACCOUNT_ID, both_keys,
                                                 signature_failures) == nullptr);
    BOOST_CHECK_EQUAL(signature_failures, 0U);
}

BOOST_AUTO_TEST_CASE(it_returns_the_candidate_whose_signature_verifies)
{
    // Two candidates for the same CPID and account ID; the test signature covers code_2's beacon key.
    const ScraperPendingBeaconMap pending {
        {"code_1", MakeCandidate(PROJECT_A, ACCOUNT_ID, OTHER_BEACON_KEY_HEX, ValidSignature())},
        {"code_2", MakeCandidate(PROJECT_A, ACCOUNT_ID, BEACON_KEY_HEX, ValidSignature())},
    };
    const std::map<std::string, std::string> keys {{PROJECT_A, rsa_test_vectors::g_test_pem_pubkey}};

    unsigned int signature_failures = 0;
    const auto* verified = FindOwnershipProofVerifiedBeacon(Candidates(pending), PROJECT_A, ACCOUNT_ID, keys,
                                                            signature_failures);

    BOOST_REQUIRE(verified != nullptr);
    BOOST_CHECK_EQUAL(verified->first, "code_2");
    BOOST_CHECK_EQUAL(verified->second.beacon_public_key_hex, BEACON_KEY_HEX);
    BOOST_CHECK_EQUAL(signature_failures, 1U);
}

BOOST_AUTO_TEST_CASE(it_matches_the_account_id_to_the_user_record)
{
    const ScraperPendingBeaconMap pending {
        {"code_a", MakeCandidate(PROJECT_A, ACCOUNT_ID, BEACON_KEY_HEX, ValidSignature())},
    };
    const std::map<std::string, std::string> keys {{PROJECT_A, rsa_test_vectors::g_test_pem_pubkey}};

    unsigned int signature_failures = 0;
    BOOST_CHECK(FindOwnershipProofVerifiedBeacon(Candidates(pending), PROJECT_A, ACCOUNT_ID + 1, keys,
                                                 signature_failures) == nullptr);
    BOOST_CHECK_EQUAL(signature_failures, 0U);
}

BOOST_AUTO_TEST_CASE(it_counts_signature_failures)
{
    std::vector<uint8_t> tampered = ValidSignature();
    tampered[0] ^= 0xFF;

    const ScraperPendingBeaconMap pending {
        {"code_a", MakeCandidate(PROJECT_A, ACCOUNT_ID, BEACON_KEY_HEX, tampered)},
    };
    const std::map<std::string, std::string> keys {{PROJECT_A, rsa_test_vectors::g_test_pem_pubkey}};

    unsigned int signature_failures = 0;
    BOOST_CHECK(FindOwnershipProofVerifiedBeacon(Candidates(pending), PROJECT_A, ACCOUNT_ID, keys,
                                                 signature_failures) == nullptr);
    BOOST_CHECK_EQUAL(signature_failures, 1U);
}

BOOST_AUTO_TEST_CASE(it_removes_duplicate_project_keys)
{
    const std::string PROJECT_C = "https://project-c.example.org/";

    // Projects A and B serve the same key, in different PEM text; project C serves its own.
    std::map<std::string, std::string> keys {
        {PROJECT_A, rsa_test_vectors::g_test_pem_pubkey},
        {PROJECT_B, rsa_test_vectors::g_test_pem_pubkey_rewrapped},
        {PROJECT_C, rsa_test_vectors::g_test_pem_pubkey_2},
    };

    const std::vector<std::string> removed = RemoveDuplicateProjectPublicKeys(keys);

    BOOST_CHECK(removed == std::vector<std::string>({PROJECT_A, PROJECT_B}));
    BOOST_CHECK_EQUAL(keys.size(), 1U);
    BOOST_CHECK(keys.count(PROJECT_C) == 1);
}

BOOST_AUTO_TEST_CASE(it_keeps_distinct_project_keys)
{
    std::map<std::string, std::string> keys {
        {PROJECT_A, rsa_test_vectors::g_test_pem_pubkey},
        {PROJECT_B, rsa_test_vectors::g_test_pem_pubkey_2},
    };

    BOOST_CHECK(RemoveDuplicateProjectPublicKeys(keys).empty());
    BOOST_CHECK_EQUAL(keys.size(), 2U);
}

BOOST_AUTO_TEST_CASE(convergence_by_project_skips_a_binned_manifest_that_is_gone)
{
    ScraperLogInDataDir log_in_data_dir;

    // A manifest binned for a scraper, then deleted by another thread before the convergence looks it up.
    const uint256 gone = uint256S("0101010101010101010101010101010101010101010101010101010101010101");
    {
        LOCK(CScraperManifest::cs_mapManifest);

        BOOST_REQUIRE(CScraperManifest::mapManifest.count(gone) == 0);
    }

    mmCSManifestsBinnedByScraper binned;
    binned["scraper_a"].insert(std::make_pair(int64_t {1234567}, std::make_pair(gone, uint256S("02"))));

    ConvergedManifest converged;

    BOOST_CHECK(!ScraperConstructConvergedManifestByProject(OneProjectWhitelist(), binned, converged));
}

BOOST_AUTO_TEST_CASE(convergence_by_project_fails_when_the_beacon_list_manifest_is_missing)
{
    ScraperLogInDataDir log_in_data_dir;

    // At a ratio of zero, a pass in which no project converges still meets it, and the BeaconList is then looked up
    // by the null hash, which no manifest has.
    ConvergenceByProjectRatio ratio(0.0);
    {
        LOCK(CScraperManifest::cs_mapManifest);

        BOOST_REQUIRE(CScraperManifest::mapManifest.count(uint256()) == 0);
    }

    mmCSManifestsBinnedByScraper binned;
    ConvergedManifest converged;

    BOOST_CHECK(!ScraperConstructConvergedManifestByProject(OneProjectWhitelist(), binned, converged));
}

//! A project's statistics part is read to its end or not used. A part whose
//! compressed data ends before its trailer gives every record and then a failed
//! read.
BOOST_AUTO_TEST_CASE(a_project_part_that_cannot_be_read_to_its_end_is_refused)
{
    const std::string records =
        "100,1700000000,10,00010203040506070809101112131415\n"
        "200,1700000000,20,15141312111009080706050403020100\n";

    std::string compressed;
    {
        std::istringstream in(records);
        std::ostringstream out;

        boost::iostreams::filtering_istream filter;
        filter.push(boost::iostreams::gzip_compressor());
        filter.push(in);
        boost::iostreams::copy(filter, out);

        compressed = out.str();
    }

    // The gzip trailer is the last eight bytes.
    BOOST_REQUIRE(compressed.size() > 8);

    const std::byte* const begin = reinterpret_cast<const std::byte*>(compressed.data());

    const SerializeData whole(begin, begin + compressed.size());
    const SerializeData cut_short(begin, begin + compressed.size() - 8);

    ScraperStats stats;

    BOOST_CHECK(LoadProjectObjectToStatsByCPID("project", whole, 0.0, stats));
    // One entry for each CPID and one for the project.
    BOOST_CHECK_EQUAL(stats.size(), 3U);

    stats.clear();

    BOOST_CHECK(!LoadProjectObjectToStatsByCPID("project", cut_short, 0.0, stats));
    BOOST_CHECK(stats.empty());
}

//! The scraper thread reaches its key while the wallet is unlocked for staking
//! only; the RPC that publishes a manifest on request does not, and needs a full
//! unlock.
BOOST_AUTO_TEST_CASE(scraper_rpc_path_refused_under_staking_only)
{
    ScraperLogInDataDir log_in_data_dir;

    // Swapped into pwalletMain, where the scraper looks for its key.
    grc_test::EncryptedWallet fx;

    // The wallet's key is an authorised scraper, and -scraperkey names it, so the
    // check goes to that key rather than walking the address book.
    grc_test::RegistryReset scrapers({GRC::ContractType::SCRAPER});
    {
        CMutableTransaction mtx;
        mtx.vContracts.push_back(GRC::MakeContract<GRC::ScraperEntryPayload>(
            uint32_t {3}, GRC::ContractAction::ADD, uint32_t {2}, fx.id, GRC::ScraperEntryStatus::AUTHORIZED));

        const CTransaction tx(mtx);
        CBlockIndex index {};

        LOCK(cs_main);
        GRC::GetScraperRegistry().Add({tx.vContracts[0], tx, &index});
    }

    const grc_test::ForcedArgGuard scraper_key("scraperkey", EncodeDestination(CTxDestination(fx.id)));

    const StakingKeyAccess scraper_access = StakingKeyAccessTestPeer::Make();

    fx.UnlockFor(UnlockScope::StakingOnly);

    {
        CTxDestination address;
        CKey key;
        BOOST_CHECK(!IsScraperAuthorizedToBroadcastManifests(address, key, nullptr));
        BOOST_CHECK(!key.IsValid());
    }

    {
        CTxDestination address;
        CKey key;
        BOOST_CHECK(IsScraperAuthorizedToBroadcastManifests(address, key, &scraper_access));
        BOOST_CHECK(key.IsValid() && key.GetPubKey() == fx.key.GetPubKey());
        BOOST_CHECK(address == CTxDestination(fx.id));
    }

    // A full unlock serves the general route as it always has.
    fx.UnlockFor(UnlockScope::Full);

    {
        CTxDestination address;
        CKey key;
        BOOST_CHECK(IsScraperAuthorizedToBroadcastManifests(address, key, nullptr));
        BOOST_CHECK(key.IsValid() && key.GetPubKey() == fx.key.GetPubKey());
    }
}

// savescraperfilemanifest names the files it writes from project names and
// ETags, and writes each under a plain name in the dump directory.
BOOST_AUTO_TEST_CASE(a_plain_file_name_part_keeps_the_characters_of_a_file_name)
{
    BOOST_CHECK_EQUAL(ToPlainFileNamePart("World Community Grid"), "World Community Grid");
    BOOST_CHECK_EQUAL(ToPlainFileNamePart("yoyo@home"), "yoyo@home");
    BOOST_CHECK_EQUAL(ToPlainFileNamePart("5f3a-61b2c.d_e"), "5f3a-61b2c.d_e");
    BOOST_CHECK_EQUAL(ToPlainFileNamePart(""), "");

    BOOST_CHECK_EQUAL(ToPlainFileNamePart("../project"), "..project");
    BOOST_CHECK_EQUAL(ToPlainFileNamePart("/project"), "project");
    BOOST_CHECK_EQUAL(ToPlainFileNamePart("project\\name"), "projectname");
    BOOST_CHECK_EQUAL(ToPlainFileNamePart("D:project"), "Dproject");
    BOOST_CHECK_EQUAL(ToPlainFileNamePart("project:stream"), "projectstream");
    BOOST_CHECK_EQUAL(ToPlainFileNamePart("a*b?c\"d<e>f|g"), "abcdefg");
    BOOST_CHECK_EQUAL(ToPlainFileNamePart(std::string("a\0b\tc\nd\x7f" "e", 9)), "abcde");
}

//! The convergence in the cache is used again while the manifests it was formed from are the manifests this node
//! holds complete. A manifest that is published after the manifests were binned for a convergence, and before that
//! convergence went into the cache, is not in it. So the next pass forms the convergence again, and the pass after
//! that, with nothing changed, uses the cache.
//!
//! In these cases no project is whitelisted: a convergence is formed from whole manifests when every whitelisted
//! project is in them, and the manifests here carry a beacon list only.
BOOST_AUTO_TEST_CASE(a_convergence_is_formed_again_when_a_manifest_was_published_while_it_was_formed)
{
    ScraperLogInDataDir log_in_data_dir;
    grc_test::StateGuard guard;
    const grc_test::RegistryReset no_projects{GRC::ContractType::PROJECT};
    ConvergedStatsCacheReset cache_reset;

    int64_t now = 1700000000;

    SetMockTime(now);
    g_previous_block_time.store(now);

    AuthorizedScrapers scrapers(2);

    const PublishedBeaconList first(scrapers.Key(0), BEACON_LIST_HEADER, now - 60);
    const PublishedBeaconList second(scrapers.Key(1), BEACON_LIST_HEADER, now - 30);

    BOOST_REQUIRE(first.Published());
    BOOST_REQUIRE(second.Published());

    // In the first pass, the first scraper publishes its next manifest.
    std::unique_ptr<PublishedBeaconList> published_meanwhile;

    const ConvergencePasses passes([&]() {
        published_meanwhile = std::make_unique<PublishedBeaconList>(
            scrapers.Key(0), BEACON_LIST_HEADER + "a cpid,1700000000,a key\n", now);
    });

    ScraperGetSuperblockContract();

    BOOST_REQUIRE_EQUAL(passes.Tried(), 1);
    BOOST_REQUIRE_EQUAL(passes.Formed(), 1);
    BOOST_REQUIRE(published_meanwhile && published_meanwhile->Published());
    BOOST_REQUIRE_EQUAL(ScrapersInCachedConvergence(), 2U);

    now = PastTheScraperSleep(now);
    ScraperGetSuperblockContract();

    BOOST_CHECK_EQUAL(passes.Tried(), 2);
    BOOST_CHECK_EQUAL(passes.Formed(), 2);
    BOOST_CHECK_EQUAL(ScrapersInCachedConvergence(), 2U);

    now = PastTheScraperSleep(now);
    ScraperGetSuperblockContract();

    BOOST_CHECK_EQUAL(passes.Tried(), 2);
}

//! A manifest that is deleted after the manifests were binned for a convergence, and before that convergence went
//! into the cache, may be in it. So the next pass forms the convergence again. The manifest is deleted by its hash and
//! then, in a second run, by its place in the manifest map.
BOOST_AUTO_TEST_CASE(a_convergence_is_formed_again_when_a_manifest_was_deleted_while_it_was_formed)
{
    for (const bool by_place : {false, true}) {
        ScraperLogInDataDir log_in_data_dir;
        grc_test::StateGuard guard;
        const grc_test::RegistryReset no_projects{GRC::ContractType::PROJECT};
        ConvergedStatsCacheReset cache_reset;

        int64_t now = 1700000000;

        SetMockTime(now);
        g_previous_block_time.store(now);

        AuthorizedScrapers scrapers(2);

        const PublishedBeaconList first(scrapers.Key(0), BEACON_LIST_HEADER, now - 60);
        const PublishedBeaconList second(scrapers.Key(1), BEACON_LIST_HEADER, now - 30);
        const PublishedBeaconList deleted_meanwhile(scrapers.Key(0), BEACON_LIST_HEADER + "a cpid,1700000000,a key\n",
                                                    now);

        BOOST_REQUIRE(first.Published());
        BOOST_REQUIRE(second.Published());
        BOOST_REQUIRE(deleted_meanwhile.Published());

        bool deleted = false;

        const ConvergencePasses passes([&]() {
            LOCK(CScraperManifest::cs_mapManifest);

            if (by_place) {
                auto place = CScraperManifest::mapManifest.find(deleted_meanwhile.Hash());

                if (place == CScraperManifest::mapManifest.end()) return;

                CScraperManifest::DeleteManifest(place, true);
                deleted = true;
            } else {
                deleted = CScraperManifest::DeleteManifest(deleted_meanwhile.Hash(), true);
            }
        });

        ScraperGetSuperblockContract();

        BOOST_REQUIRE_EQUAL(passes.Tried(), 1);
        BOOST_REQUIRE_EQUAL(passes.Formed(), 1);
        BOOST_REQUIRE(deleted);
        BOOST_REQUIRE_EQUAL(ScrapersInCachedConvergence(), 2U);

        now = PastTheScraperSleep(now);
        ScraperGetSuperblockContract();

        BOOST_CHECK_EQUAL(passes.Tried(), 2);
        BOOST_CHECK_EQUAL(passes.Formed(), 2);

        now = PastTheScraperSleep(now);
        ScraperGetSuperblockContract();

        BOOST_CHECK_EQUAL(passes.Tried(), 2);
    }
}

//! A pass deletes the manifests that are past their retention time before it bins the rest. Those deletions are
//! in the convergence the pass forms, so the next pass uses the cache.
BOOST_AUTO_TEST_CASE(a_convergence_is_used_again_when_its_own_pass_deleted_an_old_manifest)
{
    ScraperLogInDataDir log_in_data_dir;
    grc_test::StateGuard guard;
    const grc_test::RegistryReset no_projects{GRC::ContractType::PROJECT};
    ConvergedStatsCacheReset cache_reset;

    int64_t now = 1700000000;

    SetMockTime(now);
    g_previous_block_time.store(now);

    AuthorizedScrapers scrapers(2);

    const PublishedBeaconList first(scrapers.Key(0), BEACON_LIST_HEADER, now - 60);
    const PublishedBeaconList second(scrapers.Key(1), BEACON_LIST_HEADER, now - 30);
    const PublishedBeaconList old(scrapers.Key(0), BEACON_LIST_HEADER + "a cpid,1700000000,a key\n",
                                  now - WITH_LOCK(cs_ScraperGlobals, return SCRAPER_CMANIFEST_RETENTION_TIME) - 3600);

    BOOST_REQUIRE(first.Published());
    BOOST_REQUIRE(second.Published());
    BOOST_REQUIRE(old.Published());

    // A manifest deleted for its age is kept for a time as pending deletion. Takes it from there when the case ends,
    // as DeletePendingDeletedManifests() does.
    struct PendingRemoval
    {
        const uint256 hash;

        ~PendingRemoval()
        {
            LOCK(CScraperManifest::cs_mapManifest);

            const auto pending = CScraperManifest::mapPendingDeletedManifest.find(hash);

            if (pending == CScraperManifest::mapPendingDeletedManifest.end()) return;

            {
                const CScraperManifest_shared_ptr manifest = pending->second.second;

                LOCK(manifest->cs_manifest);

                manifest->phash = nullptr;
            }

            CScraperManifest::mapPendingDeletedManifest.erase(pending);
        }
    } pending_removal{old.Hash()};

    const ConvergencePasses passes;

    ScraperGetSuperblockContract();

    BOOST_REQUIRE_EQUAL(passes.Tried(), 1);
    BOOST_REQUIRE_EQUAL(passes.Formed(), 1);
    BOOST_REQUIRE_EQUAL(ScrapersInCachedConvergence(), 2U);
    BOOST_REQUIRE(WITH_LOCK(CScraperManifest::cs_mapManifest,
                            return CScraperManifest::mapManifest.count(old.Hash())) == 0);

    now = PastTheScraperSleep(now);
    ScraperGetSuperblockContract();

    BOOST_CHECK_EQUAL(passes.Tried(), 1);
}

namespace {

//! A manifest whose last part arrives after a convergence went into the cache is not in that convergence, so the next
//! pass forms the convergence again. A pass is a call of \p pass.
void FormedAgainWhenAManifestWasCompletedAfterIt(const std::function<void()>& pass)
{
    ScraperLogInDataDir log_in_data_dir;
    grc_test::StateGuard guard;
    const grc_test::RegistryReset no_projects{GRC::ContractType::PROJECT};
    ConvergedStatsCacheReset cache_reset;

    int64_t now = 1700000000;

    SetMockTime(now);
    g_previous_block_time.store(now);

    AuthorizedScrapers scrapers(2);

    // The first scraper's next manifest, as a peer sends it: the manifest first, and its beacon list part later. Its
    // project part is here before that, from the manifests published below.
    CDataStream manifest_message(SER_NETWORK, PROTOCOL_VERSION);
    CDataStream part_message(SER_NETWORK, PROTOCOL_VERSION);
    {
        const PublishedBeaconList elsewhere(scrapers.Key(0), BEACON_LIST_HEADER + "a cpid,1700000000,a key\n", now);

        BOOST_REQUIRE(elsewhere.Published());

        manifest_message = elsewhere.Message();
        part_message = elsewhere.PartMessage();
    }

    const uint256 received_hash = Hash(manifest_message);
    const ReceivedManifestRemoval removal(received_hash);

    BOOST_REQUIRE(CScraperManifest::RecvManifest(nullptr, manifest_message));

    const auto received_is_complete = [&received_hash]() {
        LOCK(CScraperManifest::cs_mapManifest);

        const auto received = CScraperManifest::mapManifest.find(received_hash);

        BOOST_REQUIRE(received != CScraperManifest::mapManifest.end());

        return WITH_LOCK(received->second->cs_manifest, return received->second->isComplete());
    };

    BOOST_REQUIRE(!received_is_complete());

    const PublishedBeaconList first(scrapers.Key(0), BEACON_LIST_HEADER, now - 60);
    const PublishedBeaconList second(scrapers.Key(1), BEACON_LIST_HEADER, now - 30);

    BOOST_REQUIRE(first.Published());
    BOOST_REQUIRE(second.Published());

    const ConvergencePasses passes;

    pass();

    BOOST_REQUIRE_EQUAL(passes.Tried(), 1);
    BOOST_REQUIRE_EQUAL(passes.Formed(), 1);
    BOOST_REQUIRE_EQUAL(ScrapersInCachedConvergence(), 2U);

    // With nothing changed, the cache is used.
    now = PastTheScraperSleep(now);
    pass();

    BOOST_REQUIRE_EQUAL(passes.Tried(), 1);

    BOOST_REQUIRE(CSplitBlob::RecvPart(nullptr, part_message));
    BOOST_REQUIRE(received_is_complete());

    now = PastTheScraperSleep(now);
    pass();

    BOOST_CHECK_EQUAL(passes.Tried(), 2);
    BOOST_CHECK_EQUAL(passes.Formed(), 2);
}

} // anonymous namespace

BOOST_AUTO_TEST_CASE(a_convergence_is_formed_again_when_a_manifest_was_completed_after_it)
{
    FormedAgainWhenAManifestWasCompletedAfterIt([]() { ScraperGetSuperblockContract(); });
}

//! convergencereport makes the same test of the cache before it reports from it.
BOOST_AUTO_TEST_CASE(convergencereport_forms_the_convergence_again_when_a_manifest_was_completed_after_it)
{
    FormedAgainWhenAManifestWasCompletedAfterIt([]() { convergencereport(UniValue(UniValue::VARR)); });
}

BOOST_AUTO_TEST_SUITE_END()
