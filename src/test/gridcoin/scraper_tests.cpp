// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/project.h"
#include "gridcoin/quorum.h"
#include "gridcoin/scraper/scraper.h"
#include "gridcoin/scraper/scraper_net.h"
#include "gridcoin/scraper/scraper_registry.h"
#include "gridcoin/superblock.h"
#include "hash.h"
#include "key.h"
#include "key_io.h"
#include "logging.h"
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

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// Defined in scraper.cpp, which declares them there rather than in a header.
extern fs::path pathDataDir;
extern ConvergedScraperStats ConvergedScraperStatsCache GUARDED_BY(cs_ConvergedScraperStatsCache);
extern bool ProcessNetworkWideFromProjectStats(ScraperStats& mScraperStats);
extern mmCSManifestsBinnedByScraper ScraperCullAndBinCScraperManifests();
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

const std::string STATS_PROJECT = "project_1";
const double STATS_PROJECT_MAG = 1000.0;

//! A project part as a scraper publishes it: gzip of TC,RAT,RAC,cpid records.
SerializeData MakeProjectPart(const std::string& records)
{
    std::stringstream compressed;

    {
        boost::iostreams::filtering_ostream out;
        out.push(boost::iostreams::gzip_compressor());
        out.push(compressed);
        out << records;
    }

    const std::string bytes = compressed.str();
    SerializeData part(bytes.size());
    std::memcpy(part.data(), bytes.data(), bytes.size());

    return part;
}

ScraperStats LoadProjectPart(const std::string& records, const bool checked_values)
{
    ScraperStats stats;

    BOOST_REQUIRE(LoadProjectObjectToStatsByCPID(
        STATS_PROJECT, MakeProjectPart(records), STATS_PROJECT_MAG, stats, checked_values));

    return stats;
}

const ScraperObjectStats* FindStats(const ScraperStats& stats, const statsobjecttype type, const std::string& id)
{
    ScraperObjectStatsKey key;
    key.objecttype = type;
    key.objectID = id;

    const auto iter = stats.find(key);

    return iter == stats.end() ? nullptr : &iter->second;
}

const ScraperObjectStats* FindRecord(const ScraperStats& stats, const std::string& cpid)
{
    return FindStats(stats, statsobjecttype::byCPIDbyProject, STATS_PROJECT + "," + cpid);
}

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

//! The projects of a manifest in their order, each a project name and the statistics published under it.
using ProjectStatistics = std::vector<std::pair<std::string, std::string>>;

//! A manifest published as a scraper publishes one, with a beacon list, then the statistics of each project under
//! its project name, and a consensus block. The manifest is taken out of the manifest map when the object goes.
class PublishedStats
{
public:
    PublishedStats(CKey& key, const ProjectStatistics& projects, const int64_t time,
                   const uint256& consensus_block = uint256())
        : m_manifest(std::make_shared<CScraperManifest>())
    {
        bool parts_added = true;
        std::vector<std::string> texts{Gzip(BEACON_LIST_HEADER)};

        for (const auto& project : projects) {
            texts.push_back(Gzip(project.second));
        }

        for (const std::string& text : texts) {
            CDataStream part(MakeUCharSpan(text), SER_NETWORK, 1);

            if (m_manifest->addPartData(std::move(part), true) < 0) parts_added = false;
        }

        {
            LOCK(m_manifest->cs_manifest);

            m_manifest->sCManifestName = EncodeDestination(CTxDestination(key.GetPubKey().GetID()));
            m_manifest->nTime = time;
            m_manifest->ConsensusBlock = consensus_block;
            m_manifest->BeaconList = 0;
            m_manifest->BeaconList_c = 0;

            for (size_t n = 0; n < projects.size(); ++n) {
                CScraperManifest::dentry project;
                project.project = projects[n].first;
                project.part1 = static_cast<int>(n + 1);
                project.LastModified = time;
                project.current = true;
                project.last = true;
                m_manifest->projects.push_back(project);
            }
        }

        LOCK2(CScraperManifest::cs_mapManifest, CSplitBlob::cs_mapParts);

        m_published = parts_added && CScraperManifest::addManifest(m_manifest, key);

        if (m_published) m_hash = WITH_LOCK(m_manifest->cs_manifest, return *m_manifest->phash);
    }

    ~PublishedStats()
    {
        if (!m_published) return;

        LOCK(CScraperManifest::cs_mapManifest);
        CScraperManifest::DeleteManifest(m_hash, true);
    }

    PublishedStats(const PublishedStats&) = delete;
    PublishedStats& operator=(const PublishedStats&) = delete;

    bool Published() const { return m_published; }
    std::shared_ptr<CScraperManifest>& Manifest() { return m_manifest; }
    uint256 ContentHash() const { return WITH_LOCK(m_manifest->cs_manifest, return m_manifest->nContentHash); }

private:
    std::shared_ptr<CScraperManifest> m_manifest;
    bool m_published{false};
    uint256 m_hash;
};

//! The message a peer is sent for \p manifest, with \p stated in the place of the hash of its contents, and the
//! signature of the scraper's key over that.
CDataStream MessageStating(const std::shared_ptr<CScraperManifest>& manifest, const uint256& stated, CKey& key)
{
    CDataStream message(SER_NETWORK, 1);

    LOCK2(CSplitBlob::cs_mapParts, manifest->cs_manifest);

    const uint256 computed = manifest->nContentHash;

    manifest->nContentHash = stated;
    manifest->SerializeWithoutSignature(message);
    manifest->nContentHash = computed;

    std::vector<unsigned char> signature;

    BOOST_REQUIRE(key.Sign(Hash(message), signature));

    message << signature;

    return message;
}

//! The hash of a manifest.
uint256 ManifestHash(const std::shared_ptr<CScraperManifest>& manifest)
{
    LOCK(manifest->cs_manifest);

    return *manifest->phash;
}

