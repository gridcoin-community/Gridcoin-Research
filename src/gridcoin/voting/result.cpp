// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "arith_uint256.h"
#include "gridcoin/protocol.h"
#include "gridcoin/beacon.h"
#include "gridcoin/pool.h"
#include "gridcoin/quorum.h"
#include "gridcoin/superblock.h"
#include "gridcoin/voting/registry.h"
#include "gridcoin/voting/result.h"
#include "gridcoin/voting/poll.h"
#include "gridcoin/voting/vote.h"
#include "hash.h"
#include "gridcoin/researcher.h"
#include "logging.h"
#include "txdb.h"
#include "util/overflow.h"
#include "util/reverse_iterator.h"
#include "wallet/wallet.h"
#include "init.h"
#include "node/blockstorage.h"

#include <algorithm>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <tuple>
#include <unordered_set>

using namespace GRC;
using LogFlags = BCLog::LogFlags;
using ResponseDetail = PollResult::ResponseDetail;
using VoteDetail = PollResult::VoteDetail;
using Weight = PollResult::Weight;

namespace {
//!
//! \brief Used for mapping legacy vote answer labels to poll choice offsets.
//!
using LegacyChoiceMap = std::map<std::string, uint8_t>;

//!
//! \brief Thrown when resolving an invalid vote contract.
//!
class InvalidVoteError : public std::exception
{
public:
    InvalidVoteError()
    {
    }
};

//!
//! \brief Thrown for a vote that carries no weight because newer counted votes
//! already hold everything it claims.
//!
class SupersededVoteError : public std::exception
{
public:
    SupersededVoteError()
    {
    }
};

//!
//! \brief Hashes \c COutPoint objects to key a lookup table.
//!
struct OutPointHasher
{
    size_t operator()(const COutPoint& outpoint) const
    {
        return outpoint.hash.GetUint64(0) + outpoint.n;
    }
};

//!
//! \brief Contains an unprocessed vote contract for a poll extracted from a
//! transaction.
//!
class VoteCandidate
{
public:
    //!
    //! \brief Initialize a vote candidate.
    //!
    //! \param contract Contains a vote contract.
    //! \param tx       Transaction that contains the vote contract.
    //! \param pos      Where the transaction is on disk.
    //!
    VoteCandidate(const Contract& contract, CTransaction tx, const CDiskTxPos& pos)
        : m_tx(std::move(tx))
        , m_pos(pos)
        , m_contract(contract)
        , m_payload(m_contract.SharePayload())
    {
    }

    //!
    //! \brief Determine whether the candidate contains a legacy vote contract.
    //!
    bool IsLegacy() const
    {
        return m_contract.m_version <= 1;
    }

    //!
    //! \brief Get the vote object from the contract.
    //!
    const GRC::Vote& Vote() const
    {
        assert(!IsLegacy());
        return m_payload.As<GRC::Vote>();
    }

    //!
    //! \brief Get the legacy vote object from the contract.
    //!
    const GRC::LegacyVote& LegacyVote() const
    {
        assert(IsLegacy());
        return m_payload.As<GRC::LegacyVote>();
    }

    //!
    //! \brief returns IsMine flag on transaction containing vote contract
    //!
    isminetype IsMine() const
    {
        LOCK(pwalletMain->cs_wallet);

        LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: vote contract tx hash %s ismine status %i",
                 __func__,
                 m_tx.GetHash().GetHex(),
                 pwalletMain->IsMine(m_tx));

        return pwalletMain->IsMine(m_tx);
    }

    //!
    //! \brief Serialize a vote for claim signing and verification.
    //!
    //! Used for non-legacy vote contracts only.
    //!
    //! \return The critical contract data serialized as bytes.
    //!
    ClaimMessage PackMessage() const
    {
        return PackVoteMessage(Vote(), m_tx);
    }

    //!
    //! \brief Get where the transaction that contains the vote is on disk.
    //!
    const CDiskTxPos& Position() const
    {
        return m_pos;
    }

private:
    const CTransaction m_tx;         //!< Transaction that contains a vote.
    const CDiskTxPos m_pos;          //!< Where the transaction is on disk.

    //!
    //! \brief The vote contract.
    //!
    //! Held by value. For a version 1 transaction, GetContracts() returns a
    //! cache parsed from hashBoinc that moving the transaction does not carry,
    //! so a reference into the caller's transaction would dangle once that
    //! transaction is gone.
    //!
    const Contract m_contract;

    const ContractPayload m_payload; //!< Contains the body of the vote.
}; // VoteCandidate

//!
//! \brief Validates and resolves the voting weight for vote contracts.
//!
class VoteResolver
{
public:
    //!
    //! \brief Initialize a vote resolver for the specified poll.
    //!
    //! \param txdb          Used to resolve balance claim amounts.
    //! \param poll          The poll to resolve voting weight for.
    //! \param v15_rules     Whether the poll started at or after the v15 height,
    //! which selects how a spent claimed output is resolved (ResolveAmount).
    //! \param pindex_bound  The block that bounds the tally by blocks (PollSnapshot::m_pindex_bound), or nullptr
    //! when the poll keeps the time rules of #3470.
    //! \param poll_start_height The height of the poll's block. Under the v15 rules the walk follows a vote
    //! transaction only from that height (ResolveAmount).
    //!
    VoteResolver(CTxDB& txdb, const Poll& poll, const bool v15_rules, const CBlockIndex* pindex_bound,
                 const int poll_start_height)
        : m_txdb(txdb)
        , m_poll(poll)
        , m_v15_rules(v15_rules)
        , m_poll_start_height(poll_start_height)
        , m_pindex_bound(pindex_bound)
        , m_superblock(SuperblockPtr::Empty())
    {
    }

    //!
    //! \brief Set the superblock used to calculate magnitude weight.
    //!
    //! \param superblock Latest superblock in the poll window.
    //!
    void SetSuperblock(SuperblockPtr superblock)
    {
        m_superblock = std::move(superblock);
    }

    //!
    //! \brief Resolve the voting weight for the provided vote.
    //!
    //! \param candidate Contains the vote contract to resolve the voting
    //! weight for.
    //!
    //! \return A summary of the voting weight for the vote.
    //!
    VoteDetail Resolve(const VoteCandidate& candidate)
    {
        g_timer.GetTimes(std::string{"Begin "} + std::string{__func__}, "buildPollTable");

        const Vote& vote = candidate.Vote();
        const ClaimMessage message = candidate.PackMessage();

        m_vote_txos.clear();
        m_vote_cpids.clear();
        m_vote_walked.clear();
        m_reached_counted = false;

        VoteDetail detail;
        detail.m_amount = Resolve(vote.m_claim.m_balance_claim, message);
        detail.m_ismine = candidate.IsMine();

        if (m_poll.IncludesMagnitudeWeight()) {
            const MagnitudeClaim& claim = vote.m_claim.m_magnitude_claim;

            // The vote takes the claim's CPID only when the claim is credited to
            // this vote.
            if (const std::optional<Magnitude> magnitude = Resolve(claim, message, candidate.Position())) {
                detail.m_mining_id = claim.m_mining_id;
                detail.m_magnitude = *magnitude;
            }
        }

        g_timer.GetTimes(std::string{"End "} + std::string{__func__}, "buildPollTable");
        return detail;
    }

    //!
    //! \brief Record the outputs and the CPID that the last resolved vote
    //! reached. Call it only when that vote is counted.
    //!
    void Commit()
    {
        m_seen_txos.insert(m_vote_txos.begin(), m_vote_txos.end());
        m_seen_cpids.insert(m_vote_cpids.begin(), m_vote_cpids.end());

        m_vote_txos.clear();
        m_vote_cpids.clear();
    }

    //!
    //! \brief Whether the last resolved vote reached an output or a CPID that a
    //! counted vote already holds.
    //!
    bool ReachedCounted() const
    {
        return m_reached_counted;
    }

private:
    CTxDB& m_txdb;              //!< Used to resolve unspent amount claims.
    const Poll& m_poll;         //!< The poll to resolve voting weight for.
    const bool m_v15_rules;     //!< The poll started at or after the v15 height.
    const int m_poll_start_height; //!< The height of the poll's block.
    //! The block that bounds the tally by blocks, or nullptr when the poll keeps the time rules of #3470. Its height
    //! never changes, so it is read without cs_main.
    const CBlockIndex* const m_pindex_bound;
    SuperblockPtr m_superblock; //!< Used to determine magnitude weight.

    //!
    //! \brief Remembers the outputs claimed by a vote contract for balance
    //! claims to filter duplicate weight claims.
    //!
    //! Each claimed output, and each output the walk follows, is recorded when
    //! it is first reached, so it counts once per poll. Votes resolve newest
    //! first, so the newest counted vote that reaches an output keeps its
    //! weight.
    //!
    std::unordered_set<COutPoint, OutPointHasher> m_seen_txos;

    //!
    //! \brief Remembers the CPID claimed by a vote contract for magnitude
    //! claims to filter duplicate weight claims.
    //!
    std::unordered_set<Cpid> m_seen_cpids;

    //!
    //! \brief The outputs and the CPID that the vote being resolved reaches.
    //!
    //! They join m_seen_txos and m_seen_cpids only when the vote is counted
    //! (Commit).
    //!
    std::unordered_set<COutPoint, OutPointHasher> m_vote_txos;
    std::unordered_set<Cpid> m_vote_cpids;

    //! The most vote transactions the walk passes a claimed output through for
    //! one vote, under the v15 rules.
    static constexpr size_t MAX_WALKED_VOTE_TXS = 512;

    //! The vote transactions the walk has passed outputs through for the vote
    //! being resolved.
    std::set<uint256> m_vote_walked;

    //!
    //! \brief What the walk needs of a transaction that spends an output it
    //! follows. It is read from disk once per tally.
    //!
    struct Spender
    {
        bool m_after_end = false;  //!< Its block is after the poll's end (AfterEnd).
        bool m_before_start = false; //!< Under the v15 rules, its block is below the poll's block.
        bool m_coinstake = false;  //!< Whether it is a coinstake.
        bool m_carries_vote = false; //!< Whether it carries a vote contract.
        uint256 m_hash;            //!< Its hash.

        //! The destination and amount of each output, kept only for a
        //! transaction the walk can follow (a coinstake, or under the v15 rules
        //! a vote transaction). A destination that cannot be extracted is
        //! CNoDestination.
        std::vector<std::pair<CTxDestination, CAmount>> m_outputs;

