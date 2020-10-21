// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "main.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/voting/payloads.h"
#include "gridcoin/voting/registry.h"
#include "gridcoin/voting/vote.h"
#include "txdb.h"
#include "ui_interface.h"

using namespace GRC;
using LogFlags = BCLog::LogFlags;

namespace {
//!
//! \brief Extract a poll title from a legacy vote contract.
//!
//! \param key The value of the legacy contract key.
//!
//! \return Normalized title of the poll to associate the vote with.
//!
std::string ParseLegacyVoteTitle(const std::string& key)
{
    std::string title = key.substr(0, key.find(';'));
    boost::to_lower(title);

    return title;
}

//!
//! \brief Thrown when verifying an invalid poll contract.
//!
class InvalidPollError : public std::exception
{
public:
    InvalidPollError()
    {
    }
};

//!
//! \brief Verifies a participant's eligibility to create a poll.
//!
//! Validation for a poll claim occurs while connecting a block. As such, the
//! claim must pass validation at that time. If validation fails, a node will
//! reject the block. A node will also verify the poll claim when it receives
//! the transaction for inclusion in the memory pool so a poll must provide a
//! claim that remains valid until the next block (no spent outputs).
//!
//! This is different from the vote claim validation which occurs while nodes
//! evaluate the results of a poll.
//!
class PollClaimValidator
{
public:
    //!
    //! \brief Initialize a poll claim validator.
    //!
    //! \param txdb Used to resolve unspent amount claims.
    //!
    PollClaimValidator(CTxDB& txdb) : m_txdb(txdb)
    {
    }

    //!
    //! \brief Determine whether the poll claim meets the minimum requirements
    //! to create a poll.
    //!
    //! \param payload   Contains the poll contract to validate.
    //! \param timestamp Transaction that contains the poll.
    //!
    //! \return \c true If the resolved claim meets the requirements to create
    //! a poll.
    //!
    bool Validate(const PollPayload& payload, const CTransaction& tx)
    {
        const ClaimMessage message = PackPollMessage(payload.m_poll, tx);

        try {
            return VerifyClaim(payload.m_claim.m_address_claim, message);
        } catch (const InvalidPollError& e) {
            LogPrint(LogFlags::VOTE, "%s: bad poll claim", __func__);
            return false;
        }
    }

private:
    CTxDB& m_txdb; //!< Used to resolve unspent amount claims.

    //!
    //! \brief Determine whether the address claim establishes eligibility for
    //! creating the poll.
    //!
    //! \param claim   The claim to verify minimum balance for.
    //! \param message Serialized context for claim signature verification.
    //!
    //! \return \c true If the resolved amount for the the claim meets the
    //! minimum balance requirement to create a poll.
    //!
    bool VerifyClaim(const AddressClaim& claim, const ClaimMessage& message)
    {
        if (!claim.VerifySignature(message)) {
            LogPrint(LogFlags::VOTE, "%s: bad address signature", __func__);
            return false;
        }

        const CTxDestination address = claim.m_public_key.GetID();
        CAmount amount = 0;

        for (const auto& txo : claim.m_outpoints) {
            amount += Resolve(txo, address);
        }

        return amount >= POLL_REQUIRED_BALANCE;
    }