//! The content hash a manifest states.
uint256 StatedContentHash(const std::shared_ptr<CScraperManifest>& manifest)
{
    return WITH_LOCK(manifest->cs_manifest, return manifest->nContentHash);
}

//! The height v15 is set to in a case of the validation by manifest.
const int V15_HEIGHT = 100;

//! The statistics of the project in a case of the validation by manifest, and other statistics of it.
const std::string AGREED_STATISTICS =
    "100,1700000000,10,00010203040506070809101112131415\n"
    "200,1700000000,20,15141312111009080706050403020100\n";
const std::string OTHER_STATISTICS =
    "100,1700000000,1000000,00010203040506070809101112131415\n"
    "200,1700000000,20,15141312111009080706050403020100\n";

//! What a node did with a superblock it was given.
struct GivenSuperblockOutcome
{
    bool accepted;       //!< Whether ValidateSuperblock() accepted the superblock.
    bool own_formed;     //!< Whether the node made a superblock of its own.
    bool own_by_project; //!< Whether the node's own superblock was converged by project.
    size_t own_scrapers; //!< The count of scrapers in the node's own convergence.
};

//! The convergence a node formed of its own.
struct OwnConvergence
{
    bool formed;                //!< Whether the node made a superblock of its own.
    bool by_project;            //!< Whether the node's own superblock was converged by project.
    size_t scrapers;            //!< The count of scrapers in the convergence.
    GRC::Superblock superblock; //!< The node's own superblock.
};

//! Keeps the result that Quorum::ValidateSuperblock() logs, for the life of the object.
class LoggedValidationResult
{
public:
    LoggedValidationResult()
        : m_callback(LogInstance().PushBackCallback([this](const std::string& line) {
              for (const char* const result : {"UNKNOWN", "INVALID", "HISTORICAL", "VALID_CURRENT", "VALID_PAST",
                                               "VALID_BY_MANIFEST", "VALID_BY_PROJECT"}) {
                  if (line.find(std::string("ValidateSuperblock(): ") + result + " - ") != std::string::npos) {
                      m_result = result;
                  }
              }
          }))
    {
    }

    ~LoggedValidationResult() { LogInstance().DeleteCallback(m_callback); }

    LoggedValidationResult(const LoggedValidationResult&) = delete;
    LoggedValidationResult& operator=(const LoggedValidationResult&) = delete;

    const std::string& Result() const { return m_result; }

private:
    std::string m_result;
    const std::list<std::function<void(const std::string&)>>::iterator m_callback;
};

//! Authorized scrapers and a node for a case of the validation by manifest. The scrapers are numbered in the order
//! of their IDs, which is the order the manifests are counted in. The node is in sync and past its first
//! housekeeping, its cache of the converged statistics is empty, project_a is on the whitelist, and v15 is at
//! V15_HEIGHT. All of it is put back when the object goes.
class ScrapersAndNode
{
public:
    explicit ScrapersAndNode(const size_t count) : m_scrapers(count), m_by_id(count)
    {
        std::iota(m_by_id.begin(), m_by_id.end(), size_t {0});

        SetMockTime(NOW);
        g_previous_block_time.store(NOW);

        // The node is past its first housekeeping.
        WITH_LOCK(cs_ConvergedScraperStatsCache, ConvergedScraperStatsCache.bMinHousekeepingComplete = true);

        const auto id = [this](const size_t n) {
            return EncodeDestination(CTxDestination(m_scrapers.Key(n).GetPubKey().GetID()));
        };

        std::sort(m_by_id.begin(), m_by_id.end(), [&id](const size_t a, const size_t b) { return id(a) < id(b); });

        CMutableTransaction mtx;

        mtx.vContracts.push_back(GRC::MakeContract<GRC::Project>(
            uint32_t {3}, GRC::ContractAction::ADD, uint32_t {4}, std::string {"project_a"}, PROJECT_A, false, false,
            GRC::ProjectEntryStatus::UNKNOWN));
        m_project_tx = CTransaction(mtx);

        LOCK(cs_main);
        GRC::GetWhitelist().Add({m_project_tx.vContracts[0], m_project_tx, &m_project_index});
    }

    ScrapersAndNode(const ScrapersAndNode&) = delete;
    ScrapersAndNode& operator=(const ScrapersAndNode&) = delete;

    //! Scraper \p n publishes a manifest with the statistics of each project under its project name, and with the
    //! consensus block.
    std::shared_ptr<CScraperManifest> Publishes(const size_t n, const ProjectStatistics& projects,
                                                const uint256& consensus_block = uint256())
    {
        m_published.push_back(
            std::make_unique<PublishedStats>(Key(n), projects, NextManifestTime(), consensus_block));

        BOOST_REQUIRE(m_published.back()->Published());

        return m_published.back()->Manifest();
    }

    //! Scraper \p n publishes a manifest with the statistics under the project name.
    std::shared_ptr<CScraperManifest> Publishes(const size_t n, const std::string& statistics,
                                                const std::string& project_name = "project_a")
    {
        return Publishes(n, ProjectStatistics{{project_name, statistics}});
    }

    //! A peer sends a manifest of scraper \p n with the statistics under the project name, as below.
    std::shared_ptr<CScraperManifest> IsSent(const size_t n, const std::string& statistics,
                                             const std::string& project_name = "project_a",
                                             const std::optional<uint256>& stated = std::nullopt)
    {
        return IsSent(n, ProjectStatistics{{project_name, statistics}}, stated);
    }