        //! Where each output is spent, read when first needed.
        std::optional<std::vector<CDiskTxPos>> m_spent;
    };

    //! The spending transactions read in this tally, by position on disk.
    std::map<std::tuple<unsigned int, unsigned int, unsigned int>, Spender> m_spenders;

    //!
    //! \brief Get the spending transaction at \p pos, reading it from disk the
    //! first time. The transaction itself is read only when its block is not
    //! past the poll's end, as the walk ignores it otherwise.
    //!
    //! \throws InvalidVoteError If it cannot be read.
    //!
    Spender& ReadSpender(const CDiskTxPos& pos)
    {
        const auto inserted = m_spenders.try_emplace(std::make_tuple(pos.nFile, pos.nBlockPos, pos.nTxPos));
        Spender& spender = inserted.first->second;

        if (!inserted.second) {
            return spender;
        }

        CAutoFile file(OpenBlockFile(pos), SER_DISK, CLIENT_VERSION);

        if (file.IsNull()) {
            m_spenders.erase(inserted.first);
            error("%s: OpenBlockFile failed", __func__);
            throw InvalidVoteError();
        }

        CBlockHeader header;

        try {
            file >> header;
        } catch (...) {
            m_spenders.erase(inserted.first);
            error("%s: deserialize or I/O error for block header", __func__);
            throw InvalidVoteError();
        }

        spender.m_after_end = AfterEnd(header);

        if (spender.m_after_end) {
            return spender;
        }

        if (m_v15_rules) {
            // The height was looked up for AfterEnd() and kept, so this takes no lock again.
            const std::optional<int> height = MainChainHeight(header);

            spender.m_before_start = height && *height < m_poll_start_height;
        }

        CTransaction tx;

        if (fseek(file.Get(), pos.nTxPos, SEEK_SET) != 0) {
            m_spenders.erase(inserted.first);
            error("%s: tx fseek failed", __func__);
            throw InvalidVoteError();
        }

        try {
            file >> tx;
        } catch (...) {
            m_spenders.erase(inserted.first);
            error("%s: deserialize or I/O error for tx", __func__);
            throw InvalidVoteError();
        }

        spender.m_coinstake = tx.IsCoinStake();
        spender.m_carries_vote = CarriesVote(tx);
        spender.m_hash = tx.GetHash();

        if (spender.m_coinstake || (m_v15_rules && spender.m_carries_vote)) {
            spender.m_outputs.reserve(tx.vout.size());

            for (const CTxOut& output : tx.vout) {
                CTxDestination dest;

                if (!ExtractDestination(output.scriptPubKey, dest)) {
                    dest = CNoDestination();
                }

                spender.m_outputs.emplace_back(std::move(dest), output.nValue);
            }
        }

        return spender;
    }

    //! Whether the vote being resolved reached something a counted vote holds.
    bool m_reached_counted {false};

    //! The block of a beacon advertisement and the superblock that verified
    //! it; each is nullptr where it was not found.
    struct Verification
    {
        const CBlockIndex* m_advertisement = nullptr;
        const CBlockIndex* m_superblock = nullptr;
    };

    //! The blocks of each beacon advertisement looked up in this tally, by
    //! advertisement hash.
    std::map<uint256, Verification> m_verifications;

    //! The main chain height of each block looked up in this tally, by block hash; nothing for a block off the main
    //! chain.
    std::map<uint256, std::optional<int>> m_heights;

    //!
    //! \brief Get the height of the block that \p header heads, if that block is on the main chain. It is looked up
    //! under a brief cs_main and kept for the rest of the tally.
    //!
    //! \throws InvalidDuetoReorgFork If the block that bounds the tally has left the main chain: a reorg during the
    //! tally. The callers retry on it, as on PollRegistry::reorg_occurred_during_reg_traversal.
    //!
    std::optional<int> MainChainHeight(const CBlockHeader& header)
    {
        const uint256 hash = header.GetHash();
        const auto found = m_heights.find(hash);

        if (found != m_heights.end()) {
            return found->second;
        }

        LOCK(cs_main);

        if (m_pindex_bound && !m_pindex_bound->IsInMainChain()) {
            throw InvalidDuetoReorgFork();
        }

        const auto iter = mapBlockIndex.find(hash);
        std::optional<int> height;

        if (iter != mapBlockIndex.end() && iter->second != nullptr && iter->second->IsInMainChain()) {
            height = iter->second->nHeight;
        }

        m_heights.emplace(hash, height);

        return height;
    }

    //! A CPID's beacon revocations on the main chain, as AcrossRevocations reads them.
    struct Revocations
    {
        //! The revocations that removed a beacon, with the main chain height of each, newest first; within a block
        //! the later revocation first. One that removed nothing continues the walk into nothing, so it decides only
        //! as a revocation at or before the vote, which m_lowest_height answers.
        std::vector<std::pair<int, Beacon_ptr>> m_removals;

        //! The lowest main chain height of the CPID's revocations, those that removed nothing included; nothing if
        //! the CPID has none on the main chain.
        std::optional<int> m_lowest_height;
    };

    //! The revocations of each CPID looked up in this tally, by CPID.
    std::map<Cpid, Revocations> m_revocations;

    //!
    //! \brief Get a CPID's revocations, read from the transaction index on the first call for the CPID and kept for
    //! the rest of the tally.
    //!
    //! \throws InvalidVoteError If the block of a revocation cannot be read. Nothing is kept for the CPID then.
    //!
    const Revocations& RevocationsOf(const Cpid& cpid) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        const auto found = m_revocations.find(cpid);

        if (found != m_revocations.end()) {
            return found->second;
        }

        const std::vector<Beacon_ptr> deleted = GetBeaconRegistry().DeletedEntries(cpid);
        std::vector<std::pair<int, Beacon_ptr>> revocations;

        // The newest record first, so that within a block the later revocation comes first.
        for (auto iter = deleted.rbegin(); iter != deleted.rend(); ++iter) {
            CTxIndex tx_index;

            if (!m_txdb.ReadTxIndex((*iter)->m_hash, tx_index)) {
                continue;
            }

            if (const std::optional<int> height = MainChainHeight(ReadHeader(tx_index.pos))) {
                revocations.emplace_back(*height, *iter);
            }
        }

        std::stable_sort(revocations.begin(), revocations.end(),
                         [](const auto& a, const auto& b) { return a.first > b.first; });

        Revocations found_revocations;

        if (!revocations.empty()) {
            found_revocations.m_lowest_height = revocations.back().first;
        }

        for (const auto& revocation : revocations) {
            if (!revocation.second->m_previous_hash.IsNull()) {
                found_revocations.m_removals.push_back(revocation);
            }
        }

