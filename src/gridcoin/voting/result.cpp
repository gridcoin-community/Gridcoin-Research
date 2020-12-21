// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "gridcoin/beacon.h"
#include "gridcoin/quorum.h"
#include "gridcoin/superblock.h"
#include "gridcoin/voting/registry.h"
#include "gridcoin/voting/result.h"
#include "gridcoin/voting/poll.h"
#include "gridcoin/voting/vote.h"
#include "txdb.h"
#include "util/reverse_iterator.h"

#include <boost/optional.hpp>
#include <queue>
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
//! \brief Hashes \c COutPoint objects to key a lookup table.
//!
struct OutPointHasher
{
    size_t operator()(const COutPoint& outpoint) const
    {
        return outpoint.hash.GetUint64() + outpoint.n;
    }
};

//!
//! \brief Contains an unprocesseed vote contract for a poll extracted from a
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
    //!
    VoteCandidate(const Contract& contract, CTransaction tx)
        : m_tx(std::move(tx))
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

private:
    const CTransaction m_tx;         //!< Transaction that contains a vote.
    const Contract& m_contract;      //!< The vote contract.
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
    //! \param txdb Used to resolve balance claim amounts.
    //! \param poll The poll to resolve voting weight for.
    //!
    VoteResolver(CTxDB& txdb, const Poll& poll)
        : m_txdb(txdb)
        , m_poll(poll)
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
        const Vote& vote = candidate.Vote();
        const ClaimMessage message = candidate.PackMessage();

        VoteDetail detail;
        detail.m_amount = Resolve(vote.m_claim.m_balance_claim, message);

        if (m_poll.IncludesMagnitudeWeight()) {
            detail.m_mining_id = vote.m_claim.m_magnitude_claim.m_mining_id;
            detail.m_magnitude = Resolve(vote.m_claim.m_magnitude_claim, message);
        }

        return detail;
    }