    //! A peer sends a manifest of scraper \p n with the statistics of each project under its project name, and then
    //! each statistics part that no manifest here has. The manifest states \p stated as the hash of its contents, or
    //! the hash of its contents when none is given.
    std::shared_ptr<CScraperManifest> IsSent(const size_t n, const ProjectStatistics& projects,
                                             const std::optional<uint256>& stated = std::nullopt)
    {
        // The scraper made the manifest elsewhere.
        CDataStream manifest_message(SER_NETWORK, 1);
        uint256 states;
        {
            PublishedStats elsewhere(Key(n), projects, NextManifestTime());

            BOOST_REQUIRE(elsewhere.Published());

            states = stated ? *stated : elsewhere.ContentHash();
            manifest_message = MessageStating(elsewhere.Manifest(), states, Key(n));
        }

        const uint256 hash = Hash(manifest_message);

        m_received.push_back(std::make_unique<ReceivedManifestRemoval>(hash));

        BOOST_REQUIRE(CScraperManifest::RecvManifest(nullptr, manifest_message));

        std::shared_ptr<CScraperManifest> received;
        {
            LOCK(CScraperManifest::cs_mapManifest);

            const auto found = CScraperManifest::mapManifest.find(hash);

            BOOST_REQUIRE(found != CScraperManifest::mapManifest.end());

            received = found->second;
        }

        for (const auto& project : projects) {
            const std::string statistics_part = Gzip(project.second);
            CDataStream part_message(MakeUCharSpan(statistics_part), SER_NETWORK, PROTOCOL_VERSION);
            const uint256 part_hash = Hash(part_message);
            bool held;
            {
                LOCK(CSplitBlob::cs_mapParts);

                const auto part = CSplitBlob::mapParts.find(part_hash);

                BOOST_REQUIRE(part != CSplitBlob::mapParts.end());

                held = part->second.present();
            }

            if (!held) BOOST_REQUIRE(CSplitBlob::RecvPart(nullptr, part_message));
        }

        LOCK(received->cs_manifest);

        BOOST_REQUIRE(received->isComplete());
        BOOST_REQUIRE(received->nContentHash == states);

        return received;
    }

    //! The superblock made from \p manifest alone.
    GRC::Superblock SuperblockOf(std::shared_ptr<CScraperManifest> manifest)
    {
        ConvergedManifest of_one(manifest);
        const ConvergedScraperStats stats(NOW, of_one,
                                          GRC::GetWhitelist().Snapshot(
                                              GRC::GreylistState::PENDING,
                                              GRC::ProjectEntry::ProjectFilterFlag::GREYLISTED),
                                          false);
        GRC::Superblock superblock = GRC::Superblock::FromConvergence(stats, 2, false, nullptr);

        BOOST_REQUIRE(superblock.WellFormed());
        BOOST_REQUIRE(!superblock.ConvergedByProject());

        return superblock;
    }

    //! The best block is the block below \p height, and the node forms its own convergence, which is then for a
    //! superblock in a block at \p height.
    OwnConvergence FormsItsOwnConvergenceFor(const int height)
    {
        WITH_LOCK(cs_main, nBestHeight = height - 1);

        ScraperGetSuperblockContract();

        OwnConvergence own{};

        own.scrapers = ScrapersInCachedConvergence();

        LOCK(cs_ConvergedScraperStatsCache);

        own.superblock = ConvergedScraperStatsCache.NewFormatSuperblock;
        own.formed = own.superblock.WellFormed();
        own.by_project = own.superblock.ConvergedByProject();

        return own;
    }

    //! Gives the node \p superblock in a block at \p height, not from the convergences in its cache, and tells the
    //! result of the validation that the node logs.
    std::string ResultNotFromTheCacheFor(GRC::Superblock superblock, const int height)
    {
        CBlockIndex index;
        index.nTime = NOW;
        index.nHeight = height;

        const GRC::SuperblockPtr incoming = GRC::SuperblockPtr::BindShared(std::move(superblock), &index);

        LoggedValidationResult logged;

        GRC::Quorum::ValidateSuperblock(incoming, false, 32, height);

        return logged.Result();
    }

    //! Gives the node the superblock made from \p manifest alone, in a block at \p height or with no height. With
    //! \p use_cache false the node does not look for the superblock among the convergences in its cache. With
    //! \p another_hint the superblock has another hint for the manifest's content hash, which is not in the hash of
    //! the superblock.
    GivenSuperblockOutcome IsGivenTheSuperblockOf(std::shared_ptr<CScraperManifest> manifest,
                                                  const std::optional<int> height, const bool use_cache = true,
                                                  const bool another_hint = false)
    {
        GRC::Superblock superblock = SuperblockOf(manifest);

        if (another_hint) superblock.m_manifest_content_hint ^= 1;

        CBlockIndex index;
        index.nTime = NOW;
        index.nHeight = height.value_or(V15_HEIGHT);

        const GRC::SuperblockPtr incoming = GRC::SuperblockPtr::BindShared(std::move(superblock), &index);

        GivenSuperblockOutcome outcome{};
        std::string result;
        {
            LoggedValidationResult logged;

            outcome.accepted = height
                ? GRC::Quorum::ValidateSuperblock(incoming, use_cache, 32, *height)
                : GRC::Quorum::ValidateSuperblock(incoming, use_cache);
            result = logged.Result();
        }

        // In these cases a superblock is valid by manifest or it is not valid. Any other result fails the case.
        BOOST_REQUIRE_MESSAGE(result == (outcome.accepted ? "VALID_BY_MANIFEST" : "INVALID"),
                              "the result of the validation is " << result);

        outcome.own_scrapers = ScrapersInCachedConvergence();

        LOCK(cs_ConvergedScraperStatsCache);

        outcome.own_formed = ConvergedScraperStatsCache.NewFormatSuperblock.WellFormed();
        outcome.own_by_project = ConvergedScraperStatsCache.NewFormatSuperblock.ConvergedByProject();

        return outcome;
    }

    //! The ID of scraper \p n.
    ScraperID Id(const size_t n) { return EncodeDestination(CTxDestination(Key(n).GetPubKey().GetID())); }

private:
    static constexpr int64_t NOW = 1700000000;

    CKey& Key(const size_t n) { return m_scrapers.Key(m_by_id[n]); }