        return m_revocations.emplace(cpid, std::move(found_revocations)).first->second;
    }

    //!
    //! \brief Whether the block that \p header heads is after the poll's end: for a poll bounded by blocks, a block
    //! above the bound or off the main chain; for any other poll, a block stamped after the end (#3470).
    //!
    //! \throws InvalidDuetoReorgFork As MainChainHeight.
    //!
    bool AfterEnd(const CBlockHeader& header)
    {
        if (!m_pindex_bound) {
            return m_poll.Expired(header.nTime);
        }

        const std::optional<int> height = MainChainHeight(header);

        return !height || *height > m_pindex_bound->nHeight;
    }

    //!
    //! \brief Whether a counted vote, or the vote being resolved, already
    //! reached an output the walk follows. The walk follows only outputs at the
    //! claim's address.
    //!
    bool Seen(const COutPoint& txo)
    {
        if (m_seen_txos.count(txo) != 0) {
            m_reached_counted = true;
            return true;
        }

        return m_vote_txos.count(txo) != 0;
    }

    //!
    //! \brief Resolve the claimed magnitude for a vote.
    //!
    //! \param claim    The claim to resolve a CPID's magnitude for.
    //! \param message  Serialized context for vote signature verification.
    //! \param vote_pos Where the transaction that contains the vote is on disk.
    //!
    //! \return Magnitude as of the latest superblock in the poll window; zero
    //! for a claim without a CPID; nothing when the claim's CPID is not credited
    //! to this vote.
    //!
    std::optional<Magnitude> Resolve(const MagnitudeClaim& claim, const ClaimMessage& message, const CDiskTxPos& vote_pos)
    {
        const CpidOption cpid = claim.m_mining_id.TryCpid();

        if (!cpid) {
            return Magnitude::Zero();
        }

        if (m_vote_cpids.count(*cpid) != 0) {
            return std::nullopt;
        }

        // A counted vote has this CPID. The claim is still checked below, so
        // only a vote whose claim holds is superseded by that vote.
        const bool counted = m_seen_cpids.count(*cpid) != 0;

        CTxIndex tx_index;

        if (!m_txdb.ReadTxIndex(claim.m_beacon_txid, tx_index)) {
            LogPrint(LogFlags::VOTE, "%s: failed to read beacon index", __func__);
            throw InvalidVoteError();
        }

        CAutoFile file(OpenBlockFile(tx_index.pos), SER_DISK, CLIENT_VERSION);

        if (file.IsNull()) {
            error("%s: OpenBlockFile failed", __func__);
            throw InvalidVoteError();
        }

        CBlockHeader header;

        try {
            file >> header;
        } catch (...) {
            error("%s: deserialize or I/O error for block header", __func__);
            throw InvalidVoteError();
        }

        // A beacon confirmed after the poll ends supplies no magnitude.
        if (AfterEnd(header)) {
            LogPrint(LogFlags::VOTE, "%s: beacon confirmed after poll", __func__);
            return std::nullopt;
        }

        if (!IsInMainChain(header)) {
            LogPrint(LogFlags::VOTE, "%s: beacon not in main chain", __func__);
            return std::nullopt;
        }

        if (fseek(file.Get(), tx_index.pos.nTxPos, SEEK_SET) != 0) {
            error("%s: tx fseek failed", __func__);
            throw InvalidVoteError();
        }

        CTransaction tx;

        try {
            file >> tx;
        } catch (...) {
            error("%s: deserialize or I/O error for tx", __func__);
            throw InvalidVoteError();
        }

        for (const auto& contract : tx.GetContracts()) {
            if (contract.m_type != ContractType::BEACON) {
                continue;
            }

            auto payload = contract.SharePayloadAs<BeaconPayload>();

            if (payload->m_cpid != *cpid) {
                continue;
            }

            if (!claim.VerifySignature(payload->m_beacon.m_public_key, message)) {
                LogPrint(LogFlags::VOTE, "%s: bad beacon signature", __func__);
                throw InvalidVoteError();
            }

            if (!BeaconKeyInForce(*cpid, claim.m_beacon_txid, payload->m_beacon.m_public_key, contract.m_version,
                                  tx.nTime, vote_pos)) {
                LogPrint(LogFlags::VOTE, "%s: beacon key not in force for the vote", __func__);
                return std::nullopt;
            }

            if (counted) {
                m_reached_counted = true;
                return std::nullopt;
            }

            m_vote_cpids.emplace(*cpid);

            return m_superblock->m_cpids.MagnitudeOf(*cpid);
        }

        return std::nullopt;
    }

    //!
    //! \brief Determine whether a key was the CPID's beacon key when the vote
    //! was mined.
    //!
    //! The key in force is found by walking back from the CPID's current
    //! beacon: the first key whose verifying superblock is at or before the
    //! vote's block, provided the beacon had not expired by that block's time.
    //! A renewal keeps the key of the beacon it renews. Where the walk steps
    //! back past a key verified after the vote, the key of the advertisement
    //! the claim names is in force if its verifying superblock is at or above
    //! the block of that key's advertisement and at or before the vote's
    //! block, and the named beacon had not expired by then. A version 1 beacon,
    //! which the beacon registry does not hold, is in force for 14 days after
    //! block version 11, as it was then.
    //!
    //! Where the walk reaches the start of the current beacon's chain, or
    //! the CPID has no beacon, it continues past the CPID's revocations
    //! (AcrossRevocations): a revocation at or before the vote's block means
    //! no key was in force, and decides. The CPID's beacon entries are read as
    //! they stand when the poll is tallied.
    //!
    //! \param cpid             The CPID that the magnitude claim names.
    //! \param beacon_txid      The beacon contract transaction the claim names.
    //! \param key              The key in that beacon contract.
    //! \param contract_version The version of that beacon contract.
    //! \param beacon_time      The time of the transaction that carries it.
    //! \param vote_pos         Where the transaction that contains the vote is.
    //!
    //! \throws InvalidVoteError If the vote's block, or the block of a beacon
    //! advertisement or revocation it looks up, cannot be read.
    //!
    bool BeaconKeyInForce(
        const Cpid& cpid,
        const uint256& beacon_txid,
        const CPubKey& key,
        const uint32_t contract_version,
        const int64_t beacon_time,
        const CDiskTxPos& vote_pos)
    {
        const CBlockHeader header = ReadHeader(vote_pos);

        LOCK(cs_main);

        const auto iter = mapBlockIndex.find(header.GetHash());

        if (iter == mapBlockIndex.end() || iter->second == nullptr) {
            LogPrint(LogFlags::VOTE, "%s: vote block not indexed", __func__);
            throw InvalidVoteError();
        }

        const int vote_height = iter->second->nHeight;
        const int64_t vote_time = iter->second->nTime;

        if (const std::optional<bool> in_force =
                KeyInForce(GetBeaconRegistry().Try(cpid), key, vote_height, vote_time, beacon_txid)) {
            return *in_force;
        }

        if (const std::optional<bool> in_force = AcrossRevocations(cpid, beacon_txid, key, vote_height, vote_time)) {
            return *in_force;
        }

        return contract_version == 1
            && g_v11_timestamp > 0
            && vote_time - g_v11_timestamp <= 14 * 86400
            && vote_time - beacon_time <= Beacon::MAX_AGE;
    }

    //!
    //! \brief Determine whether a key was the one in force at the vote on a
    //! chain of a CPID's beacon entries.
    //!
    //! \param entry          The newest entry of the chain.
    //! \param key            The key in the beacon contract the claim names.
    //! \param vote_height    The height of the vote's block.
    //! \param vote_time      The time of the vote's block.
    //! \param named_txid     The beacon contract the claim names, for a key the
    //! walk does not reach (see NamedKeyInForce).
    //!
    //! \return Nothing if no key on the chain was in force at the vote.
    //!
    std::optional<bool> KeyInForce(
        Beacon_ptr entry,
        const CPubKey& key,
        const int vote_height,
        const int64_t vote_time,
        const uint256& named_txid) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        BeaconRegistry& beacons = GetBeaconRegistry();
        std::set<uint256> visited;
        const CBlockIndex* verified = nullptr;

        // The newest entry of the key being examined that is stamped at or
        // before the vote: the entry whose age decides expiry.
        std::optional<int64_t> entry_time;

        // Walk the entries newest first, one key at a time.
        while (entry && visited.insert(entry->m_hash).second) {
            if (entry->m_status == BeaconStatusForStorage::RENEWAL) {
                if (!entry_time && entry->m_timestamp <= vote_time) {
                    entry_time = entry->m_timestamp;
                }
            } else if (entry->m_status == BeaconStatusForStorage::ACTIVE) {
                const Beacon_ptr pending = entry->m_previous_hash.IsNull()
                    ? nullptr
                    : beacons.FindHistorical(entry->m_previous_hash);

                const bool from_superblock = pending && pending->m_status == BeaconStatusForStorage::PENDING;

                verified = from_superblock ? VerifiedBy(*entry, *pending) : nullptr;

                const bool in_force = from_superblock
                    ? verified && verified->nHeight <= vote_height
                    : entry->m_timestamp <= vote_time;

                if (in_force) {
                    // GCC 12 warns that entry_time's value may be used
                    // uninitialized here, but value_or() reads it only when it
                    // is set. GCC 13 and later do not warn.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 13
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
                    const int64_t age = vote_time - entry_time.value_or(entry->m_timestamp);
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 13
#   pragma GCC diagnostic pop
#endif

                    return entry->m_public_key == key && age <= Beacon::MAX_AGE;
                }

                // Verified after the vote: the key before it was in force then.
                entry_time.reset();

                if (from_superblock) {
                    if (verified) {
                        if (const std::optional<bool> named =
                                NamedKeyInForce(named_txid, *pending, key, vote_height, vote_time)) {
                            return named;
                        }
                    }

                    // The advertisement links to the beacon that was active when it was mined. A revocation mined in
                    // its block or later, and at or before the vote, removed that beacon's chain, so no key on it was
                    // in force at the vote: the rule AcrossRevocations applies where the CPID has no current beacon.
                    if (!pending->m_previous_hash.IsNull() && RevokedSince(*pending, vote_height)) {
                        return false;
                    }

                    entry = pending;
                }
            } else {
                break;
            }

            if (entry->m_previous_hash.IsNull()) {
                break;
            }

            entry = beacons.FindHistorical(entry->m_previous_hash);
        }

        return std::nullopt;
    }

    //!
    //! \brief Continue the key-in-force walk past a CPID's revocations.
    //!
    //! An advertisement made after a revocation is not linked to the CPID's
    //! earlier entries, so the walk from the current beacon, or from none when
    //! the CPID has no beacon, stops at the start of its chain. Taken newest
    //! first, a revocation at or before the vote's block means no key was in
    //! force at the vote; one after it continues the walk into the chain it
    //! removed. A revocation's block is read through the transaction index, on
    //! the main chain only. A revocation that removed no beacon continues into
    //! nothing. KeyInForce applies the same rule where the walk steps from a
    //! pending advertisement into the beacon it links to (RevokedSince).
    //!
    //! \return Nothing if no revocation decides.
    //!
    std::optional<bool> AcrossRevocations(
        const Cpid& cpid,
        const uint256& beacon_txid,
        const CPubKey& key,
        const int vote_height,
        const int64_t vote_time) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        BeaconRegistry& beacons = GetBeaconRegistry();
        const Revocations& revocations = RevocationsOf(cpid);

        // Every removal after the vote comes before any revocation at or before it.
        for (const auto& [height, revocation] : revocations.m_removals) {
            if (height <= vote_height) {
                break;
            }

            if (const std::optional<bool> in_force = KeyInForce(
                    beacons.FindHistorical(revocation->m_previous_hash), key, vote_height, vote_time, beacon_txid)) {
                return in_force;
            }
        }

        if (revocations.m_lowest_height && *revocations.m_lowest_height <= vote_height) {
            return false;
        }

        return std::nullopt;
    }

    //!
    //! \brief Whether a revocation of the CPID of a beacon advertisement that
    //! removed a beacon was mined in the advertisement's block or later, and at
    //! or before the vote's block.
    //!
    //! \param advertisement The pending entry of the advertisement.
    //! \param vote_height   The height of the vote's block.
    //!
    bool RevokedSince(const Beacon& advertisement, const int vote_height) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        const CBlockIndex* const pindex = VerificationOf(advertisement).m_advertisement;

        if (!pindex) {
            return false;
        }

        // Newest first: the first removal at or before the vote is the latest one.
        for (const auto& removal : RevocationsOf(advertisement.m_cpid).m_removals) {
            if (removal.first <= vote_height) {
                return removal.first >= pindex->nHeight;
            }
        }

        return false;
    }

    //!
    //! \brief Determine whether the key of the advertisement a magnitude claim
    //! names was in force at the vote, where the walk steps back past a key
    //! verified after the vote.
    //!
    //! An advertisement links to the entry in force when its contract is
    //! applied. In a superblock's own block that is after the superblock's
    //! activations when the block is connected, and before them in a replay.
    //! If the later key was verified after the vote, a key that a superblock at
    //! or above the later advertisement's block, and at or before the vote's
    //! block, verified is taken as in force at the vote, unless it had expired
    //! by then.
    //!
    //! \param beacon_txid The beacon contract the claim names.
    //! \param later       The entry of the later advertisement.
    //! \param key         The key in the beacon contract the claim names.
    //! \param vote_height The height of the vote's block.
    //! \param vote_time   The time of the vote's block.
    //!
    //! \return Nothing if the claim names no key verified in that interval.
    //!
    std::optional<bool> NamedKeyInForce(
        const uint256& beacon_txid,
        const Beacon& later,
        const CPubKey& key,
        const int vote_height,
        const int64_t vote_time) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        const Beacon_ptr named = GetBeaconRegistry().FindHistorical(beacon_txid);

        if (!named || named->m_status != BeaconStatusForStorage::PENDING) {
            return std::nullopt;
        }

        const CBlockIndex* superblock = VerificationOf(*named).m_superblock;
        const CBlockIndex* later_block = VerificationOf(later).m_advertisement;

        if (!superblock || !later_block || superblock->nHeight > vote_height
            || superblock->nHeight < later_block->nHeight) {
            return std::nullopt;
        }

        return named->m_public_key == key && vote_time - named->m_timestamp <= Beacon::MAX_AGE;
    }

    //!
    //! \brief Find the superblock that verified a beacon.
    //!
    //! \param active  The entry the superblock activated.
    //! \param pending The advertisement's entry.
    //!
    //! \return nullptr if no superblock on the main chain made \p active from
    //! \p pending.
    //!
    const CBlockIndex* VerifiedBy(const Beacon& active, const Beacon& pending) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        const CBlockIndex* superblock = VerificationOf(pending).m_superblock;

        if (superblock && Hash(superblock->GetBlockHash(), pending.m_hash) == active.m_hash) {
            return superblock;
        }

        return nullptr;
    }

    //!
    //! \brief Find the block of a beacon advertisement and the superblock that
    //! verified it.
    //!
    //! The entry a superblock activates carries the advertisement's time, not
    //! the superblock's. Its hash is Hash(superblock block hash, advertisement
    //! hash), so the superblock is found by scanning forward from the
    //! advertisement's block. The scan ends at the first superblock that would
    //! have swept the advertisement as expired. The blocks found are kept for
    //! the rest of the tally.
    //!
    //! \return A null block where the advertisement, or a superblock that
    //! verified it, is not found on the main chain.
    //!
    const Verification& VerificationOf(const Beacon& pending) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        auto iter = m_verifications.find(pending.m_hash);

        if (iter == m_verifications.end()) {
            iter = m_verifications.emplace(pending.m_hash, FindVerification(pending)).first;
        }

        return iter->second;
    }

    Verification FindVerification(const Beacon& pending) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        Verification found;
        CTxIndex tx_index;

        if (!m_txdb.ReadTxIndex(pending.m_hash, tx_index)) {
            LogPrint(LogFlags::VOTE, "%s: failed to read beacon advertisement index", __func__);
            return found;
        }

        const auto iter = mapBlockIndex.find(ReadHeader(tx_index.pos).GetHash());

        if (iter == mapBlockIndex.end() || iter->second == nullptr || !iter->second->IsInMainChain()) {
            return found;
        }

        found.m_advertisement = iter->second;

        BeaconRegistry& beacons = GetBeaconRegistry();

        for (const CBlockIndex* pindex = iter->second; pindex; pindex = pindex->pnext) {
            if (!pindex->IsSuperblock()) {
                continue;
            }

            // The superblock that sweeps the advertisement as expired stores an
            // entry under the same form of hash, so the entry's status decides.
            const Beacon_ptr entry = beacons.FindHistorical(Hash(pindex->GetBlockHash(), pending.m_hash));

            if (entry && entry->m_status == BeaconStatusForStorage::ACTIVE) {
                found.m_superblock = pindex;
                return found;
            }

            if (pindex->nTime - pending.m_timestamp > PendingBeacon::RETENTION_AGE) {
                break;
            }
        }

        return found;
    }

    //!
    //! \brief Read the header of the block that holds a transaction.
    //!
    //! \throws InvalidVoteError If the block cannot be read.
    //!
    static CBlockHeader ReadHeader(const CDiskTxPos& pos)
    {
        CAutoFile file(OpenBlockFile(pos), SER_DISK, CLIENT_VERSION);

        if (file.IsNull()) {
            error("%s: OpenBlockFile failed", __func__);
            throw InvalidVoteError();
        }

        CBlockHeader header;

        try {
            file >> header;
        } catch (...) {
            error("%s: deserialize or I/O error for block header", __func__);
            throw InvalidVoteError();
        }

        return header;
    }

    //!
    //! \brief Resolve the claimed balance for a vote.
    //!
    //! \param claim   The claim to resolve balance weight for.
    //! \param message Serialized context for vote signature verification.
    //!
    //! \return Claimed amount in units of 1/100000000 GRC.
    //!
    CAmount Resolve(const BalanceClaim& claim, const ClaimMessage& message)
    {
        CAmount amount = 0;

        for (const auto& address_claim : claim.m_address_claims) {
            amount = AddClaimedAmount(amount, Resolve(address_claim, message));
        }

        return amount;
    }

    //!
    //! \brief Resolve the claimed balance for an address.
    //!
    //! \param claim   The claim to resolve balance weight for.
    //! \param message Serialized context for vote signature verification.
    //!
    //! \return Claimed amount in units of 1/100000000 GRC.
    //!
    //! \throws InvalidVoteError If the vote fails to validate or if an IO
    //! error occurs.
    //!
    CAmount Resolve(const AddressClaim& claim, const ClaimMessage& message)
    {
        if (!claim.VerifySignature(message)) {
            LogPrint(LogFlags::VOTE, "%s: bad address signature", __func__);
            throw InvalidVoteError();
        }

        const CTxDestination address = claim.m_public_key.GetID();
        CAmount amount = 0;

        for (const auto& txo : claim.m_outpoints) {
            amount = AddClaimedAmount(amount, Resolve(txo, address));
        }

        return amount;
    }

    //!
    //! \brief Resolve the claimed amount for an output.
    //!
    //! \param txo     Refers to the output to resolve balance weight for.
    //! \param address Must match the address of the resolved output.
    //!
    //! \return Claimed amount in units of 1/100000000 GRC.
    //!
    //! \throws InvalidVoteError If the vote fails to validate or if an IO
    //! error occurs.
    //!
    CAmount Resolve(const COutPoint& txo, const CTxDestination& address)
    {
        if (m_vote_txos.count(txo) != 0) {
            LogPrint(LogFlags::VOTE, "%s: duplicate txo", __func__);
            return 0;
        }

        // A counted vote holds this output. The claim is still checked below,
        // so only a vote that owns the output is superseded by that vote.
        const bool counted = m_seen_txos.count(txo) != 0;

        CTxIndex tx_index;

        if (!m_txdb.ReadTxIndex(txo.hash, tx_index)) {
            LogPrint(LogFlags::VOTE, "%s: failed to read tx index", __func__);
            return 0;
        }

        CAutoFile file(OpenBlockFile(tx_index.pos), SER_DISK, CLIENT_VERSION);

        if (file.IsNull()) {
            error("%s: OpenBlockFile failed", __func__);
            throw InvalidVoteError();
        }

        CBlockHeader header;

        try {
            file >> header;
        } catch (...) {
            error("%s: deserialize or I/O error for block header", __func__);
            throw InvalidVoteError();
        }

        // An output confirmed after the poll ends supplies no weight.
        if (AfterEnd(header)) {
            LogPrint(LogFlags::VOTE, "%s: txo confirmed after poll", __func__);
            return 0;
        }

        if (!IsInMainChain(header)) {
            LogPrint(LogFlags::VOTE, "%s: txo not in main chain", __func__);
            return 0;
        }

        if (fseek(file.Get(), tx_index.pos.nTxPos, SEEK_SET) != 0) {
            error("%s: tx fseek failed", __func__);
            throw InvalidVoteError();
        }

        CTransaction tx;

        try {
            file >> tx;
        } catch (...) {
            error("%s: deserialize or I/O error for tx", __func__);
            throw InvalidVoteError();
        }

        if (txo.n >= tx.vout.size()) {
            LogPrint(LogFlags::VOTE, "%s: txo out of range", __func__);
            throw InvalidVoteError();
        }

        const CTxOut& output = tx.vout[txo.n];

        if (output.nValue < COIN) {
            LogPrint(LogFlags::VOTE, "%s: txo < 1 GRC", __func__);
            throw InvalidVoteError();
        }

        CTxDestination dest;

        if (!ExtractDestination(output.scriptPubKey, dest)) {
            LogPrint(LogFlags::VOTE, "%s: invalid txo address", __func__);
            throw InvalidVoteError();
        }

        if (dest != address) {
            LogPrint(LogFlags::VOTE, "%s: txo address mismatch", __func__);
            throw InvalidVoteError();
        }

        if (txo.n >= tx_index.vSpent.size()) {
            error("%s: txo out of spent range", __func__);
            throw InvalidVoteError(); // should never happen
        }

        if (counted) {
            m_reached_counted = true;
            return 0;
        }

        m_vote_txos.emplace(txo);

        if (tx_index.vSpent[txo.n].IsNull()) {
            return output.nValue; // txo is unspent
        }

        return ResolveAmount(address, tx_index.vSpent[txo.n], output.nValue);
    }

    //!
    //! \brief Resolve the claimed amount for an output.
    //!
    //! This method determines whether a claimed output is spent. A spent
    //! output supplies no weight for the claim but the voting system can
    //! determine whether a claimed output is spent only for staking. For
    //! staked outputs, the voting system determines the voting weight by
    //! the value of the outputs in the latest staking transaction during
    //! the poll window as long as the transaction outputs the subsidy to
    //! the same address. Outputs spent or side-staked to another address
    //! will invalidate the claim for that amount. For polls that start at or
    //! after the v15 height, a vote transaction passes an output through to
    //! its outputs at the same address in the same way.
    //!
    //! \param address  Must match the address of the resolved output.
    //! \param disk_pos Location of the transaction that contains the output.
    //! \param amount   Original amount of the claimed UTXO.
    //!
    //! \return Claimed amount in units of 1/100000000 GRC. Returns zero when
    //! the output is spent.
    //!
    //! \throws InvalidVoteError If the vote fails to validate or if an IO
    //! error occurs.
    //!
    CAmount ResolveAmount(
        const CTxDestination& address,
        const CDiskTxPos& disk_pos,
        CAmount amount)
    {
        // Although this routine could make a nice recursive algorithm, we need
        // to ensure that it won't overflow the stack. Instead, we implement an
        // iterative approach using a queue-like model to walk the chain of the
        // transactions that staked the original output until we determine that
        // an amount remains unspent for the voting claim.
        //
        struct TxoFrame
        {
            CDiskTxPos m_pos;
            CAmount m_amount;

            TxoFrame(const CDiskTxPos pos, CAmount amount)
                : m_pos(pos), m_amount(amount)
            {
            }
        };

        std::queue<TxoFrame> txo_queue;
        txo_queue.emplace(disk_pos, amount);

        while (!txo_queue.empty()) {
            const TxoFrame frame = txo_queue.front();
            txo_queue.pop();

            Spender& spender = ReadSpender(frame.m_pos);

            if (spender.m_after_end) {
                continue; // Txo spent after the poll finished is irrelevant
            }

            // If we get here, the transaction spends the output referenced
            // by the vote claim within the voting window of the poll. When
            // we can determine that the transaction spends the output as a
            // staking transaction, we give the vote the reward amount from
            // the outputs of the stake if the transaction pays to the same
            // address. This supports multiple outputs from the stake input
            // (stake-splitting). We cannot consider outputs to a different
            // address (side-staking) because the vote is signed by the key
            // for the original address.
            //
            // From v15 a vote transaction passes the output through the same
            // way. A vote pays its burn fee from one of the voter's coins,
            // which its own claim usually includes, and returns the rest to
            // that coin's address. Any poll's vote counts, so voting
            // on one poll does not take the coin's weight from another. One
            // vote's walk passes through at most MAX_WALKED_VOTE_TXS vote
            // transactions; past that, a vote transaction spends the output
            // like any other. So does a vote transaction whose block is below
            // the poll's block.
            //
            amount -= frame.m_amount;

            const bool vote_tx = !spender.m_coinstake && m_v15_rules && spender.m_carries_vote
                && !spender.m_before_start;

            if (!spender.m_coinstake && !vote_tx) {
                continue; // This tx spends the txo but did not stake it
            }

            const uint256& tx_hash = spender.m_hash;

            if (vote_tx && m_vote_walked.count(tx_hash) == 0) {
                if (m_vote_walked.size() >= MAX_WALKED_VOTE_TXS) {
                    continue;
                }

                m_vote_walked.insert(tx_hash);
            }

            // Intermediate mapping of output offsets to amounts:
            std::vector<std::pair<uint32_t, CAmount>> next_txos;

            // Find outputs for the same address:
            for (uint32_t i = 0; i < spender.m_outputs.size(); ++i) {
                if (spender.m_outputs[i].first != address) {
                    continue;
                }

                if (Seen(COutPoint(tx_hash, i))) {
                    continue;
                }

                amount = AddClaimedAmount(amount, spender.m_outputs[i].second);
                next_txos.emplace_back(i, spender.m_outputs[i].second);
            }

            if (next_txos.empty()) {
                continue; // Tx stakes the entire txo to different addresses
            }

            if (!spender.m_spent) {
                CTxIndex tx_index;

                if (!m_txdb.ReadTxIndex(tx_hash, tx_index)) {
                    error("%s: failed to read next tx index", __func__);
                    throw InvalidVoteError();
                }

                spender.m_spent = std::move(tx_index.vSpent);
            }

            for (auto& next_pair : next_txos) {
                if (next_pair.first >= spender.m_spent->size()) {
                    error("%s: txo out of spent range", __func__);
                    throw InvalidVoteError(); // should never happen
                }

                const CDiskTxPos& pos = (*spender.m_spent)[next_pair.first];

                m_vote_txos.emplace(tx_hash, next_pair.first);

                if (!pos.IsNull()) {
                    txo_queue.emplace(pos, next_pair.second);
                }
            }
        }

        return amount;
    }

    //!
    //! \brief Add to a claimed amount, rejecting the vote if the sum does not fit
    //! in a CAmount.
    //!
    //! \throws InvalidVoteError If the sum does not fit in a CAmount.
    //!
    static CAmount AddClaimedAmount(const CAmount amount, const CAmount addend)
    {
        const std::optional<CAmount> sum = CheckedAdd(amount, addend);

        if (!sum) {
            LogPrint(LogFlags::VOTE, "%s: claimed amount overflows", __func__);
            throw InvalidVoteError();
        }

        return *sum;
    }

    //!
    //! \brief Determine whether the specified block exists in the main chain.
    //!
    //! \param header Block header to look up in the chain index.
    //!
    //! \return \c true if the main chain contains the specified block.
    //!
    static bool IsInMainChain(const CBlockHeader& header)
    {
        LOCK(cs_main);

        const auto iter = mapBlockIndex.find(header.GetHash());

        if (iter == mapBlockIndex.end()) {
            return false;
        }

        const CBlockIndex* const pindex = iter->second;

        return pindex && pindex->IsInMainChain();
    }

    //!
    //! \brief Get a file handle to the block file that contains the specified
    //! transaction position.
    //!
    static FILE* OpenBlockFile(const CDiskTxPos& pos)
    {
        return ::OpenBlockFile(pos.nFile, pos.nBlockPos, "rb");
    }
}; // VoteResolver