    //!
    //! \brief Resolve the claimed amount for an output.
    //!
    //! \param txo     Refers to the output to resolve balance weight for.
    //! \param address Must match the address of the resolved output.
    //!
    //! \return Claimed amount in units of 1/100000000 GRC.
    //!
    //! \throws InvalidPollError If the output fails to validate or if an IO
    //! error occurs.
    //!
    int64_t Resolve(const COutPoint& txo, const CTxDestination& address)
    {
        CTxIndex tx_index;

        if (!m_txdb.ReadTxIndex(txo.hash, tx_index)) {
            LogPrint(LogFlags::VOTE, "%s: failed to read tx index", __func__);
            throw InvalidPollError();
        }

        if (txo.n >= tx_index.vSpent.size()) {
            error("%s: txo out of spent range", __func__);
            throw InvalidPollError(); // should never happen
        }

        if (!tx_index.vSpent[txo.n].IsNull()) {
            error("%s: found spent txo", __func__);
            throw InvalidPollError();
        }

        CTransaction tx;

        if (!tx.ReadFromDisk(tx_index.pos)) {
            throw InvalidPollError();
        }

        if (txo.n >= tx.vout.size()) {
            LogPrint(LogFlags::VOTE, "%s: txo out of range", __func__);
            throw InvalidPollError();
        }

        const CTxOut& output = tx.vout[txo.n];

        if (output.nValue < COIN) {
            LogPrint(LogFlags::VOTE, "%s: txo < 1 GRC", __func__);
            throw InvalidPollError();
        }

        CTxDestination dest;

        if (!ExtractDestination(output.scriptPubKey, dest)) {
            LogPrint(LogFlags::VOTE, "%s: invalid txo address", __func__);
            throw InvalidPollError();
        }

        if (dest != address) {
            LogPrint(LogFlags::VOTE, "%s: txo address mismatch", __func__);
            throw InvalidPollError();
        }

        return output.nValue;
    }
}; // PollClaimValidator

//!
//! \brief Global poll registry instance.
//!
PollRegistry g_poll_registry;
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Global Functions
// -----------------------------------------------------------------------------

PollRegistry& GRC::GetPollRegistry()
{
    return g_poll_registry;
}

std::string GRC::GetCurrentPollTitle()
{
    LOCK(cs_main);

    if (const PollReference* poll_ref = GetPollRegistry().TryLatestActive()) {
        return poll_ref->Title();
    }

    return _("No current polls");
}

ClaimMessage GRC::PackPollMessage(const Poll& poll, const CTransaction& tx)
{
    std::vector<uint8_t> bytes;
    CVectorWriter writer(SER_NETWORK, PROTOCOL_VERSION, bytes, 0);

    writer << poll;
    writer << tx.nTime;

    for (const auto& txin : tx.vin) {
        writer << txin.prevout;
    }

    return bytes;
}

// -----------------------------------------------------------------------------
// Class: PollReference
// -----------------------------------------------------------------------------

PollReference::PollReference()
    : m_ptxid(nullptr)
    , m_ptitle(nullptr)
    , m_timestamp(0)
    , m_duration_days(0)
{
}

PollOption PollReference::TryReadFromDisk(CTxDB& txdb) const
{
    CTransaction tx;

    if (!txdb.ReadDiskTx(*m_ptxid, tx)) {
        error("%s: failed to read poll tx from disk", __func__);
        return boost::none;
    }

    for (auto& contract : tx.PullContracts()) {
        if (contract.m_type == ContractType::POLL) {
            auto payload = contract.PullPayloadAs<PollPayload>();
            payload.m_poll.m_timestamp = m_timestamp;

            return std::move(payload.m_poll);
        }
    }

    error("%s: transaction does not contain a poll contract", __func__);

    return boost::none;
}

PollOption PollReference::TryReadFromDisk() const
{
    CTxDB txdb("r");

    return TryReadFromDisk(txdb);
}

uint256 PollReference::Txid() const
{
    if (!m_ptxid) {
        return uint256();
    }

    return *m_ptxid;
}

const std::string& PollReference::Title() const
{
    if (!m_ptitle) {
        static const std::string empty("");
        return empty;
    }

    return *m_ptitle;
}

const std::vector<uint256>& PollReference::Votes() const
{
    return m_votes;
}

int64_t PollReference::Age(const int64_t now) const
{
    return now - m_timestamp;
}

bool PollReference::Expired(const int64_t now) const
{
    return Age(now) > m_duration_days * 86400;
}

int64_t PollReference::Expiration() const
{
    return m_timestamp + (m_duration_days * 86400);
}

void PollReference::LinkVote(const uint256 txid)
{
    m_votes.emplace_back(txid);
}

void PollReference::UnlinkVote(const uint256 txid)
{
    for (auto it = m_votes.crbegin(), end = m_votes.crend(); it != end; ++it) {
        if (*it == txid) {
            m_votes.erase(std::next(it).base());
            return;
        }
    }
}

// -----------------------------------------------------------------------------
// Class: PollRegistry
// -----------------------------------------------------------------------------

const PollRegistry::Sequence PollRegistry::Polls() const
{
    return Sequence(m_polls);
}

const PollReference* PollRegistry::TryLatestActive() const
{
    if (m_latest_poll && !m_latest_poll->Expired(GetAdjustedTime())) {
        return m_latest_poll;
    }

    if (m_polls.empty()) {
        return nullptr;
    }

    const PollReference* latest = &m_polls.cbegin()->second;

    for (auto iter = ++m_polls.cbegin(); iter != m_polls.cend(); ++iter) {
        if (iter->second.m_timestamp > latest->m_timestamp) {
            latest = &iter->second;
        }
    }

    if (latest->Expired(GetAdjustedTime())) {
        return nullptr;
    }

    return latest;
}

const PollReference* PollRegistry::TryByTxid(const uint256 txid) const
{
    const auto iter = m_polls_by_txid.find(txid);

    if (iter == m_polls_by_txid.end()) {
        return nullptr;
    }

    return iter->second;
}

PollReference* PollRegistry::TryBy(const uint256 txid)
{
    return const_cast<PollReference*>(TryByTxid(txid));
}

const PollReference* PollRegistry::TryByTitle(const std::string& title) const
{
    const auto iter = m_polls.find(title);

    if (iter == m_polls.end()) {
        return nullptr;
    }

    return &iter->second;
}

PollReference* PollRegistry::TryBy(const std::string& title)
{
    return const_cast<PollReference*>(TryByTitle(title));
}

void PollRegistry::Reset()
{
    m_polls.clear();
    m_polls_by_txid.clear();
    m_latest_poll = nullptr;
}

bool PollRegistry::Validate(const Contract& contract, const CTransaction& tx) const
{
    // Vote contract claims do not affect consensus. Vote claim validation
    // occurs on-demand while computing the results of the poll:
    //
    if (contract.m_type == ContractType::VOTE) {
        return true;
    }

    // Legacy poll contracts do not invoke any contextual validation:
    //
    if (contract.m_version == 1) {
        return true;
    }

    const auto payload = contract.SharePayloadAs<PollPayload>();

    if (payload->m_version < 2) {
        LogPrint(LogFlags::CONTRACT, "%s: rejected legacy poll", __func__);
        return false;
    }

    CTxDB txdb("r");

    return PollClaimValidator(txdb).Validate(*payload, tx);
}

void PollRegistry::Add(const ContractContext& ctx)
{
    if (ctx->m_type == ContractType::VOTE) {
        AddVote(ctx);
    } else {
        AddPoll(ctx);
    }
}

void PollRegistry::Delete(const ContractContext& ctx)
{
    if (ctx->m_type == ContractType::VOTE) {
        DeleteVote(ctx);
    } else {
        DeletePoll(ctx);
    }
}

void PollRegistry::AddPoll(const ContractContext& ctx)
{
    const auto payload = ctx->SharePayloadAs<PollPayload>();
    std::string poll_title = payload->m_poll.m_title;

    if (ctx->m_version == 1) {
        boost::to_lower(poll_title);
    }

    auto result_pair = m_polls.emplace(std::move(poll_title), PollReference());

    if (result_pair.second) {
        const std::string& title = result_pair.first->first;

        PollReference& poll_ref = result_pair.first->second;
        poll_ref.m_ptitle = &title;
        poll_ref.m_timestamp = ctx.m_tx.nTime;
        poll_ref.m_duration_days = payload->m_poll.m_duration_days;

        m_latest_poll = &poll_ref;

        auto result_pair = m_polls_by_txid.emplace(ctx.m_tx.GetHash(), &poll_ref);
        poll_ref.m_ptxid = &result_pair.first->first;
    }
}

void PollRegistry::AddVote(const ContractContext& ctx)
{
    if (ctx->m_version >= 2) {
        const auto vote = ctx->SharePayloadAs<Vote>();

        if (PollReference* poll_ref = TryBy(vote->m_poll_txid)) {
            if (poll_ref->Expired(ctx.m_pindex->nTime)) {
                LogPrint(LogFlags::VOTE,
                    "%s: ignored vote %s for finished poll %s",
                    __func__,
                    ctx.m_tx.GetHash().ToString(),
                    poll_ref->Txid().ToString());
            } else {
                poll_ref->LinkVote(ctx.m_tx.GetHash());
            }
        }

        return;
    }

    const ContractPayload vote = ctx->m_body.AssumeLegacy();
    const std::string title = ParseLegacyVoteTitle(vote->LegacyKeyString());

    if (title.empty()) {
        return;
    }

    if (PollReference* poll_ref = TryBy(title)) {
        poll_ref->LinkVote(ctx.m_tx.GetHash());
    }
}

void PollRegistry::DeletePoll(const ContractContext& ctx)
{
    const auto payload = ctx->SharePayloadAs<PollPayload>();

    if (ctx->m_version >= 2) {
        m_polls.erase(payload->m_poll.m_title);
    } else {
        m_polls.erase(boost::to_lower_copy(payload->m_poll.m_title));
    }

    m_polls_by_txid.erase(ctx.m_tx.GetHash());
    m_latest_poll = nullptr;
}

void PollRegistry::DeleteVote(const ContractContext& ctx)
{
    if (ctx->m_version >= 2) {
        const auto vote = ctx->SharePayloadAs<Vote>();

        if (PollReference* poll_ref = TryBy(vote->m_poll_txid)) {
            poll_ref->UnlinkVote(ctx.m_tx.GetHash());
        }

        return;
    }

    const auto vote = ctx->SharePayload();
    const std::string title = ParseLegacyVoteTitle(vote->LegacyKeyString());

    if (title.empty()) {
        return;
    }

    if (PollReference* poll_ref = TryBy(title)) {
        poll_ref->UnlinkVote(ctx.m_tx.GetHash());
    }
}

// -----------------------------------------------------------------------------
// Class: PollRegistry::Sequence
// -----------------------------------------------------------------------------

using Sequence = PollRegistry::Sequence;

Sequence::Sequence(const PollMapByTitle& polls, const bool active_only)
    : m_polls(polls), m_active_only(active_only)
{
}

Sequence Sequence::OnlyActive(const bool active_only) const
{
    return Sequence(m_polls, active_only);
}

Sequence::Iterator Sequence::begin() const
{
    auto iter = m_polls.begin();
    auto end = m_polls.end();
    int64_t now = 0;

    if (m_active_only) {
        now = GetAdjustedTime();

        while (iter != end && iter->second.Expired(now)) {
            ++iter;
        }
    }

    return Iterator(iter, end, m_active_only, now);
}

Sequence::Iterator Sequence::end() const
{
    return Iterator(m_polls.end());
}

// -----------------------------------------------------------------------------
// Class: PollRegistry::Sequence::Iterator
// -----------------------------------------------------------------------------

using Iterator = PollRegistry::Sequence::Iterator;

Iterator::Iterator(
    BaseIterator iter,
    BaseIterator end,
    const bool active_only,
    const int64_t now)
    : m_iter(iter)
    , m_end(end)
    , m_active_only(active_only)
    , m_now(now)
{
}

Iterator::Iterator(BaseIterator end) : m_iter(end), m_end(end)
{
}

const PollReference& Iterator::Ref() const
{
    return m_iter->second;
}

PollOption Iterator::TryPollFromDisk() const
{
    return m_iter->second.TryReadFromDisk();
}

Iterator::reference Iterator::operator*() const
{
    return *this;
}

Iterator::pointer Iterator::operator->() const
{
    return this;
}

Iterator& Iterator::operator++()
{
    do {
        ++m_iter;
    } while (m_active_only && m_iter != m_end && m_iter->second.Expired(m_now));

    return *this;
}

Iterator Iterator::operator++(int)
{
    Iterator copy(*this);
    ++(*this);

    return copy;
}

bool Iterator::operator==(const Iterator& other) const
{
    return m_iter == other.m_iter;
}

bool Iterator::operator!=(const Iterator& other) const
{
    return m_iter != other.m_iter;
}