    //! Each manifest of a case has a time of its own.
    int64_t NextManifestTime() { return NOW - 600 + 10 * m_manifests++; }

    ScraperLogInDataDir m_log_in_data_dir;
    grc_test::StateGuard m_guard;
    grc_test::RegistryReset m_projects{GRC::ContractType::PROJECT};
    ConvergedStatsCacheReset m_cache_reset;
    grc_test::V15HeightGuard m_v15_height{V15_HEIGHT};
    AuthorizedScrapers m_scrapers;
    std::vector<size_t> m_by_id;
    CTransaction m_project_tx;
    CBlockIndex m_project_index{};
    int m_manifests{0};
    std::vector<std::unique_ptr<PublishedStats>> m_published;
    std::vector<std::unique_ptr<ReceivedManifestRemoval>> m_received;
};

//! Three scrapers, of which the supermajority is two.
class ThreeScrapers : public ScrapersAndNode
{
public:
    ThreeScrapers() : ScrapersAndNode(3) {}
};

//! Five scrapers, of which the supermajority is three.
class FiveScrapers : public ScrapersAndNode
{
public:
    FiveScrapers() : ScrapersAndNode(5) {}
};

//! The hint a superblock has for a content hash.
uint32_t HintOf(const uint256& content_hash)
{
    return content_hash.GetUint64(0) >> 32;
}

//! The hash of a part with the data.
uint256 PartHash(const std::string& data)
{
    return Hash(CDataStream(MakeUCharSpan(data), SER_NETWORK, 1));
}

//! The content hash of a manifest with parts of the hashes, in their order, and the consensus block. A case that
//! uses it requires the manifest it publishes to state this hash.
uint256 ContentHashOf(const std::vector<uint256>& part_hashes, const uint256& consensus_block)
{
    CDataStream ss(SER_NETWORK, 1);

    WriteCompactSize(ss, part_hashes.size());

    for (const uint256& part_hash : part_hashes) {
        ss << part_hash;
    }

    ss << consensus_block;

    return Hash(ss);
}

//! The first two numbers below \p count, counting from 0, for which \p content_hash_of gives content hashes with the
//! same hint. A hint is 32 bits of the hash, so two are found in some tens of thousands of numbers.
std::pair<uint64_t, uint64_t> TwoWithTheSameHint(const uint64_t count,
                                                 const std::function<uint256(uint64_t)>& content_hash_of)
{
    std::unordered_map<uint32_t, uint64_t> numbers_by_hint;

    for (uint64_t n = 0; n < count; ++n) {
        const auto inserted = numbers_by_hint.emplace(HintOf(content_hash_of(n)), n);

        if (!inserted.second) return {inserted.first->second, n};
    }

    BOOST_FAIL("no two content hashes with the same hint");

    return {0, 0};
}

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

    BOOST_CHECK(LoadProjectObjectToStatsByCPID("project", whole, 0.0, stats, false));
    // One entry for each CPID and one for the project.
    BOOST_CHECK_EQUAL(stats.size(), 3U);

    stats.clear();

    BOOST_CHECK(!LoadProjectObjectToStatsByCPID("project", cut_short, 0.0, stats, false));
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

//! Below v15 and from it, a superblock rebuilt from the manifest that two of three scrapers hold is valid by manifest,
//! wherever the scraper with other statistics is in the order. The hint in the superblock selects the manifests that
//! are counted, so the same superblock with another hint is not.
BOOST_AUTO_TEST_CASE(validation_by_manifest_accepts_the_manifest_of_two_of_three_scrapers)
{
    for (const int height : {V15_HEIGHT - 1, V15_HEIGHT}) {
        for (size_t place = 0; place < 3; ++place) {
            ThreeScrapers scrapers;

            const std::shared_ptr<CScraperManifest> agreed = scrapers.Publishes((place + 1) % 3, AGREED_STATISTICS);

            scrapers.Publishes((place + 2) % 3, AGREED_STATISTICS);
            scrapers.Publishes(place, OTHER_STATISTICS);

            // Not from the cache, where it is the node's own superblock.
            BOOST_CHECK_MESSAGE(scrapers.IsGivenTheSuperblockOf(agreed, height, false).accepted,
                                "height " << height << " place " << place);
            BOOST_CHECK_MESSAGE(!scrapers.IsGivenTheSuperblockOf(agreed, height, false, true).accepted,
                                "height " << height << " place " << place);
        }
    }
}