//!
//! \brief Manages state for counting legacy votes.
//!
class LegacyVoteCounterContext
{
public:
    //!
    //! \brief Initialize a legacy vote counter context.
    //!
    //! \param poll         The poll to count votes for.
    //! \param money_supply The money supply of the poll's last block, for the
    //! legacy magnitude factor (see GetMagnitudeFactor), or 0 for a poll that
    //! does not use the factor.
    //! \param superblock   The last well-formed superblock at or before the
    //! poll's last block, for the legacy magnitude factor, or none: where no
    //! superblock exists at or before that block, and for a poll that does not
    //! use the factor. The caller resolves both under cs_main, so the tally
    //! itself takes no lock.
    //!
    LegacyVoteCounterContext(const Poll& poll,
                             const CAmount money_supply,
                             std::optional<SuperblockPtr> superblock)
        : m_poll(poll)
        , m_magnitude_factor(std::nullopt)
        , m_money_supply(money_supply)
        , m_superblock(std::move(superblock))
    {
    }

    //!
    //! \brief Get a map that associates legacy vote response to the offset of
    //! the choices in the poll.
    //!
    const LegacyChoiceMap& GetChoices()
    {
        if (m_legacy_choices_cache.empty()) {
            uint8_t m_poll_choices_size = 0;

            // This silences the code scan warning of a possible infinite loop due to the overflow of the uint8_t i counter
            // against the comparison to m_poll.Choices.size(). Note it is the logged equivalent to std::min<uint8_t>. Since
            // this is legacy polls only and those are no longer issued, and this condition does not actually exist in the
            // chain, this is purely for formality's sake.
            if (m_poll.Choices().size() > (size_t) std::numeric_limits<uint8_t>::max()) {
                LogPrintf("WARN: %s: Number of legacy poll choices exceeds bins available in m_legacy_choices_cache map. "
                          "Limiting to %u.", __func__, std::numeric_limits<uint8_t>::max());
                m_poll_choices_size = std::numeric_limits<uint8_t>::max();
            } else {
                m_poll_choices_size = m_poll.Choices().size();
            }

            for (uint8_t i = 0; i < m_poll_choices_size; ++i) {
                std::string label = m_poll.Choices().At(i)->m_label;
                label = ToLower(label);

                m_legacy_choices_cache.emplace(std::move(label), i);
            }
        }

        return m_legacy_choices_cache;
    }