private:
    CTxDB& m_txdb;              //!< Used to resolve unspent amount claims.
    const Poll& m_poll;         //!< The poll to resolve voting weight for.
    SuperblockPtr m_superblock; //!< Used to determine magnitude weight.

    //!
    //! \brief Remembers the outputs claimed by a vote contract for balance
    //! claims to filter duplicate weight claims.
    //!
    std::unordered_set<COutPoint, OutPointHasher> m_seen_txos;

    //!
    //! \brief Remembers the CPID claimed by a vote contract for magnitude
    //! claims to filter duplicate weight claims.
    //!
    std::unordered_set<Cpid> m_seen_cpids;

    //!
    //! \brief Resolve the claimed magnitude for a vote.
    //!
    //! \param claim   The claim to resolve a CPID's magnitude for.
    //! \param message Serialized context for vote signature verification.
    //!
    //! \return Magnitude as of the latest superblock in the poll window.
    //!
    Magnitude Resolve(const MagnitudeClaim& claim, const ClaimMessage& message)
    {
        const CpidOption cpid = claim.m_mining_id.TryCpid();

        if (!cpid) {
            return Magnitude::Zero();
        }

        if (m_seen_cpids.find(*cpid) != m_seen_cpids.end()) {
            return Magnitude::Zero();
        }

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

        if (m_poll.Expired(header.nTime)) {
            LogPrint(LogFlags::VOTE, "%s: beacon confirmed after poll", __func__);
            throw InvalidVoteError();
        }

        if (!IsInMainChain(header)) {
            LogPrint(LogFlags::VOTE, "%s: beacon not in main chain", __func__);
            return Magnitude::Zero();
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

            m_seen_cpids.emplace(*cpid);

            return m_superblock->m_cpids.MagnitudeOf(*cpid);
        }

        return Magnitude::Zero();
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
            amount += Resolve(address_claim, message);
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
            amount += Resolve(txo, address);
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
        if (m_seen_txos.find(txo) != m_seen_txos.end()) {
            LogPrint(LogFlags::VOTE, "%s: duplicate txo", __func__);
            return 0;
        }

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

        if (m_poll.Expired(header.nTime)) {
            LogPrint(LogFlags::VOTE, "%s: txo confirmed after poll", __func__);
            throw InvalidVoteError();
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

        if (tx_index.vSpent[txo.n].IsNull()) {
            m_seen_txos.emplace(txo);
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
    //! will invalidate the claim for that amount.
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

        CBlockHeader header;
        CTransaction tx;
        CTxIndex tx_index;

        while (!txo_queue.empty()) {
            const TxoFrame frame = txo_queue.front();
            txo_queue.pop();

            CAutoFile file(OpenBlockFile(frame.m_pos), SER_DISK, CLIENT_VERSION);

            if (file.IsNull()) {
                error("%s: OpenBlockFile failed", __func__);
                throw InvalidVoteError();
            }

            try {
                file >> header;
            } catch (...) {
                error("%s: deserialize or I/O error for block header", __func__);
                throw InvalidVoteError();
            }

            if (m_poll.Expired(header.nTime)) {
                continue; // Txo spent after the poll finished is irrelevant
            }

            if (fseek(file.Get(), frame.m_pos.nTxPos, SEEK_SET) != 0) {
                error("%s: tx fseek failed", __func__);
                throw InvalidVoteError();
            }

            try {
                file >> tx;
            } catch (...) {
                error("%s: deserialize or I/O error for tx", __func__);
                throw InvalidVoteError();
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
            amount -= frame.m_amount;

            if (!tx.IsCoinStake()) {
                continue; // This tx spends the txo but did not stake it
            }

            const uint256 tx_hash = tx.GetHash();

            // Intermediate mapping of output offsets to amounts:
            std::vector<std::pair<uint32_t, CAmount>> next_txos;

            // Find outputs for the same address:
            for (uint32_t i = 0; i < tx.vout.size(); ++i) {
                CTxDestination dest;

                if (!ExtractDestination(tx.vout[i].scriptPubKey, dest)) {
                    continue;
                }

                if (dest != address) {
                    continue;
                }

                if (m_seen_txos.find({ tx_hash, i }) != m_seen_txos.end()) {
                    continue;
                }

                amount += tx.vout[i].nValue;
                next_txos.emplace_back(i, tx.vout[i].nValue);
            }

            if (next_txos.empty()) {
                continue; // Tx stakes the entire txo to different addresses
            }

            if (!m_txdb.ReadTxIndex(tx_hash, tx_index)) {
                error("%s: failed to read next tx index", __func__);
                throw InvalidVoteError();
            }

            for (auto& next_pair : next_txos) {
                if (next_pair.first >= tx_index.vSpent.size()) {
                    error("%s: txo out of spent range", __func__);
                    throw InvalidVoteError(); // should never happen
                }

                const CDiskTxPos& pos = tx_index.vSpent[next_pair.first];

                if (!pos.IsNull()) {
                    txo_queue.emplace(pos, next_pair.second);
                } else {
                    m_seen_txos.emplace(tx_hash, next_pair.first);
                }
            }
        }

        return amount;
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
    //! \param poll The poll to count votes for.
    //!
    LegacyVoteCounterContext(const Poll& poll)
        : m_poll(poll)
        , m_magnitude_factor(0)
    {
    }

    //!
    //! \brief Get a map that associates legacy vote response to the offset of
    //! the choices in the poll.
    //!
    const LegacyChoiceMap& GetChoices()
    {
        if (m_legacy_choices_cache.empty()) {
            for (uint8_t i = 0; i < m_poll.Choices().size(); ++i) {
                std::string label = m_poll.Choices().At(i)->m_label;
                boost::to_lower(label);

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
    Weight GetMagnitudeFactor()
    {
        if (m_magnitude_factor != 0) {
            return m_magnitude_factor;
        }

        // Note that this function produces an incorrect value because the
        // result depends on the money supply and superblock from the most
        // recent block instead of from the poll window. We retain the old
        // behavior here so that legacy poll results match existing nodes,
        // but we may want to fix this in the future:
        //
        const SuperblockPtr superblock = Quorum::CurrentSuperblock();
        const uint32_t total_mag = superblock->m_cpids.TotalMagnitude();
        const uint64_t supply = pindexBest->nMoneySupply;

        // Legacy money supply factor calculations added 0.01 to the total
        // magnitude as a lazy way to prevent a division-by-zero error. We
        // retain this silliness to produce the same value that the legacy
        // voting system returned:
        //
        m_magnitude_factor = supply / (total_mag + 0.01) / 5.67;

        return m_magnitude_factor;
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
    Weight m_magnitude_factor;
    std::set<std::string> m_seen_keys;
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
    //!
    VoteCounter(CTxDB& txdb, const Poll& poll)
        : m_txdb(txdb)
        , m_poll(poll)
        , m_resolver(txdb, poll)
        , m_legacy(poll)
    {
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
        m_magnitude_factor = supply / total_mag * 100 / 567;
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
        m_votes.reserve(vote_txids.size());

        for (const auto& txid : reverse_iterate(vote_txids)) {
            try {
                ProcessVoteCandidate(FetchVoteCandidate(txid));
            } catch (const InvalidVoteError& e) {
                LogPrint(LogFlags::VOTE, "%s: skipped invalid vote: %s",
                    __func__,
                    txid.ToString());

                ++result.m_invalid_votes;
            }
        }

        for (auto& vote : m_votes) {
            result.TallyVote(std::move(vote));
        }
    }

private:
    CTxDB& m_txdb;
    const Poll& m_poll;
    std::vector<VoteDetail> m_votes;
    Weight m_magnitude_factor;
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

        if (!m_txdb.ReadDiskTx(txid, tx)) {
            LogPrint(LogFlags::VOTE, "%s: failed to read vote tx", __func__);
            throw InvalidVoteError();
        }

        if (tx.nTime < m_poll.m_timestamp) {
            LogPrint(LogFlags::VOTE, "%s: tx earlier than poll", __func__);
            throw InvalidVoteError();
        }

        if (m_poll.Expired(tx.nTime)) {
            LogPrint(LogFlags::VOTE, "%s: tx exceeds expiration", __func__);
            throw InvalidVoteError();
        }

        for (auto& contract : tx.GetContracts()) {
            if (contract.m_type != ContractType::VOTE) {
                continue;
            }

            if (!contract.WellFormed()) {
                LogPrint(LogFlags::VOTE, "%s: skipped bad contract", __func__);
                continue;
            }

            return VoteCandidate(contract, std::move(tx));
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

        ProcessLegacyVote(candidate.LegacyVote());
    }

    //!
    //! \brief Tally a vote candidate.
    //!
    //! \param candidate Contains the vote contract to resolve.
    //!
    void ProcessVote(const VoteCandidate& candidate)
    {
        VoteDetail detail = m_resolver.Resolve(candidate);

        if (detail.Empty()) {
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

        m_votes.emplace_back(std::move(detail));
    }

    //!
    //! \brief Tally a legacy vote candidate.
    //!
    //! \param vote Contains the vote contract to resolve.
    //!
    void ProcessLegacyVote(const LegacyVote& vote)
    {
        if (m_legacy.SeenKey(vote.m_key)) {
            LogPrint(LogFlags::VOTE, "%s: skipped duplicate key", __func__);
            throw InvalidVoteError();
        }

        VoteDetail detail;
        detail.m_responses = vote.ParseResponses(m_legacy.GetChoices());

        if (detail.m_responses.empty()) {
            LogPrint(LogFlags::VOTE, "%s: no valid response", __func__);
            throw InvalidVoteError();
        }

        detail.m_amount = vote.m_amount * COIN;
        detail.m_mining_id = vote.m_mining_id;
        detail.m_magnitude = Magnitude::RoundFrom(vote.m_magnitude);

        CalculateWeight(detail, m_legacy.GetMagnitudeFactor());

        m_votes.emplace_back(std::move(detail));
        m_legacy.RememberKey(vote.m_key);
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
                const Weight mag_weight = magnitude_factor
                    * detail.m_magnitude.Scaled()
                    / Magnitude::SCALE_FACTOR;

                response_weight = (mag_weight + detail.m_amount) / response_count;
                break;
            }

            case PollWeightType::CPID_COUNT:
                response_weight = detail.m_magnitude.Scaled() > 0;
                break;

            case PollWeightType::PARTICIPANT_COUNT:
                response_weight = 1 * COIN;

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
//! \brief Fetch the superblock used to calculate magnitude weight for the
//! specified poll.
//!
//! \param poll Poll to fetch the superblock for.
//!
//! \return Latest superblock active during the poll.
//!
SuperblockPtr ResolveSuperblockForPoll(const Poll& poll)
{
    SuperblockPtr superblock = Quorum::CurrentSuperblock();

    if (!poll.Expired(superblock.m_timestamp)) {
        return superblock;
    }

    const CBlockIndex* pindex = pindexBest;

    // Seek past the current superblock:
    for (; pindex && pindex->nHeight >= superblock.m_height; pindex = pindex->pprev);

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

//!
//! \brief Fetch the total network-wide money supply used to calculate magnitude
//! weight for the specified poll.
//!
//! \param poll Poll to fetch the money supply for.
//!
//! \return Money supply as of the last block in the poll window in units of
//! 1/100000000 GRC.
//!
CAmount ResolveMoneySupplyForPoll(const Poll& poll)
{
    if (!poll.Expired(pindexBest->nTime)) {
        return pindexBest->nMoneySupply;
    }

    const CBlockIndex* pindex = pindexBest;
    const int64_t poll_expiration = poll.Expiration();

    for (; pindex && pindex->nTime > poll_expiration; pindex = pindex->pprev);

    return pindex->nMoneySupply;
}
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Global Functions
// -----------------------------------------------------------------------------

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
{
    m_responses.resize(m_poll.Choices().size());
}

PollResultOption PollResult::BuildFor(const PollReference& poll_ref)
{
    if (PollOption poll = poll_ref.TryReadFromDisk()) {
        CTxDB txdb("r");
        PollResult result(std::move(*poll));
        VoteCounter counter(txdb, result.m_poll);

        if (result.m_poll.IncludesMagnitudeWeight()) {
            counter.EnableMagnitudeWeight(
                ResolveSuperblockForPoll(result.m_poll),
                ResolveMoneySupplyForPoll(result.m_poll));
        }

        counter.CountVotes(result, poll_ref.Votes());

        return result;
    }

    return boost::none;
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

    for (const auto& response_pair : detail.m_responses) {
        const uint8_t response_offset = response_pair.first;
        const Weight response_weight = response_pair.second;

        m_responses[response_offset].m_weight += response_weight;
        m_responses[response_offset].m_votes += 1.0 / detail.m_responses.size();
        m_total_weight += response_weight;
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

VoteDetail::VoteDetail() : m_amount(0), m_magnitude(Magnitude::Zero())
{
}

bool VoteDetail::Empty() const
{
    return m_amount == 0 && m_magnitude == 0;
}