//! Manifests with other contents are counted apart, below v15 and from it, when the hint in the superblock is the
//! hint of each of them. They are manifests with other statistics and the same consensus block, and manifests with
//! the same statistics and another consensus block.
BOOST_AUTO_TEST_CASE(validation_by_manifest_counts_manifests_with_other_contents_and_the_same_hint_apart)
{
    const auto block = [](const uint64_t n) { return (CHashWriter(SER_GETHASH, 0) << n).GetHash(); };
    const auto statistics = [](const uint64_t n) {
        return "100,1700000000," + ToString(2000000 + n) + ",00010203040506070809101112131415\n"
               "200,1700000000,20,15141312111009080706050403020100\n";
    };

    const uint256 beacon_list_hash = PartHash(Gzip(BEACON_LIST_HEADER));
    const uint256 agreed_hash = PartHash(Gzip(AGREED_STATISTICS));

    // A manifest of two projects has two of a thousand statistics, so a million manifests with other statistics
    // take a thousand statistics parts.
    const uint64_t thousand = 1000;
    std::vector<uint256> statistics_hashes;

    for (uint64_t n = 0; n < thousand; ++n) {
        statistics_hashes.push_back(PartHash(Gzip(statistics(n))));
    }

    const auto two_statistics = [&statistics](const uint64_t n) {
        return ProjectStatistics{{"project_a", statistics(n / thousand)}, {"project_b", statistics(n % thousand)}};
    };

    const std::pair<uint64_t, uint64_t> of_statistics = TwoWithTheSameHint(thousand * thousand, [&](const uint64_t n) {
        return ContentHashOf({beacon_list_hash, statistics_hashes[n / thousand], statistics_hashes[n % thousand]},
                             block(0));
    });
    const std::pair<uint64_t, uint64_t> of_blocks = TwoWithTheSameHint(4000000, [&](const uint64_t n) {
        return ContentHashOf({beacon_list_hash, agreed_hash}, block(n));
    });

    // The projects and the consensus block of a manifest.
    using Contents = std::pair<ProjectStatistics, uint256>;

    const ProjectStatistics agreed{{"project_a", AGREED_STATISTICS}};
    const std::vector<std::pair<Contents, Contents>> pairs{
        {{two_statistics(of_statistics.first), block(0)}, {two_statistics(of_statistics.second), block(0)}},
        {{agreed, block(of_blocks.first)}, {agreed, block(of_blocks.second)}},
    };

    // The content hash of a manifest with the contents.
    const auto content_hash_of = [&beacon_list_hash](const Contents& contents) {
        std::vector<uint256> part_hashes{beacon_list_hash};

        for (const auto& project : contents.first) {
            part_hashes.push_back(PartHash(Gzip(project.second)));
        }

        return ContentHashOf(part_hashes, contents.second);
    };

    for (size_t which = 0; which < pairs.size(); ++which) {
        const Contents& first = pairs[which].first;
        const Contents& second = pairs[which].second;

        for (const int height : {V15_HEIGHT - 1, V15_HEIGHT}) {
            // The first scraper has a manifest of the first contents and the second scraper has one of the second.
            // The third scraper has one of the second contents or none. Then each scraper publishes the same
            // manifest of other statistics, which the node's own convergence is of.
            for (const bool third_has_the_second : {false, true}) {
                ThreeScrapers scrapers;

                const std::shared_ptr<CScraperManifest> of_first = scrapers.Publishes(0, first.first, first.second);
                const std::shared_ptr<CScraperManifest> of_second =
                    scrapers.Publishes(1, second.first, second.second);

                if (third_has_the_second) {
                    scrapers.Publishes(2, second.first, second.second);
                }

                BOOST_REQUIRE(StatedContentHash(of_first) == content_hash_of(first));
                BOOST_REQUIRE(StatedContentHash(of_second) == content_hash_of(second));
                BOOST_REQUIRE(StatedContentHash(of_first) != StatedContentHash(of_second));
                BOOST_REQUIRE_EQUAL(HintOf(StatedContentHash(of_first)), HintOf(StatedContentHash(of_second)));

                for (size_t n = 0; n < 3; ++n) {
                    const std::shared_ptr<CScraperManifest> of_all = scrapers.Publishes(n, OTHER_STATISTICS);

                    BOOST_REQUIRE(HintOf(StatedContentHash(of_all)) != HintOf(StatedContentHash(of_second)));
                }

                // The node is given the superblock made from the second manifest, not from its cache. It is valid by
                // manifest when the third scraper holds a manifest of the second contents.
                BOOST_CHECK_MESSAGE(
                    scrapers.IsGivenTheSuperblockOf(of_second, height, false).accepted == third_has_the_second,
                    "pair " << which << " height " << height << " third has the second " << third_has_the_second);
            }
        }
    }
}

//! The supermajority is of the scrapers that hold manifests. With five scrapers of which three hold manifests it is
//! two, below v15 and from it.
BOOST_AUTO_TEST_CASE(validation_by_manifest_takes_the_supermajority_of_the_scrapers_that_hold_manifests)
{
    for (const int height : {V15_HEIGHT - 1, V15_HEIGHT}) {
        FiveScrapers scrapers;

        scrapers.Publishes(0, OTHER_STATISTICS);

        const std::shared_ptr<CScraperManifest> of_two = scrapers.Publishes(1, AGREED_STATISTICS);

        scrapers.Publishes(2, AGREED_STATISTICS);

        // Not from the cache, where it is the node's own superblock.
        BOOST_CHECK_MESSAGE(scrapers.IsGivenTheSuperblockOf(of_two, height, false).accepted, "height " << height);
    }
}

//! When the superblock is not the one rebuilt from the manifest at which a count reaches the supermajority, the
//! count goes on with the manifests after it, below v15 and from it.
BOOST_AUTO_TEST_CASE(validation_by_manifest_goes_on_after_a_manifest_that_does_not_give_the_superblock)
{
    for (const int height : {V15_HEIGHT - 1, V15_HEIGHT}) {
        ThreeScrapers scrapers;

        // Two scrapers each have a manifest with the statistics under project_b and a later one with the same parts
        // under project_a. A scraper's manifests are counted from the latest, so from v15 the count of the
        // manifests under project_a reaches the supermajority first.
        const std::shared_ptr<CScraperManifest> earlier = scrapers.Publishes(0, AGREED_STATISTICS, "project_b");

        scrapers.Publishes(1, AGREED_STATISTICS, "project_b");

        const std::shared_ptr<CScraperManifest> later = scrapers.Publishes(0, AGREED_STATISTICS, "project_a");

        scrapers.Publishes(1, AGREED_STATISTICS, "project_a");
        scrapers.Publishes(2, OTHER_STATISTICS);

        BOOST_REQUIRE(StatedContentHash(later) == StatedContentHash(earlier));

        // The node is given the superblock made from a manifest under project_b, not from its cache.
        BOOST_CHECK_MESSAGE(scrapers.IsGivenTheSuperblockOf(earlier, height, false).accepted, "height " << height);
    }
}