    //!
    //! \brief Get the factor used to calculate magnitude weight for legacy
    //! votes.
    //!
    //! https://github.com/gridcoin-community/Gridcoin-Research/issues/87
    //!
    //! The factor takes the money supply of the poll's last block and the last
    //! well-formed superblock at or before it, so a legacy poll's result stays
    //! as it was when the poll ended. Where no superblock exists at or before
    //! that block, the factor is zero and the votes carry no magnitude weight;
    //! the current superblock never serves.
    //!
    Weight GetMagnitudeFactor()
    {
        if (m_magnitude_factor) {
            return *m_magnitude_factor;
        }

        if (!m_superblock) {
            m_magnitude_factor = 0;

            return 0;
        }

        const uint32_t total_mag = (*m_superblock)->m_cpids.TotalMagnitude();
        const uint64_t supply = m_money_supply;

        // Legacy money supply factor calculations added 0.01 to the total
        // magnitude as a lazy way to prevent a division-by-zero error. We
        // retain this silliness to produce the same value that the legacy
        // voting system returned:
        //
        m_magnitude_factor = static_cast<Weight>(supply / (total_mag + 0.01) / 5.67);

        return *m_magnitude_factor;
    }

    //!
    //! \brief Determine whether a vote for the specified key has been counted.
    //!
    //! \param key Legacy key of the vote contract.
    //!
    //! \return \c true If the vote key was already counted.
    //!
    bool SeenKey(const std::string& key) const
    {
        return m_seen_keys.find(key) != m_seen_keys.end();
    }

    //!
    //! \brief Mark a vote contract key as already counted.
    //!
    //! \param key Legacy key of the vote contract.
    //!
    void RememberKey(std::string key)
    {
        m_seen_keys.emplace(std::move(key));
    }

private:
    const Poll& m_poll;
    LegacyChoiceMap m_legacy_choices_cache;
    std::optional<Weight> m_magnitude_factor; //!< Once computed (zero with no superblock).
    std::set<std::string> m_seen_keys;

    //! Money supply of the poll's last block, for the magnitude factor, or 0
    //! for a poll that does not use the factor.
    CAmount m_money_supply;

    //! Last well-formed superblock at or before the poll's last block, for the
    //! magnitude factor, or none.
    std::optional<SuperblockPtr> m_superblock;
}; // LegacyVoteCounterContext

//!
//! \brief Validates votes and tallies the responses to a poll.
//!
class VoteCounter
{
public:
    //!
    //! \brief Initialize a vote counter for the specified poll.
    //!
    //! \param txdb Used to fetch vote contracts from disk.
    //! \param poll Poll to count votes for.
    //! \param poll_start_height Block height of the poll's starting block,
    //! used to anchor consensus-critical PoolRegistry queries (AVW) so
    //! every node tallying the same poll uses an identical registry view.
    //! \param v15_rules Whether the poll started at or after the v15 height;
    //! forwarded to the vote resolver.
    //! \param legacy_money_supply Money supply of the poll's last block,
    //! forwarded to the legacy vote counter for its magnitude factor, or 0
    //! for a poll that does not use the factor.
    //! \param legacy_superblock Last well-formed superblock at or before the
    //! poll's last block, or none, forwarded to the legacy vote counter for its
    //! magnitude factor.
    //! \param pindex_bound The block that bounds the tally by blocks (PollSnapshot::m_pindex_bound), or nullptr.
    //!
    VoteCounter(CTxDB& txdb, const Poll& poll, int poll_start_height, const bool v15_rules,
                const CBlockIndex* pindex_bound, const CAmount legacy_money_supply,
                std::optional<SuperblockPtr> legacy_superblock)
        : m_txdb(txdb)
        , m_poll(poll)
        , m_poll_start_height(poll_start_height)
        , m_v15_rules(v15_rules)
        , m_resolver(txdb, poll, v15_rules, pindex_bound, poll_start_height)
        , m_legacy(poll, legacy_money_supply, std::move(legacy_superblock))
    {
        // Snapshot the pool CPID set at the poll's start height into an
        // unordered_set for O(1) membership in ProcessVote. Single
        // chain-walk for the whole poll instead of one per vote.
        for (const GRC::Pool& pool : GRC::GetPoolRegistry().ActivePoolsAtHeight(m_poll_start_height)) {
            m_pool_cpids_at_start.insert(pool.m_cpid);
        }
    }

    //!
    //! \brief Initialize the vote counter for magnitude weight calculation.
    //!
    //! For polls that account for magnitude in voting weight, we need to
    //! determine the amount of weight to award to each unit of magnitude
    //! claimed by a vote. Since voting weight is represented as GRC, the
    //! magnitude weight is derived from the money supply of the network:
    //!
    //!   base_weight_per_magnitude = network_money_supply / total_magnitude
    //!
    //! The above will represent the sum of the votes with magnitude with
    //! as much weight as the sum of the votes with a GRC balance. A poll
    //! elected that the voting system rebalance voting weight so that it
    //! allocates less to magnitude votes. It established a ratio of 5.67
    //! to 1 for balance to magnitude weight:
    //!
    //!   weight_per_magnitude = (1 / 5.67) * base_weight_per_magnitude
    //!
    //! See: https://github.com/gridcoin-community/Gridcoin-Research/issues/87
    //!
    //! \param superblock The latest superblock in the poll window.
    //! \param supply     The total network money supply as of the latest
    //! block in the poll window in units of 1/100000000 GRC.
    //!
    void EnableMagnitudeWeight(SuperblockPtr superblock, const uint64_t supply)
    {
        const uint64_t total_mag = superblock->m_cpids.TotalMagnitude();

        if (total_mag <= 0) {
            return;
        }

        // Use integer arithmetic to avoid floating-point discrepancies:

        // Overflow analysis. This is uint64_t arithmetic and wraps without
        // notice, so the bound has to come from the inputs.
        //
        // supply is in units of 1/100000000 GRC (see \param supply), and
        // supply / total_mag is multiplied by the weight factor's numerator
        // before the division. At block 4114587 (supply 540,405,134 GRC, total
        // magnitude 114,987) that leaves room for a numerator of 39,250,843.
        //
        // From block version 15 the protocol registry refuses a
        // "magnitudeweightfactor" entry whose reduced numerator or denominator
        // exceeds MAX_MAGNITUDE_ENTRY_TERM (10^6), and every entry made through
        // 2026-10-20 is inside that bound. The worst case is supply at MAX_MONEY over
        // a total magnitude of 32,767, one participant at the magnitude cap with
        // one active project: total magnitude is essentially fixed at 115,000
        // and falls below it only through CPIDs at the cap. That product is
        // 6.10e18, 3.02 times under 2^64, and the static_assert holds it. See
        // the analysis at MAX_MAGNITUDE_ENTRY_TERM in protocol.h.
        static_assert(static_cast<uint64_t>(MAX_MONEY) / Magnitude::MAX
                          <= std::numeric_limits<uint64_t>::max() / MAX_MAGNITUDE_ENTRY_TERM,
                      "the vote weight per magnitude must fit uint64_t at the weight factor entry bound");
        m_magnitude_factor = supply / total_mag
                             * (uint64_t) m_poll.m_magnitude_weight_factor.GetNumerator()
                             / (uint64_t) m_poll.m_magnitude_weight_factor.GetDenominator();
        m_resolver.SetSuperblock(std::move(superblock));
    }

    //!
    //! \brief Tally the supplied votes for the poll result.
    //!
    //! \param result     The result object to count votes for.
    //! \param vote_txids Hashes of the transactions that contain the vote
    //! contracts to apply to the poll result in the order that the blocks
    //! and transactions that contain the votes were connected.
    //!
    void CountVotes(PollResult& result, const std::vector<uint256>& vote_txids)
    {
        g_timer.GetTimes(std::string{"Begin "} + std::string{__func__}, "buildPollTable");

        m_votes.reserve(vote_txids.size());
        result.m_vote_count = vote_txids.size();

        for (const auto& txid : reverse_iterate(vote_txids)) {
            try {
                ProcessVoteCandidate(FetchVoteCandidate(txid));
            } catch (const SupersededVoteError& e) {
                LogPrint(LogFlags::VOTE, "INFO: %s: skipped superseded vote: %s",
                    __func__,
                    txid.ToString());

                ++result.m_superseded_votes;
            } catch (const InvalidVoteError& e) {
                LogPrint(LogFlags::VOTE, "INFO: %s: skipped invalid vote: %s",
                    __func__,
                    txid.ToString());

                ++result.m_invalid_votes;
            } catch (const InvalidDuetoReorgFork& e) {
                LogPrint(LogFlags::VOTE, " INFO: %s: aborted vote count due to reorg/fork",
                         __func__);

                g_timer.GetTimes(std::string{"End "} + std::string{__func__}, "buildPollTable");
                throw InvalidDuetoReorgFork();
            }
        }

        for (auto& vote : m_votes) {
            result.TallyVote(std::move(vote));
        }

        result.m_pools_voted = m_pools_voted;

        g_timer.GetTimes(std::string{"End "} + std::string{__func__}, "buildPollTable");
    }

private:
    CTxDB& m_txdb;
    const Poll& m_poll;
    int m_poll_start_height; //!< Anchor for ActivePoolsAtHeight queries — see ctor.
    const bool m_v15_rules; //!< The poll's block is at or above the v15 height.
    std::unordered_set<Cpid> m_pool_cpids_at_start; //!< Cached pool CPIDs as of m_poll_start_height.
    std::vector<VoteDetail> m_votes;
    std::vector<Cpid> m_pools_voted;
    Weight m_magnitude_factor {0}; //!< Zero unless EnableMagnitudeWeight sets it.
    Weight m_counted_weight {0};     //!< Total weight of the votes accepted so far.
    Weight m_counted_own_amount {0}; //!< Total balance of the wallet's own votes accepted so far.
    VoteResolver m_resolver;
    LegacyVoteCounterContext m_legacy;

    //!
    //! \brief Read a vote contract from disk for the specified transaction.
    //!
    //! TODO: The protocol only allows one contract per transaction. If this
    //! changes, we need to update this class to process all of the votes in
    //! each transaction.
    //!
    //! \param txid Hash of the transaction that contains the vote to load
    //! from disk.
    //!
    //! \return The vote contract contained in the specified transaction.
    //!
    //! \throws InvalidVoteError When the referenced transaction does not
    //! contain a well-formed vote contract.
    //!
    VoteCandidate FetchVoteCandidate(const uint256 txid)
    {
        CTransaction tx;
        CTxIndex tx_index;

        {
            // This lock is taken here to ensure that we wait on the leveldb batch write ("transaction commit") to finish
            // in ReorganizeChain (which is essentially the ConnectBlock scope) and ensure that the voting transactions
            // which correspond to the new vote signals sent from the contract handlers are actually present in leveldb when
            // the below ReadDiskTx is called.
            LOCK(cs_tx_val_commit_to_disk);
            LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: cs_tx_val_commit_to_disk locked", __func__);

            if (!m_txdb.ReadDiskTx(txid, tx, tx_index)) {
                LogPrintf("WARN: %s: failed to read vote tx.", __func__);
            }

            LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: cs_tx_val_commit_to_disk unlocked", __func__);
        }

        // From v15 a poll reads no vote transaction time: the registry linked each of its votes by the vote's block,
        // inside the poll's window, and the tally does not repeat that judgment. Any other poll keeps the
        // #3470 rule: the vote's transaction time must be in the window.
        if (!m_v15_rules) {
            if (tx.nTime < m_poll.m_timestamp) {
                LogPrintf("WARN: %s: tx earlier than poll", __func__);
                throw InvalidVoteError();
            }

            if (m_poll.Expired(tx.nTime)) {
                LogPrintf("WARN: %s: tx exceeds expiration", __func__);
                throw InvalidVoteError();
            }
        }

        for (auto& contract : tx.GetContracts()) {
            if (contract.m_type != ContractType::VOTE) {
                continue;
            }

            if (!contract.WellFormed()) {
                LogPrintf("WARN: %s: skipped bad contract", __func__);
                continue;
            }

            return VoteCandidate(contract, std::move(tx), tx_index.pos);
        }

        LogPrint(LogFlags::VOTE, "%s: tx has no vote contract", __func__);
        throw InvalidVoteError();
    }

    //!
    //! \brief Tally a vote candidate.
    //!
    //! \param candidate Contains the vote contract to resolve.
    //!
    void ProcessVoteCandidate(const VoteCandidate& candidate)
    {
        if (GetPollRegistry().reorg_occurred_during_reg_traversal) {
            throw InvalidDuetoReorgFork();
        }

        if (!candidate.IsLegacy()) {
            ProcessVote(candidate);
            return;
        }

        // Unlike binary-format votes in version 2 contracts, legacy votes are
        // not checked for correct form along with the contract:
        //
        if (!candidate.LegacyVote().WellFormed()) {
            LogPrint(LogFlags::VOTE, "%s: skipped malformed vote", __func__);
            throw InvalidVoteError();
        }

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"
#endif
        ProcessLegacyVote(candidate.LegacyVote());
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    }

    //!
    //! \brief Tally a vote candidate.
    //!
    //! \param candidate Contains the vote contract to resolve.
    //!
    void ProcessVote(const VoteCandidate& candidate)
    {
        g_timer.GetTimes(std::string{"Begin "} + std::string{__func__}, "buildPollTable");

        VoteDetail detail = m_resolver.Resolve(candidate);

        if (detail.Empty()) {
            if (m_resolver.ReachedCounted()) {
                LogPrint(LogFlags::VOTE, "%s: resolved superseded vote", __func__);
                throw SupersededVoteError();
            }

            LogPrint(LogFlags::VOTE, "%s: resolved empty vote", __func__);
            throw InvalidVoteError();
        }

        for (const auto choice_offset : candidate.Vote().m_responses) {
            if (m_poll.Choices().OffsetInRange(choice_offset)) {
                detail.m_responses.emplace_back(choice_offset, 0.0);
            } else {
                LogPrint(LogFlags::VOTE, "%s: invalid response", __func__);
                throw InvalidVoteError();
            }
        }

        CalculateWeight(detail, m_magnitude_factor);
        AcceptIntoTotals(detail);
        m_resolver.Commit();

        // Record if a pool votes. Membership check is O(1) via the cached
        // m_pool_cpids_at_start set. See VoteCounter ctor for the
        // height-anchored snapshot rationale.
        if (const auto cpid_opt = detail.m_mining_id.TryCpid()) {
            if (m_pool_cpids_at_start.find(*cpid_opt) != m_pool_cpids_at_start.end()) {
                LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: Pool with CPID %s voted on poll %s.",
                         __func__,
                         cpid_opt->ToString(),
                         m_poll.m_title);

                m_pools_voted.push_back(*cpid_opt);
            }
        }