//! From v15 ManifestsAgreedBySupermajority() gives the manifests that a supermajority of the scrapers agree on, to the
//! construction of a convergence and to the validation of a superblock by manifest. The agreements are given latest
//! first, an agreement holds the latest of the agreeing manifests of each of its scrapers, and manifests of fewer
//! scrapers than the supermajority are no agreement.
BOOST_AUTO_TEST_CASE(manifests_agreed_by_supermajority_are_given_latest_first_with_one_manifest_of_each_scraper)
{
    ThreeScrapers scrapers;

    const std::shared_ptr<CScraperManifest> earlier = scrapers.Publishes(0, OTHER_STATISTICS);

    scrapers.Publishes(1, OTHER_STATISTICS);

    const uint256 earlier_of_2 = ManifestHash(scrapers.Publishes(2, OTHER_STATISTICS));

    scrapers.Publishes(0, AGREED_STATISTICS);

    const uint256 later_of_0 = ManifestHash(scrapers.Publishes(0, AGREED_STATISTICS));
    const std::shared_ptr<CScraperManifest> later = scrapers.Publishes(1, AGREED_STATISTICS);

    // The manifest of one scraper of the three.
    scrapers.Publishes(2, AGREED_STATISTICS, "project_b");

    mmCSManifestsBinnedByScraper manifests_by_scraper = ScraperCullAndBinCScraperManifests();

    BOOST_REQUIRE(manifests_by_scraper.size() == 3);

    // A manifest that was deleted after the manifests were binned.
    manifests_by_scraper[scrapers.Id(2)].emplace(
        std::numeric_limits<int64_t>::max(),
        std::make_pair(uint256S("01"), StatedContentHash(later)));

    const std::vector<ManifestAgreement> agreements = ManifestsAgreedBySupermajority(manifests_by_scraper);

    BOOST_REQUIRE(agreements.size() == 2);

    BOOST_CHECK(agreements[0].m_content_hash == StatedContentHash(later));
    BOOST_REQUIRE(agreements[0].m_manifests.size() == 2);
    BOOST_CHECK(agreements[0].m_manifests.at(scrapers.Id(0)) == later_of_0);
    BOOST_CHECK(agreements[0].m_manifests.at(scrapers.Id(1)) == ManifestHash(later));

    BOOST_CHECK(agreements[1].m_content_hash == StatedContentHash(earlier));
    BOOST_REQUIRE(agreements[1].m_manifests.size() == 3);
    BOOST_CHECK(agreements[1].m_manifests.at(scrapers.Id(0)) == ManifestHash(earlier));
    BOOST_CHECK(agreements[1].m_manifests.at(scrapers.Id(2)) == earlier_of_2);
}

//! An agreement has the time of the latest of its manifests, whichever scraper holds that one. Here the latest
//! manifest of the first scraper is of the earlier agreement.
BOOST_AUTO_TEST_CASE(manifests_agreed_by_supermajority_have_the_time_of_their_latest_manifest)
{
    ThreeScrapers scrapers;

    const std::shared_ptr<CScraperManifest> of_0_and_2 = scrapers.Publishes(0, OTHER_STATISTICS);

    scrapers.Publishes(1, AGREED_STATISTICS);

    const std::shared_ptr<CScraperManifest> of_0_and_1 = scrapers.Publishes(0, AGREED_STATISTICS);

    scrapers.Publishes(2, OTHER_STATISTICS);

    const std::vector<ManifestAgreement> agreements =
        ManifestsAgreedBySupermajority(ScraperCullAndBinCScraperManifests());

    BOOST_REQUIRE(agreements.size() == 2);
    BOOST_CHECK(agreements[0].m_content_hash == StatedContentHash(of_0_and_2));
    BOOST_CHECK(agreements[1].m_content_hash == StatedContentHash(of_0_and_1));
}

//! The supermajority is of the scrapers that hold manifests. Two of five agree, and each of the three others holds a
//! manifest with the same parts that states another content hash, which is in no agreement.
BOOST_AUTO_TEST_CASE(manifests_agreed_by_supermajority_are_of_a_supermajority_of_the_scrapers_that_hold_manifests)
{
    FiveScrapers scrapers;

    const std::shared_ptr<CScraperManifest> of_two = scrapers.Publishes(0, AGREED_STATISTICS);

    scrapers.Publishes(1, AGREED_STATISTICS);

    uint256 another = StatedContentHash(of_two);
    *(another.end() - 1) ^= 0x01;

    for (const size_t n : {2, 3, 4}) {
        scrapers.IsSent(n, AGREED_STATISTICS, "project_a", another);
    }

    const mmCSManifestsBinnedByScraper manifests_by_scraper = ScraperCullAndBinCScraperManifests();

    BOOST_REQUIRE(manifests_by_scraper.size() == 5);
    BOOST_CHECK(ManifestsAgreedBySupermajority(manifests_by_scraper).empty());
}

//! A convergence is built from the latest of the agreements. A node that validates a superblock can hold later
//! manifests than the node that staked it, so the validation by manifest takes each of the agreements.
BOOST_AUTO_TEST_CASE(a_convergence_is_built_from_the_latest_agreement_and_a_superblock_is_valid_by_each)
{
    for (const int height : {V15_HEIGHT - 1, V15_HEIGHT}) {
        ThreeScrapers scrapers;

        const std::shared_ptr<CScraperManifest> earlier = scrapers.Publishes(0, OTHER_STATISTICS);

        scrapers.Publishes(1, OTHER_STATISTICS);
        scrapers.Publishes(2, OTHER_STATISTICS);

        const std::shared_ptr<CScraperManifest> later = scrapers.Publishes(1, AGREED_STATISTICS);

        scrapers.Publishes(2, AGREED_STATISTICS);

        const OwnConvergence own = scrapers.FormsItsOwnConvergenceFor(height);

        BOOST_REQUIRE_MESSAGE(own.formed, "height " << height);
        BOOST_CHECK_MESSAGE(!own.by_project, "height " << height);
        BOOST_CHECK_MESSAGE(own.scrapers == 2, "height " << height);
        BOOST_CHECK_MESSAGE(own.superblock.GetHash() == scrapers.SuperblockOf(later).GetHash(), "height " << height);

        BOOST_CHECK_MESSAGE(scrapers.IsGivenTheSuperblockOf(later, height, false).accepted, "height " << height);
        BOOST_CHECK_MESSAGE(scrapers.IsGivenTheSuperblockOf(earlier, height, false).accepted, "height " << height);
    }
}