        m_votes.emplace_back(std::move(detail));

        g_timer.GetTimes(std::string{"End "} + std::string{__func__}, "buildPollTable");
    }

    //!
    //! \brief Tally a legacy vote candidate.
    //!
    //! \param vote Contains the vote contract to resolve.
    //!
    void ProcessLegacyVote(const LegacyVote& vote) EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
    {
        if (m_legacy.SeenKey(vote.m_key)) {
            LogPrint(LogFlags::VOTE, "%s: skipped superseded legacy vote", __func__);
            throw SupersededVoteError();
        }

        VoteDetail detail;
        detail.m_responses = vote.ParseResponses(m_legacy.GetChoices());

        if (detail.m_responses.empty()) {
            LogPrint(LogFlags::VOTE, "%s: no valid response", __func__);
            throw InvalidVoteError();
        }

        const std::optional<PollResult::Weight> amount = LegacyVoteAmount(vote.m_amount);

        if (!amount) {
            LogPrint(LogFlags::VOTE, "%s: balance out of range", __func__);
            throw InvalidVoteError();
        }

        detail.m_amount = *amount;
        detail.m_mining_id = vote.m_mining_id;
        detail.m_magnitude = Magnitude::RoundFrom(vote.m_magnitude);

        CalculateWeight(detail, m_legacy.GetMagnitudeFactor());
        AcceptIntoTotals(detail);

        m_votes.emplace_back(std::move(detail));
        m_legacy.RememberKey(vote.m_key);
    }

    //!
    //! \brief Accept a vote's weight into the poll's running totals, or reject the
    //! vote.
    //!
    //! PollResult::TallyVote refuses a vote that would carry a reported total past
    //! what a CAmount holds. The counter applies the same check as it accepts each
    //! vote.
    //!
    //! \throws InvalidVoteError If the vote would carry a total past the limit.
    //!
    void AcceptIntoTotals(const VoteDetail& detail)
    {
        constexpr Weight max_weight = std::numeric_limits<CAmount>::max();

        Weight vote_weight = 0;

        for (const auto& response_pair : detail.m_responses) {
            if (response_pair.second > max_weight - vote_weight) {
                LogPrint(LogFlags::VOTE, "%s: vote weight overflows", __func__);
                throw InvalidVoteError();
            }

            vote_weight += response_pair.second;
        }

        const bool own = detail.m_ismine != ISMINE_NO;

        if (vote_weight > max_weight - m_counted_weight
            || (own && detail.m_amount > max_weight - m_counted_own_amount))
        {
            LogPrint(LogFlags::VOTE, "%s: poll total overflows", __func__);
            throw InvalidVoteError();
        }

        m_counted_weight += vote_weight;

        if (own) {
            m_counted_own_amount += detail.m_amount;
        }
    }

    //!
    //! \brief Calculate and apply the response weight for the supplied vote.
    //!
    //! \param detail           Vote to calculate the response weight for.
    //! \param magnitude_factor Weight per unit of magnitude.
    //!
    void CalculateWeight(VoteDetail& detail, const Weight magnitude_factor) const
    {
        const size_t response_count = detail.m_responses.size();

        if (response_count == 0) {
            return;
        }

        Weight response_weight = 0;

        switch (m_poll.m_weight_type.Value()) {
            case PollWeightType::MAGNITUDE:
                response_weight = detail.m_magnitude.Floating() / response_count;
                break;

            case PollWeightType::BALANCE:
                response_weight = detail.m_amount / response_count;
                break;

            case PollWeightType::BALANCE_AND_MAGNITUDE: {
                // Checked: a product or sum that does not fit in a Weight makes
                // the vote invalid.
                const Weight scaled_magnitude = detail.m_magnitude.Scaled();

                if (scaled_magnitude != 0 && magnitude_factor > std::numeric_limits<Weight>::max() / scaled_magnitude) {
                    LogPrint(LogFlags::VOTE, "%s: magnitude weight overflows", __func__);
                    throw InvalidVoteError();
                }

                const Weight mag_weight = magnitude_factor * scaled_magnitude / Magnitude::SCALE_FACTOR;
                const std::optional<Weight> weight = CheckedAdd(mag_weight, detail.m_amount);

                if (!weight) {
                    LogPrint(LogFlags::VOTE, "%s: vote weight overflows", __func__);
                    throw InvalidVoteError();
                }

                response_weight = *weight / response_count;
                break;
            }

            case PollWeightType::CPID_COUNT:
                response_weight = detail.m_magnitude.Scaled() > 0;
                break;

            case PollWeightType::PARTICIPANT_COUNT:
                response_weight = 1 * COIN;
                break;

            case PollWeightType::UNKNOWN:
            case PollWeightType::OUT_OF_BOUND:
                break; // Suppress warning
        }

        for (auto& response_pair : detail.m_responses) {
            response_pair.second = response_weight;
        }
    }
}; // VoteCounter

//!
//! \brief Get the last block in the poll window.
//!
//! \param poll Poll to find the block for.
//! \param pindex_tip Pinned chain tip to walk back from (in place of the live
//! pindexBest) so the result is consistent with the rest of the tally.
//!
//! \return The pinned tip while the poll is open, otherwise the last block at
//! or before the poll's end.
//!
const CBlockIndex* PollEndBlock(const Poll& poll, const CBlockIndex* pindex_tip)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    if (!poll.Expired(pindex_tip->nTime)) {
        return pindex_tip;
    }

    const CBlockIndex* pindex = pindex_tip;
    const int64_t poll_expiration = poll.Expiration();

    for (; pindex && pindex->nTime > poll_expiration; pindex = pindex->pprev);

    return pindex;
}

} // Anonymous namespace

// -----------------------------------------------------------------------------
// Global Functions
// -----------------------------------------------------------------------------

//!
//! \brief Fetch the superblock used to calculate magnitude weight for the
//! specified poll.
//!
//! \param poll Poll to fetch the superblock for.
//! \param pindex_tip Pinned chain tip to walk back from (in place of the live
//! pindexBest) so the result is consistent with the rest of the tally.
//!
//! \return Latest superblock active during the poll.
//!
//! The live superblock serves only when it is on the pinned tip's own chain.
//!
SuperblockPtr GRC::ResolveSuperblockForPoll(const Poll& poll, const CBlockIndex* pindex_tip)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    SuperblockPtr superblock = Quorum::CurrentSuperblock();
    const CBlockIndex* pindex = pindex_tip;

    // The live current superblock belongs to the live tip, and a table build tallies each poll on the tip it pinned.
    // The live superblock is on the pinned tip's own chain only when the pinned tip is still on the main chain and the
    // superblock is at or below it (both are then main chain blocks, so the superblock is the tip's ancestor). Only
    // then may it serve, or be skipped past; otherwise the walk below reads only the pinned tip's chain. The result is
    // the one for the pinned tip.
    if (pindex_tip->IsInMainChain() && superblock.m_height <= pindex_tip->nHeight) {
        if (!poll.Expired(superblock.m_timestamp)) {
            return superblock;
        }

        // Seek past the current superblock:
        for (; pindex && pindex->nHeight >= superblock.m_height; pindex = pindex->pprev);
    }

    // Find the superblock active at the end of the poll:
    for (; pindex; pindex = pindex->pprev) {
        if (!pindex->IsSuperblock() || poll.Expired(pindex->nTime)) {
            continue;
        }

        superblock = SuperblockPtr::ReadFromDisk(pindex);
        break;
    }

    return superblock;
}

bool GRC::MeetsMinimumVotePercentAVW(
    const PollResult::Weight total_weight,
    const CAmount active_vote_weight,
    const uint32_t min_vote_percent)
{
    if (active_vote_weight <= 0) {
        return false;
    }

    // For a whole min_vote_percent and a positive active vote weight,
    // floor(100 * W / A) >= min  <=>  100 * W >= min * A.
    return arith_uint256(total_weight) * 100
        >= arith_uint256(min_vote_percent) * arith_uint256(static_cast<uint64_t>(active_vote_weight));
}

std::optional<GRC::PollResult::Weight> GRC::LegacyVoteAmount(const double amount)
{
    static_assert(std::is_same<PollResult::Weight, uint64_t>::value, "the bound below is 2^64");

    const double units = amount * COIN;

    // Converting a double to an integer truncates it toward zero, and is undefined
    // when the truncated value is outside the integer type: here, at or below -1,
    // or at or above 2^64. 2^64 is exact as a double, and the comparisons are
    // written so that a NaN fails them.
    if (!(units > -1.0 && units < 18446744073709551616.0)) {
        return std::nullopt;
    }

    return static_cast<PollResult::Weight>(units);
}

ClaimMessage GRC::PackVoteMessage(const Vote& vote, const CTransaction& tx)
{
    std::vector<uint8_t> bytes;
    CVectorWriter writer(SER_NETWORK, PROTOCOL_VERSION, bytes, 0);

    writer << vote.m_poll_txid;
    writer << vote.m_responses;
    writer << tx.nTime;

    for (const auto& txin : tx.vin) {
        writer << txin.prevout;
    }

    return bytes;
}

// -----------------------------------------------------------------------------
// Class: PollResult
// -----------------------------------------------------------------------------

PollResult::PollResult(Poll poll)
    : m_poll(std::move(poll))
    , m_total_weight(0)
    , m_invalid_votes(0)
    , m_superseded_votes(0)
    , m_vote_count(0)
    , m_pools_voted({})
    , m_active_vote_weight()
    , m_vote_percent_avw()
    , m_poll_results_validated()
    , m_finished(false)
    , m_self_voted(false)
    , m_self_vote_detail()
{
    m_responses.resize(m_poll.Choices().size());
}