//! From v15 the construction and the validation by manifest both take the agreements that
//! ManifestsAgreedBySupermajority() gives, so the superblock of the convergence a node builds from one manifest is
//! valid by manifest for a node with the same manifests.
BOOST_AUTO_TEST_CASE(the_superblock_of_a_convergence_built_from_one_manifest_is_valid_by_manifest_from_v15)
{
    const ProjectStatistics agreed{{"project_a", AGREED_STATISTICS}, {"project_b", OTHER_STATISTICS}};
    const ProjectStatistics renamed{{"project_b", AGREED_STATISTICS}, {"project_a", OTHER_STATISTICS}};

    using Manifests = std::function<void(ThreeScrapers&, size_t)>;

    const std::vector<std::pair<std::string, Manifests>> held{
        {"the manifests of three scrapers that agree",
         [&](ThreeScrapers& scrapers, const size_t place) {
             scrapers.Publishes(place, agreed);
             scrapers.Publishes((place + 1) % 3, agreed);
             scrapers.Publishes((place + 2) % 3, agreed);
         }},
        {"one of three with other project names",
         [&](ThreeScrapers& scrapers, const size_t place) {
             scrapers.Publishes((place + 1) % 3, agreed);
             scrapers.Publishes((place + 2) % 3, agreed);
             scrapers.IsSent(place, renamed);
         }},
        {"one of three that states another content hash",
         [&](ThreeScrapers& scrapers, const size_t place) {
             const std::shared_ptr<CScraperManifest> of_two = scrapers.Publishes((place + 1) % 3, agreed);

             scrapers.Publishes((place + 2) % 3, agreed);
             scrapers.IsSent(place, ProjectStatistics{{"project_a", OTHER_STATISTICS}, {"project_b", AGREED_STATISTICS}},
                             StatedContentHash(of_two));
         }},
        {"an earlier agreement of three and a later one of two",
         [&](ThreeScrapers& scrapers, const size_t place) {
             scrapers.Publishes(0, AGREED_STATISTICS);
             scrapers.Publishes(1, AGREED_STATISTICS);
             scrapers.Publishes(2, AGREED_STATISTICS);
             scrapers.Publishes((place + 1) % 3, agreed);
             scrapers.Publishes((place + 2) % 3, agreed);
         }},
    };

    for (const auto& manifests : held) {
        for (size_t place = 0; place < 3; ++place) {
            ThreeScrapers scrapers;

            manifests.second(scrapers, place);

            const OwnConvergence own = scrapers.FormsItsOwnConvergenceFor(V15_HEIGHT);

            BOOST_REQUIRE_MESSAGE(own.formed, manifests.first << ", place " << place);
            BOOST_REQUIRE_MESSAGE(!own.by_project, manifests.first << ", place " << place);
            BOOST_CHECK_MESSAGE(
                scrapers.ResultNotFromTheCacheFor(own.superblock, V15_HEIGHT) == "VALID_BY_MANIFEST",
                manifests.first << ", place " << place);
        }
    }
}

BOOST_AUTO_TEST_CASE(project_statistics_in_range_load_alike_before_and_from_version_15)
{
    const std::string records = "1000,10,100,c1\n2000.5,20,300,c2\n0,0,0,c3\n";

    const ScraperStats before = LoadProjectPart(records, false);
    const ScraperStats from_v15 = LoadProjectPart(records, true);

    // Three records and the project entry.
    BOOST_REQUIRE_EQUAL(before.size(), 4U);
    BOOST_REQUIRE_EQUAL(from_v15.size(), 4U);

    for (const auto& entry : before) {
        const ScraperObjectStats* const other = FindStats(from_v15, entry.first.objecttype, entry.first.objectID);

        BOOST_REQUIRE(other != nullptr);
        BOOST_CHECK_EQUAL(entry.second.statsvalue.dTC, other->statsvalue.dTC);
        BOOST_CHECK_EQUAL(entry.second.statsvalue.dRAT, other->statsvalue.dRAT);
        BOOST_CHECK_EQUAL(entry.second.statsvalue.dRAC, other->statsvalue.dRAC);
        BOOST_CHECK_EQUAL(entry.second.statsvalue.dAvgRAC, other->statsvalue.dAvgRAC);
        BOOST_CHECK_EQUAL(entry.second.statsvalue.dMag, other->statsvalue.dMag);
    }

    BOOST_CHECK_EQUAL(FindRecord(from_v15, "c1")->statsvalue.dMag, MagRound(100.0 / 400.0 * STATS_PROJECT_MAG));
    BOOST_CHECK_EQUAL(FindRecord(from_v15, "c2")->statsvalue.dMag, MagRound(300.0 / 400.0 * STATS_PROJECT_MAG));
    BOOST_CHECK_EQUAL(FindRecord(from_v15, "c3")->statsvalue.dMag, 0.0);
}

BOOST_AUTO_TEST_CASE(from_version_15_a_record_with_a_negative_value_is_passed_over)
{
    // c2 has a negative RAC, c3 a negative total credit and c4 a negative RAT.
    const std::string records = "1000,0,100,c1\n1000,0,-50,c2\n-1,0,100,c3\n1000,-1,100,c4\n1000,0,100,c5\n";

    const ScraperStats from_v15 = LoadProjectPart(records, true);

    BOOST_CHECK_EQUAL(from_v15.size(), 3U);
    BOOST_CHECK(FindRecord(from_v15, "c2") == nullptr);
    BOOST_CHECK(FindRecord(from_v15, "c3") == nullptr);
    BOOST_CHECK(FindRecord(from_v15, "c4") == nullptr);
    BOOST_REQUIRE(FindRecord(from_v15, "c1") != nullptr);
    BOOST_REQUIRE(FindRecord(from_v15, "c5") != nullptr);

    // The two records left share the project between them.
    BOOST_CHECK_EQUAL(FindRecord(from_v15, "c1")->statsvalue.dMag, STATS_PROJECT_MAG / 2);
    BOOST_CHECK_EQUAL(FindRecord(from_v15, "c5")->statsvalue.dMag, STATS_PROJECT_MAG / 2);

    const ScraperObjectStats* const project = FindStats(from_v15, statsobjecttype::byProject, STATS_PROJECT);

    BOOST_REQUIRE(project != nullptr);
    BOOST_CHECK_EQUAL(project->statsvalue.dRAC, 200.0);
    BOOST_CHECK_EQUAL(project->statsvalue.dTC, 2000.0);
}

BOOST_AUTO_TEST_CASE(from_version_15_a_record_with_a_value_that_is_not_finite_is_passed_over)
{
    // The records for c1, c2 and c3 each carry a word in place of a finite number. None is in the result.
    const std::string records = "nan,0,100,c1\n1000,0,inf,c2\n1000,infinity,100,c3\n1000,0,100,c4\n";

    const ScraperStats from_v15 = LoadProjectPart(records, true);

    BOOST_CHECK_EQUAL(from_v15.size(), 2U);
    BOOST_REQUIRE(FindRecord(from_v15, "c4") != nullptr);
    BOOST_CHECK_EQUAL(FindRecord(from_v15, "c4")->statsvalue.dMag, STATS_PROJECT_MAG);
}

BOOST_AUTO_TEST_CASE(from_version_15_a_project_with_no_rac_gives_no_magnitude)
{
    const std::string records = "1000,0,0,c1\n2000,0,0,c2\n";

    ScraperStats from_v15 = LoadProjectPart(records, true);

    BOOST_REQUIRE(FindRecord(from_v15, "c1") != nullptr);
    BOOST_REQUIRE(FindRecord(from_v15, "c2") != nullptr);
    BOOST_CHECK_EQUAL(FindRecord(from_v15, "c1")->statsvalue.dMag, 0.0);
    BOOST_CHECK_EQUAL(FindRecord(from_v15, "c2")->statsvalue.dMag, 0.0);

    BOOST_REQUIRE(ProcessNetworkWideFromProjectStats(from_v15));

    const ScraperObjectStats* cpid_1 = FindStats(from_v15, statsobjecttype::byCPID, "c1");

    BOOST_REQUIRE(cpid_1 != nullptr);
    BOOST_CHECK_EQUAL(cpid_1->statsvalue.dMag, 0.0);
}

BOOST_AUTO_TEST_CASE(from_version_15_a_project_rac_total_too_large_to_be_finite_gives_no_magnitude)
{
    const std::string records = "0,0,1e308,c1\n0,0,1e308,c2\n0,0,5,c3\n";

    const ScraperStats from_v15 = LoadProjectPart(records, true);

    BOOST_CHECK_EQUAL(from_v15.size(), 4U);

    for (const std::string cpid : {"c1", "c2", "c3"}) {
        BOOST_REQUIRE(FindRecord(from_v15, cpid) != nullptr);
        BOOST_CHECK_EQUAL(FindRecord(from_v15, cpid)->statsvalue.dMag, 0.0);
    }
}

BOOST_AUTO_TEST_CASE(from_version_15_a_project_total_credit_that_is_not_usable_is_taken_as_zero)
{
    const std::map<std::string, double> total_credits {
        { "project_1", 5000.5 },
        { "project_2", -1.0 },
        { "project_3", std::numeric_limits<double>::quiet_NaN() },
        { "project_4", std::numeric_limits<double>::infinity() },
    };

    CDataStream stream(SER_NETWORK, 1);
    stream << total_credits;

    CSplitBlob::CPart beacon_list_part(uint256S("01"));

    CSplitBlob::CPart total_credits_part(uint256S("02"));
    total_credits_part.data.resize(stream.size());
    std::memcpy(total_credits_part.data.data(), stream.data(), stream.size());

    CSplitBlob::CPart project_part(uint256S("03"));
    project_part.data = MakeProjectPart("1000,0,100,c1\n1000,0,-50,c2\n");

    ConvergedManifest converged;
    converged.ConvergedManifestPartPtrsMap.emplace("BeaconList", &beacon_list_part);
    converged.ConvergedManifestPartPtrsMap.emplace("ProjectsAllCpidTotalCredits", &total_credits_part);
    converged.ConvergedManifestPartPtrsMap.emplace(STATS_PROJECT, &project_part);

    const GRC::WhitelistSnapshot no_greylist(std::make_shared<GRC::ProjectList>());

    const ScraperStatsVerifiedBeaconsTotalCredits from_v15 =
        GetScraperStatsByConvergedManifest(no_greylist, converged, true);

    BOOST_CHECK(from_v15.m_checked_values);
    BOOST_REQUIRE_EQUAL(from_v15.m_total_credit_map.size(), 4U);
    BOOST_CHECK_EQUAL(from_v15.m_total_credit_map.at("project_1"), 5000.5);
    BOOST_CHECK_EQUAL(from_v15.m_total_credit_map.at("project_2"), 0.0);
    BOOST_CHECK_EQUAL(from_v15.m_total_credit_map.at("project_3"), 0.0);
    BOOST_CHECK_EQUAL(from_v15.m_total_credit_map.at("project_4"), 0.0);

    // The rules reach the project parts of the convergence as well.
    BOOST_CHECK(FindRecord(from_v15.mScraperStats, "c1") != nullptr);
    BOOST_CHECK(FindRecord(from_v15.mScraperStats, "c2") == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