PollResultOption PollResult::BuildFor(const PollReference& poll_ref, const CBlockIndex* pindex_tip)
{
    PollSnapshot snapshot;

    {
        LOCK2(cs_main, PollRegistry::cs_poll_registry);
        snapshot = GetPollRegistry().Snapshot(poll_ref, pindex_tip);
    }

    return BuildFor(snapshot);
}

PollResultOption PollResult::BuildFor(const PollSnapshot& snapshot)
{
    // Create the timer HERE, at the engine's outermost entry, not in a caller.
    //
    // Every "buildPollTable" read in this file -- SetSuperblock, CountVotes,
    // ProcessVote, Resolve and BuildFor itself -- is a phase of one resolution, and
    // they all run underneath this call. The label used to be created by
    // PollResultToJson() in rpc/voting.cpp, which reads it never and owns it only by
    // accident: the RPC is one caller among several. PollResultCache::BuildFor and
    // the GUI's voting page reach the same engine without it, so on a node serving
    // poll results the timer lookup missed on every phase of every poll and
    // MilliTimer warned each time -- dozens of lines per resolution at default log
    // level, since the warning is a LogPrintf rather than a category.
    //
    // Same log gate the RPC used, so the timings themselves are unchanged.
    g_timer.InitTimer("buildPollTable", LogInstance().WillLogCategory(BCLog::LogFlags::VOTE));

    g_timer.GetTimes(std::string{"Begin "} + std::string{__func__}, "buildPollTable");

    CTxDB txdb("r");
    PollOption poll = ReadPollFromDisk(txdb, snapshot.m_txid);

    if (!poll) {
        g_timer.GetTimes(std::string{"End "} + std::string{__func__}, "buildPollTable");

        return std::nullopt;
    }

    // Neither the timestamp nor the magnitude weight factor is serialized. The copy takes the poll's start from the
    // snapshot, which from v15 is its block's time; the tally's time rules and the displays read it. The
    // factor is resolved from the poll's block and start, as PollReference::GetMagnitudeWeightFactor resolves it, from
    // the snapshot's copies.
    poll->m_timestamp = snapshot.m_start_time;
    poll->m_magnitude_weight_factor =
        Poll::ResolveMagnitudeWeightFactor(snapshot.m_pindex_start, snapshot.m_start_time);

    PollResult result(std::move(*poll));
    const CBlockIndex* const pindex_tip = snapshot.m_pindex_tip;

    // Anchor consensus-critical pool queries (AVW) to the poll's start height. A skeleton reference (no block)
    // shouldn't reach here; if one does, the pinned tip's height keeps VoteCounter working, and the v15 rules,
    // which apply to polls whose block is at or above the v15 height, do not apply.
    const int poll_start_height = snapshot.m_pindex_start ? snapshot.m_pindex_start->nHeight : pindex_tip->nHeight;
    const bool v15_rules = snapshot.m_v15;
    const CBlockIndex* pindex_end = nullptr;
    CAmount legacy_money_supply = 0;
    std::optional<SuperblockPtr> legacy_superblock;
    {
        LOCK(cs_main);

        // The last block in the poll window gives the money supply for the
        // magnitude weight of a poll weighed by magnitude. A legacy poll
        // weighed by balance and magnitude also takes the last well-formed
        // superblock at or before that block for its legacy magnitude
        // factor; where none exists, the factor is zero, and the current
        // superblock never serves. Every other poll does not use
        // the factor. Resolved under the cs_main we already hold here, so the
        // tally itself stays lock-free.
        // A poll bounded by blocks reads the money supply at its bound (the closing block, or the pinned tip while
        // it is open); any other at the last block at or before its end.
        if (result.m_poll.IncludesMagnitudeWeight()) {
            pindex_end = snapshot.m_pindex_bound ? snapshot.m_pindex_bound : PollEndBlock(result.m_poll, pindex_tip);
        }

        if (pindex_end && snapshot.m_payload_version == 1
            && result.m_poll.m_weight_type == PollWeightType::BALANCE_AND_MAGNITUDE) {
            legacy_money_supply = pindex_end->nMoneySupply;
            legacy_superblock = LastWellFormedSuperblock(pindex_end);
        }
    }

    VoteCounter counter(txdb, result.m_poll, poll_start_height, v15_rules, snapshot.m_pindex_bound,
                        legacy_money_supply, std::move(legacy_superblock));

    if (result.m_poll.IncludesMagnitudeWeight()) {
        SuperblockPtr superblock;
        uint64_t supply;

        {
            LOCK(cs_main);

            // A poll bounded by blocks takes the last well-formed superblock at or before its bound, from the bound's
            // chain alone, or none; any other the superblock of its window.
            superblock = snapshot.m_pindex_bound
                ? LastWellFormedSuperblock(snapshot.m_pindex_bound).value_or(SuperblockPtr::Empty())
                : ResolveSuperblockForPoll(result.m_poll, pindex_tip);
            supply = pindex_end->nMoneySupply;
        }

        counter.EnableMagnitudeWeight(std::move(superblock), supply);
    }

    LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: number of votes = %u for poll %s",
             __func__, snapshot.m_votes.size(), result.m_poll.m_title);

    counter.CountVotes(result, snapshot.m_votes);

    LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: snapshot.m_start_time = %" PRId64 " poll.m_magnitude_weight_factor = %s",
             __func__,
             snapshot.m_start_time,
             result.m_poll.m_magnitude_weight_factor.ToString());

    if (auto active_vote_weight = ActiveVoteWeight(snapshot, result)) {
        result.m_active_vote_weight = active_vote_weight;

        // No percentage is computed without an active vote weight.
        if (*result.m_active_vote_weight > 0) {
            result.m_vote_percent_avw = (double) result.m_total_weight / (double) *result.m_active_vote_weight * 100.0;
        }

        // For v1 and v2 polls, there is only one type, SURVEY, that was used on the blockchain for all polls, so this
        // can only be done for v3+. Validation uses integer arithmetic: see MeetsMinimumVotePercentAVW.
        if (snapshot.m_payload_version > 2) {
            const uint32_t min_vote_percent_avw_for_validation
                    = Poll::POLL_TYPE_RULES[(int) snapshot.m_type].m_min_vote_percent_AVW;

            result.m_poll_results_validated = MeetsMinimumVotePercentAVW(
                result.m_total_weight,
                *result.m_active_vote_weight,
                min_vote_percent_avw_for_validation);
        }
    }

    result.m_finished = snapshot.m_finished;

    // The tally ran without cs_main, on the chain that ended at the pinned tip when the snapshot was taken. The
    // pinned tip is checked again before the result is returned, and the callers retry on the throw as they do on
    // the snapshot's own check.
    {
        LOCK(cs_main);

        if (snapshot.m_pindex_tip && !snapshot.m_pindex_tip->IsInMainChain()) {
            throw InvalidDuetoReorgFork();
        }
    }

    g_timer.GetTimes(std::string{"End "} + std::string{__func__}, "buildPollTable");

    return result;
}

size_t PollResult::Winner() const
{
    return std::distance(
        m_responses.begin(),
        std::max_element(m_responses.begin(), m_responses.end()));
}

const std::string& PollResult::WinnerLabel() const
{
    return m_poll.Choices().At(Winner())->m_label;
}

void PollResult::TallyVote(VoteDetail detail)
{
    if (detail.m_responses.empty()) {
        return;
    }

    // Every weight is reported through ValueFromAmount, so no total may exceed
    // what a CAmount holds. A vote that would carry a total past it is counted as
    // invalid. VoteCounter applies the same check; this one guards other callers.
    constexpr Weight max_weight = std::numeric_limits<CAmount>::max();

    Weight vote_weight = 0;

    for (const auto& response_pair : detail.m_responses) {
        if (response_pair.second > max_weight - vote_weight) {
            ++m_invalid_votes;
            return;
        }

        vote_weight += response_pair.second;
    }

    if (vote_weight > max_weight - m_total_weight
        || (detail.m_ismine != ISMINE_NO && detail.m_amount > max_weight - m_self_vote_detail.m_amount))
    {
        ++m_invalid_votes;
        return;
    }

    if (detail.m_ismine != ISMINE_NO) {
        // The counter tallies votes newest first, so the CPID and magnitude are
        // those of the wallet holder's newest counted vote.
        if (!m_self_voted) {
            m_self_vote_detail.m_mining_id = detail.m_mining_id;
            m_self_vote_detail.m_magnitude = detail.m_magnitude;
            m_self_vote_detail.m_ismine = detail.m_ismine;
        }

        m_self_voted = true;
        m_self_vote_detail.m_amount += detail.m_amount;
    }

    for (const auto& response_pair : detail.m_responses) {
        const uint8_t response_offset = response_pair.first;
        const Weight response_weight = response_pair.second;

        m_responses[response_offset].m_weight += response_weight;
        m_responses[response_offset].m_votes += 1.0 / detail.m_responses.size();
        m_total_weight += response_weight;

        if (detail.m_ismine != ISMINE_NO) {
            bool choice_found = false;

            // If the response offset entry already exists, then append to the existing entry...
            for (auto& choice : m_self_vote_detail.m_responses) {
                if (choice.first == response_offset) {
                    choice.second += response_weight;
                    choice_found = true;
                    break;
                }
            }

            // Otherwise make a new m_responses entry to represent the response offset and the associated weight.
            if (!choice_found) {
                m_self_vote_detail.m_responses.push_back(std::make_pair(response_offset, response_weight));
            }
        }
    }

    m_votes.emplace_back(std::move(detail));
}

// -----------------------------------------------------------------------------
// Class: PollResult::ResponseDetail
// -----------------------------------------------------------------------------

ResponseDetail::ResponseDetail() : m_weight(0), m_votes(0)
{
}

// -----------------------------------------------------------------------------
// Class: PollResult::VoteDetail
// -----------------------------------------------------------------------------

VoteDetail::VoteDetail() : m_amount(0), m_magnitude(Magnitude::Zero()), m_ismine(ISMINE_NO)
{
}

VoteDetail::VoteDetail(const VoteDetail &original_votedetail)
    : m_amount(original_votedetail.m_amount)
      , m_mining_id(original_votedetail.m_mining_id)
      , m_magnitude(original_votedetail.m_magnitude)
      , m_ismine(original_votedetail.m_ismine)
      , m_responses(original_votedetail.m_responses)
{
}

bool VoteDetail::Empty() const
{
    return m_amount == 0 && m_magnitude == 0;
}

VoteDetail& VoteDetail::operator=(const VoteDetail& b) = default;
