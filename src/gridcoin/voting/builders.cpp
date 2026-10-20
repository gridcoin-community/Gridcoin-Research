// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "chainparams.h"
#include "init.h"
#include "gridcoin/beacon.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/contract/message.h"
#include "gridcoin/researcher.h"
#include "gridcoin/voting/builders.h"
#include "gridcoin/voting/claims.h"
#include "gridcoin/voting/payloads.h"
#include "gridcoin/voting/registry.h"
#include "node/blockstorage.h"
#include "node/ui_interface.h"
#include "txmempool.h"
#include "validation.h"
#include "wallet/coincontrol.h"
#include "wallet/wallet.h"
#include "consensus/consensus.h"
#include <numeric>
#include <optional>
#include <set>
#include <util/string.h>

using namespace GRC;
using LogFlags = BCLog::LogFlags;

namespace {
//!
//! \brief Determine whether a transaction carries a vote on a poll.
//!
bool VotesOn(const CTransaction& tx, const uint256& poll_txid)
{
    for (const auto& contract : tx.GetContracts()) {
        if (contract.m_type == ContractType::VOTE && contract.m_version >= 2
            && contract.SharePayloadAs<Vote>()->m_poll_txid == poll_txid) {
            return true;
        }
    }

    return false;
}

//!
//! \brief A set of outputs associated with an address for a voting claim.
//!
//! This intermediate container holds context about the unspent outputs for
//! an address in the wallet to build voting claims from.
//!
class AddressOutputs
{
public:
    CKeyID m_key_id;                    //!< Address of the outputs.
    std::vector<COutPoint> m_outpoints; //!< Outputs for the address.
    std::vector<CAmount> m_amounts;     //!< Amounts for each output.
    CAmount m_total_amount;             //!< Total amount of the outputs.

    //!
    //! \brief Initialize an output address grouping.
    //!
    //! \param key_id The address that groups the contained outputs.
    //!
    AddressOutputs(const CKeyID key_id) : m_key_id(key_id), m_total_amount(0)
    {
    }

    //!
    //! \brief Determine whether the total amount of the grouped outputs
    //! compares greater than than another address grouping.
    //!
    bool operator>(const AddressOutputs& other) const
    {
        return m_total_amount > other.m_total_amount;
    }

    //!
    //! \brief Add an output to the address grouping.
    //!
    //! \param output An output associated with the address.
    //!
    void Add(const COutput& output)
    {
        m_outpoints.emplace_back(output.tx->GetHash(), output.i);
        m_amounts.emplace_back(output.tx->vout[output.i].nValue);

        m_total_amount += m_amounts.back();
    }

    //!
    //! \brief Add an output to the address grouping by its outpoint.
    //!
    //! \param outpoint The output associated with the address.
    //! \param amount   An amount used only to order and trim the claim's outputs
    //! (for an origin, the value of the young output it was traced from).
    //!
    void Add(const COutPoint& outpoint, const CAmount amount)
    {
        m_outpoints.push_back(outpoint);
        m_amounts.push_back(amount);

        m_total_amount += amount;
    }
}; // AddressOutputs

using OutputAddresses = std::vector<AddressOutputs>;

//!
//! \brief How many of the wallet's own vote or coinstake transactions, at most,
//! CoinPicker::TraceOrigins walks back through along each path to find a
//! claimable origin.
//!
constexpr int MAX_TRACE_DEPTH = 16;

//!
//! \brief Confirmations an output needs before a voting claim names it. Mirrors
//! CWalletTx::IsConfirmed (depth >= 10), which the claim filters apply.
//!
constexpr int CLAIM_MIN_DEPTH = 10;

//!
//! \brief Selects outputs for building voting claims.
//!
class CoinPicker
{
public:
    //!
    //! \brief Initialize a coin picker.
    //!
    //! \param wallet The wallet to fetch outputs from.
    //!
    CoinPicker(const CWallet& wallet) : m_wallet(wallet)
    {
    }

    //!
    //! \brief Get a set of outputs to build voting claims with.
    //!
    //! \param trace_origins Whether to claim a young output that one of the
    //! wallet's own votes or coinstakes paid back to one of its input addresses
    //! by the input it came from (TraceOrigins). Only for polls that start at
    //! or after the v15 height. A young output may then also be one of the
    //! wallet's own trusted outputs with no confirmation yet; the claim never
    //! names such an output itself, only its origins.
    //! \param poll_txid The poll the claim is for.
    //! \param pending_vote_output Set to the young output of one of the wallet's
    //! own votes that is claimed by its origins: the largest one that comes from
    //! a vote on \p poll_txid, else the largest one; left as it is if none.
    //!
    //! \return Unspent outputs grouped by address in descending order by
    //! the total amount of the outputs in each address grouping.
    //!
    //! \throws VotingError If the wallet contains no unspent outputs eligible
    //! for a claim. When it holds young single-key outputs of at least 1 GRC
    //! that it can sign for and could not claim, counting its own trusted
    //! outputs with no confirmation yet (an incoming payment from another
    //! wallet that has not confirmed is not counted), the message says how many
    //! more confirmations the first of them to mature needs.
    //!
    OutputAddresses PickCoins(
        const bool trace_origins,
        const uint256& poll_txid,
        std::optional<COutPoint>& pending_vote_output) const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        // The outputs AvailableCoins returns point into the wallet's
        // transactions, so the wallet stays locked while they are read.
        LOCK(m_wallet.cs_wallet);

        std::vector<COutput> outputs;
        m_wallet.AvailableCoins(outputs, true, nullptr, true);

        // AvailableCoins skips outputs with no confirmation when it includes
        // staked coins. The wallet's own trusted ones, such as a previous vote's
        // change, are young outputs too: a vote on a poll that starts at or
        // after v15 traces them to their origins (TraceOrigins), and one that is
        // not claimed counts toward the wait the refusal below reports.
        if (trace_origins) {
            const std::vector<COutput> unconfirmed = UnconfirmedOutputs();
            outputs.insert(outputs.end(), unconfirmed.begin(), unconfirmed.end());
        }

        const auto descending_by_amount = [](const COutput& a, const COutput& b) {
            return a.tx->vout[a.i].nValue > b.tx->vout[b.i].nValue;
        };

        std::sort(outputs.begin(), outputs.end(), descending_by_amount);

        LogPrint(LogFlags::VOTE,
            "%s: wallet supplied %" PRIszu " tx output candidates",
            __func__,
            outputs.size());

        int young_depth = -1;
        OutputAddressMap by_address = FilterOutputs(outputs, trace_origins, poll_txid, young_depth, pending_vote_output);

        LogPrint(LogFlags::VOTE,
            "%s: filtered tx output set contains %" PRIszu " addresses",
            __func__,
            by_address.size());

        if (by_address.empty()) {
            // A vote on an earlier poll looks the wallet's own trusted outputs
            // with no confirmation yet up only here, for the wait they need.
            if (young_depth < 0 && !trace_origins && !UnconfirmedOutputs().empty()) {
                young_depth = 0;
            }

            if (young_depth >= 0) {
                throw VotingError(strprintf(
                    _("No eligible outputs greater than 1 GRC yet: the wallet's coins need %d more confirmation(s)."),
                    std::max(1, CLAIM_MIN_DEPTH - young_depth)));
            }

            throw VotingError(_("No eligible outputs greater than 1 GRC."));
        }

        return SortAddressesByAmount(std::move(by_address));
    }

private:
    const CWallet& m_wallet; //!< The wallet to fetch outputs from.

    using OutputAddressMap = std::map<CKeyID, AddressOutputs>;

    //!
    //! \brief Adds outputs for an address only when an address represents
    //! a pay-to-public-key or pay-to-public-key-hash destination that the
    //! wallet holds the key for.
    //!
    class PubkeyDestinationFilter
    {
    public:
        //!
        //! \brief Initialize a filter.
        //!
        //! \param wallet     Holds the keys that sign the address claims.
        //! \param by_address A set of outputs grouped by address to add the
        //! filtered outputs to.
        //!
        PubkeyDestinationFilter(const CWallet& wallet, OutputAddressMap& by_address)
            : m_wallet(wallet)
            , m_by_address(by_address)
        {
        }

        void operator()(const CKeyID& keyId, const COutput& output) const
        {
            // An address claim is signed with the address's key. AvailableCoins
            // also returns outputs the wallet holds no key for: watch-only
            // outputs, and the outputs an immature coinstake paid to others.
            if (!m_wallet.HaveKey(keyId)) {
                LogPrint(LogFlags::VOTE, "  skipped output for an address without a key: %s",
                    EncodeDestination(keyId));
                return;
            }

            LogPrint(LogFlags::VOTE, "  added output for address: %s",
                EncodeDestination(keyId));

            auto result_pair = m_by_address.emplace(keyId, keyId);
            AddressOutputs& address_outputs = result_pair.first->second;

            address_outputs.Add(output);
        }

        void operator()(const CNoDestination& dest, const COutput& output) const
        {
            // Voting does not support unspendable outputs.
            LogPrint(LogFlags::VOTE, "  skipped no-destination address");
        }

        void operator()(const CScriptID& scriptId, const COutput& output) const
        {
            // Voting does not support redemption script outputs yet.
            LogPrint(LogFlags::VOTE, "  skipped script address: %s",
                EncodeDestination(scriptId));
        }

    private:
        const CWallet& m_wallet;        //!< Holds the keys for the address claims.
        OutputAddressMap& m_by_address; //!< Outputs grouped by address.
    }; // PubkeyDestinationFilter

    //!
    //! \brief Get the wallet's own unspent, trusted outputs with no confirmation
    //! yet, of at least 1 GRC, at a single-key address it can sign for.
    //!
    //! AvailableCoins skips these when it includes staked coins (PickCoins). An
    //! incoming payment from another wallet is not trusted until it confirms, so
    //! it is not among them.
    //!
    std::vector<COutput> UnconfirmedOutputs() const EXCLUSIVE_LOCKS_REQUIRED(cs_main, m_wallet.cs_wallet)
    {
        std::vector<COutput> outputs;
        const CCoinControl spendable; // Only outputs the wallet can sign for.
        m_wallet.AvailableCoins(outputs, true, &spendable, false);

        const auto ineligible = [](const COutput& txo) {
            const CTxOut& output = txo.tx->vout[txo.i];
            CTxDestination dest;

            return txo.nDepth != 0
                || output.nValue < COIN
                || !ExtractDestination(output.scriptPubKey, dest)
                || !std::get_if<CKeyID>(&dest);
        };

        outputs.erase(std::remove_if(outputs.begin(), outputs.end(), ineligible), outputs.end());

        return outputs;
    }

    //!
    //! \brief Get a set of filtered outputs spent for public key addresses.
    //!
    //! \param outputs       The set of outputs to filter. Sorted in descending
    //! order by amount.
    //! \param trace_origins Whether to claim young outputs by their origins.
    //! \param young_depth   Set to the greatest depth among the young single-key
    //! outputs of at least 1 GRC that the wallet can sign for and that it skipped
    //! (0 for one with no confirmation yet); left as it is if none.
    //! \param poll_txid     The poll the claim is for.
    //! \param pending_vote_output Set to the young output of one of the wallet's
    //! own votes that is claimed by its origins: the largest one that comes from
    //! a vote on \p poll_txid, else the largest one; left as it is if none.
    //!
    //! \return Filtered outputs grouped by address.
    //!
    OutputAddressMap FilterOutputs(
        const std::vector<COutput>& outputs,
        const bool trace_origins,
        const uint256& poll_txid,
        int& young_depth,
        std::optional<COutPoint>& pending_vote_output) const EXCLUSIVE_LOCKS_REQUIRED(cs_main, m_wallet.cs_wallet)
    {
        // The young output of the wallet's own votes that a re-vote's fee
        // should come from: one that comes from a vote on this poll, so the
        // re-vote spends that vote's change, else any.
        std::optional<COutPoint> pending_same_poll;
        std::optional<COutPoint> pending_any;

        OutputAddressMap by_address;
        PubkeyDestinationFilter pubkey_filter(m_wallet, by_address);

        // Every outpoint the claim lists. Several young outputs can trace to one
        // origin, and an origin may also be a coin the claim lists directly; the
        // claim names each coin once.
        std::set<COutPoint> claimed;

        for (const auto& txo : outputs) {
            CTxDestination dest;

            if (!ExtractDestination(txo.tx->vout[txo.i].scriptPubKey, dest)) {
                LogPrint(LogFlags::VOTE, "  skipped invalid output in %s",
                    txo.tx->GetHash().ToString());
                continue;
            }

            // Avoid voting with outputs less than 1 GRC. This avoids bloating
            // the vote contract with insignificant weight that costs the user
            // additional fees:
            //
            if (txo.tx->vout[txo.i].nValue < COIN) {
                LogPrint(LogFlags::VOTE, "  skipped < 1 GRC outputs for: %s",
                    EncodeDestination(dest));

                break; // The input set is sorted so the rest are too small.
            }

            if (!txo.tx->IsTrusted() || !txo.tx->IsConfirmed()) {
                // AvailableCoins also returns every output of an immature
                // coinstake, including any it paid to other addresses.
                const CKeyID* const key_id = std::get_if<CKeyID>(&dest);
                const bool own_key = key_id && m_wallet.HaveKey(*key_id);
                std::vector<std::pair<COutPoint, CAmount>> origins;
                bool from_poll_vote = false;

                if (trace_origins && own_key) {
                    origins = TraceOrigins(*txo.tx, txo.i, poll_txid, from_poll_vote);
                }

                // Every origin pays the young output's own address (TraceOrigins).
                for (const auto& origin : origins) {
                    if (claimed.insert(origin.first).second) {
                        by_address.emplace(*key_id, *key_id).first->second.Add(origin.first, origin.second);
                    }
                }

                if (origins.empty()) {
                    // It becomes claimable when it has CLAIM_MIN_DEPTH confirmations.
                    if (own_key) {
                        young_depth = std::max(young_depth, txo.nDepth);
                    }

                    LogPrint(LogFlags::VOTE, "  skipped unconfirmed output for %s",
                        EncodeDestination(dest));
                } else {
                    LogPrint(LogFlags::VOTE, "  claimed young output for %s by %" PRIszu " origin(s)",
                        EncodeDestination(dest),
                        origins.size());

                    // The outputs are sorted by amount, so the first is the largest.
                    if (CarriesVote(*txo.tx)) {
                        const COutPoint outpoint(txo.tx->GetHash(), txo.i);

                        if (from_poll_vote && !pending_same_poll) {
                            pending_same_poll = outpoint;
                        }

                        if (!pending_any) {
                            pending_any = outpoint;
                        }
                    }
                }

                continue;
            }

            if (!claimed.insert(COutPoint(txo.tx->GetHash(), txo.i)).second) {
                LogPrint(LogFlags::VOTE, "  skipped output already claimed for %s",
                    EncodeDestination(dest));
                continue;
            }

            // Check that the output is P2PK or P2PKH. Voting does not support
            // more advanced redemption scripts like multisig yet:
            //
            auto filter = std::bind(pubkey_filter, std::placeholders::_1, txo);
            std::visit(filter, dest);
        }

        if (!pending_vote_output) {
            pending_vote_output = pending_same_poll ? pending_same_poll : pending_any;
        }

        return by_address;
    }

    //!
    //! \brief Find the claimable origins of a young output that one of the wallet's
    //! own votes or coinstakes paid back to one of its input addresses.
    //!
    //! For polls that start at or after v15, the tally follows a claimed coin
    //! through a coinstake and through any vote transaction to their outputs at
    //! the same address. Such an output can therefore be claimed as the input it
    //! came from: the claim names a coin with the usual confirmations, and the
    //! tally walks forward from it. An input that is itself a young output of
    //! such a transaction is traced again, through at most MAX_TRACE_DEPTH
    //! transactions along each path.
    //!
    //! \param wtx   The transaction that contains the young output.
    //! \param n     The offset of the young output in \p wtx.
    //! \param poll_txid The poll the claim is for.
    //! \param from_poll_vote Set if a transaction the trace walks through
    //! carries a vote on \p poll_txid; left as it is otherwise.
    //! \param depth How many transactions the trace has already walked through.
    //!
    //! \return The origins with the young output's value, or nothing if the output
    //! did not come from such a transaction or no step reaches a claimable coin;
    //! the output is then skipped, as it was before. Every origin has at least
    //! 1 GRC, the usual confirmations, and the young output's single-key address.
    //!
    std::vector<std::pair<COutPoint, CAmount>> TraceOrigins(
        const CWalletTx& wtx,
        const unsigned int n,
        const uint256& poll_txid,
        bool& from_poll_vote,
        const int depth = 0) const EXCLUSIVE_LOCKS_REQUIRED(cs_main, m_wallet.cs_wallet)
    {
        std::vector<std::pair<COutPoint, CAmount>> origins;

        if (depth >= MAX_TRACE_DEPTH || n >= wtx.vout.size()) {
            return origins;
        }

        if (!wtx.IsCoinStake() && !CarriesVote(wtx)) {
            return origins;
        }

        if (VotesOn(wtx, poll_txid)) {
            from_poll_vote = true;
        }

        CTxDestination dest;

        if (!ExtractDestination(wtx.vout[n].scriptPubKey, dest) || !std::get_if<CKeyID>(&dest)) {
            return origins;
        }

        for (const auto& txin : wtx.vin) {
            const auto iter = m_wallet.mapWallet.find(txin.prevout.hash);

            if (iter == m_wallet.mapWallet.end() || txin.prevout.n >= iter->second.vout.size()) {
                continue;
            }

            const CWalletTx& prev_tx = iter->second;
            const CTxOut& prev = prev_tx.vout[txin.prevout.n];
            CTxDestination prev_dest;

            if (!ExtractDestination(prev.scriptPubKey, prev_dest) || prev_dest != dest) {
                continue;
            }

            if (prev_tx.IsTrusted() && prev_tx.IsConfirmed()) {
                // The tally rejects a claimed output under 1 GRC; never name one.
                if (prev.nValue >= COIN) {
                    origins.emplace_back(txin.prevout, wtx.vout[n].nValue);
                }

                continue;
            }

            for (const auto& origin : TraceOrigins(prev_tx, txin.prevout.n, poll_txid, from_poll_vote, depth + 1)) {
                origins.emplace_back(origin.first, wtx.vout[n].nValue);
            }
        }

        return origins;
    }

    //!
    //! \brief Get an ordered set of output address groups.
    //!
    //! \param by_address The set of outputs grouped by address to sort.
    //!
    //! \return Output address groups sorted by the total amount of the outputs
    //! in each group in descending order.
    //!
    static OutputAddresses SortAddressesByAmount(OutputAddressMap by_address)
    {
        OutputAddresses ordered;
        ordered.reserve(by_address.size());

        for (auto& address_pair : by_address) {
            ordered.emplace_back(std::move(address_pair.second));
        }

        // Order the output address groups by total amount. For wallets with
        // more outputs than the number of outputs that will fit in a voting
        // transaction, we want to choose the largest addresses first, so we
        // sort in descending order.
        //
        // TODO: Ordering by address is not perfect because an address could
        // contain one large output and hundreds of small outputs. We should
        // use a more accurate order based on something like the average for
        // the output amounts associated with the address.
        //
        std::sort(ordered.begin(), ordered.end(), std::greater<AddressOutputs>());

        return ordered;
    }
}; // CoinPicker

//!
//! \brief Constructs a provable address weight claim for voting.
//!
class AddressClaimBuilder
{
public:
    //!
    //! \brief Initialize an address claim builder.
    //!
    //! \param wallet Used to fetch a private key to sign the claim.
    //!
    AddressClaimBuilder(const CWallet& wallet) : m_wallet(wallet)
    {
    }

    //!
    //! \brief Generate an address claim.
    //!
    //! \param address_outputs Context about the address to build the claim for.
    //!
    //! \return An address claim for the provided outputs.
    //!
    std::optional<AddressClaim> TryBuildClaim(AddressOutputs address_outputs) const
    {
        AddressClaim claim(std::move(address_outputs.m_outpoints));

        if (!m_wallet.GetPubKey(address_outputs.m_key_id, claim.m_public_key)) {
            error("%s: failed to fetch public key for address %s",
                __func__,
                EncodeDestination(address_outputs.m_key_id));

            return std::nullopt;
        }

        LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: building address claim for address_outputs.m_key_id %s, "
                                        "claim.m_public_key address %s.",
                 __func__,
                 EncodeDestination(address_outputs.m_key_id),
                 EncodeDestination(claim.m_public_key.GetID()));

        // An address claim must submit outputs in ascending order. This
        // improves the performance of duplicate output validation:
        //
        std::sort(claim.m_outpoints.begin(), claim.m_outpoints.end());

        return claim;
    }

    //!
    //! \brief Sign an address claim with the private key for the address.
    //!
    //! \param claim   The claim object to sign.
    //! \param message The message to sign for the claim.
    //!
    //! \return \c false if an error occurs while signing the claim.
    //!
    bool SignClaim(AddressClaim& claim, const ClaimMessage& message) const
    {
        const CKeyID key_id = claim.m_public_key.GetID();
        CKey private_key;

        if (!m_wallet.GetKey(key_id, private_key)) {
            return error("%s: failed to fetch private key for address %s",
                __func__,
                EncodeDestination(key_id));
        }

        if (!private_key.IsValid()) {
            return error("%s: invalid private key for address %s",
                __func__,
                EncodeDestination(key_id));
        }

        if (!claim.Sign(private_key, message)) {
            return error("%s: failed to sign address claim for address %s",
                __func__,
                EncodeDestination(key_id));
        }

        LogPrint(LogFlags::VOTE,
            "%s: signed address claim for address: %s",
            __func__,
            EncodeDestination(key_id));

        return true;
    }

private:
    const CWallet& m_wallet; //!< Wallet to generate an address claim from.
}; // AddressClaimBuilder

//!
//! \brief Constructs a provable balance weight claim for voting.
//!
class BalanceClaimBuilder
{
public:
    //!
    //! \brief Initialize a balance claim builder.
    //!
    //! \param wallet Supplies outputs to generate a balance claim from.
    //!
    BalanceClaimBuilder(const CWallet& wallet) : m_wallet(wallet)
    {
    }

    //!
    //! \brief Generate a balance claim.
    //!
    //! \param claim         The object to fill with the claim.
    //! \param trace_origins Whether to claim the wallet's young recirculated
    //! outputs by their origins (CoinPicker::TraceOrigins).
    //! \param poll_txid     The poll the claim is for.
    //! \param pending_vote_output Set to the young output of one of the
    //! wallet's own votes that the picker claimed by its origins, preferring one
    //! that comes from a vote on \p poll_txid (CoinPicker::PickCoins); left as
    //! it is if none.
    //!
    void BuildClaim(
        BalanceClaim& claim,
        const bool trace_origins,
        const uint256& poll_txid,
        std::optional<COutPoint>& pending_vote_output) const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        const AddressClaimBuilder builder(m_wallet);
        OutputAddresses outputs_by_address = CoinPicker(m_wallet).PickCoins(trace_origins, poll_txid, pending_vote_output);

        TrimSmallestOutputs(outputs_by_address);
        claim.m_address_claims.reserve(outputs_by_address.size());

        for (auto& address : outputs_by_address) {
            if (auto claim_option = builder.TryBuildClaim(std::move(address))) {
                claim.m_address_claims.emplace_back(std::move(*claim_option));
            }
        }

        // A balance claim must submit addresses in ascending order. This
        // improves the performance of duplicate address validation:
        //
        std::sort(claim.m_address_claims.begin(), claim.m_address_claims.end());
    }

    //!
    //! \brief Generate a balance claim from an explicit list of the wallet's
    //! outputs.
    //!
    //! \throws VotingError If an output is not the wallet's, has no P2PK/P2PKH
    //! destination, pays an address the wallet holds no key for, or its address
    //! claim cannot be built: such an output is refused, never dropped.
    //!
    void BuildClaimFromOutpoints(BalanceClaim& claim, const std::vector<COutPoint>& outpoints) const
    {
        LOCK(m_wallet.cs_wallet);

        std::map<CKeyID, AddressOutputs> by_address;

        for (const auto& outpoint : outpoints) {
            const auto iter = m_wallet.mapWallet.find(outpoint.hash);

            if (iter == m_wallet.mapWallet.end() || outpoint.n >= iter->second.vout.size()) {
                throw VotingError(strprintf("%s is not a wallet output.", outpoint.ToString()));
            }

            const CTxOut& output = iter->second.vout[outpoint.n];
            CTxDestination dest;

            if (!ExtractDestination(output.scriptPubKey, dest) || !std::get_if<CKeyID>(&dest)) {
                throw VotingError(strprintf("%s has no single address.", outpoint.ToString()));
            }

            const CKeyID key_id = *std::get_if<CKeyID>(&dest);

            if (!m_wallet.HaveKey(key_id)) {
                throw VotingError(strprintf("%s: the wallet holds no key for its address.", outpoint.ToString()));
            }

            AddressOutputs& address = by_address.emplace(key_id, key_id).first->second;
            address.m_outpoints.push_back(outpoint);
            address.m_amounts.push_back(output.nValue);
            address.m_total_amount += output.nValue;
        }

        const AddressClaimBuilder builder(m_wallet);

        for (auto& address_pair : by_address) {
            auto claim_option = builder.TryBuildClaim(std::move(address_pair.second));

            if (!claim_option) {
                throw VotingError("Could not build an address claim for an output. See debug.log.");
            }

            claim.m_address_claims.emplace_back(std::move(*claim_option));
        }

        std::sort(claim.m_address_claims.begin(), claim.m_address_claims.end());
    }

    //!
    //! \brief Sign each of the address claims with their respective private
    //! keys.
    //!
    //! \param claim   The claim object to sign.
    //! \param message The message to sign for the claim.
    //!
    //! \return \c false if an error occurs while signing the claim.
    //!
    bool SignClaim(BalanceClaim& claim, const ClaimMessage& message) const
    {
        const AddressClaimBuilder builder(m_wallet);

        for (auto& address_claim : claim.m_address_claims) {
            LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: signing claim address %s",
                     __func__,
                     EncodeDestination(address_claim.m_public_key.GetID())
                     );

            if (!builder.SignClaim(address_claim, message)) {
                return false;
            }
        }

        return true;
    }

private:
    const CWallet& m_wallet; //!< Wallet to generate a balance claim from.

    //!
    //! \brief Remove outputs for the claim until it fits inside of a standard
    //! transaction.
    //!
    //! \param by_address Outputs for the claim grouped by address.
    //!
    static void TrimSmallestOutputs(OutputAddresses& by_address)
    {
        constexpr size_t pubkey_bytes = 50; // compressed (33) uncompressed (66)
        constexpr size_t signature_bytes = 70; // usually 68-72 bytes
        constexpr size_t address_claim_bytes = pubkey_bytes + signature_bytes;
        constexpr size_t txo_bytes = sizeof(COutPoint);

        size_t bytes_estimate = 0;

        for (const auto& address : by_address) {
            bytes_estimate += address_claim_bytes;
            bytes_estimate += txo_bytes * address.m_outpoints.size();
        }

        while (bytes_estimate > MAX_STANDARD_TX_SIZE - 5000) {
            AddressOutputs& address = by_address.back();

            LogPrint(LogFlags::VOTE, "%s: trimmed output claim from %s",
                __func__,
                EncodeDestination(address.m_key_id));

            address.m_outpoints.pop_back();
            bytes_estimate -= txo_bytes;

            if (address.m_outpoints.empty()) {
                LogPrint(LogFlags::VOTE, "%s: trimmed address claim %s",
                    __func__,
                    EncodeDestination(address.m_key_id));

                by_address.pop_back();
                bytes_estimate -= address_claim_bytes;
            }
        }
    }
}; // BalanceClaimBuilder

//!
//! \brief Constructs a provable magnitude weight claim for voting.
//!
class MagnitudeClaimBuilder
{
public:
    //!
    //! \brief Initialize a magnitude claim builder.
    //!
    //! \param wallet     Used to fetch the private key to sign the claim.
    //! \param researcher Supplies beacon and magnitude context.
    //!
    MagnitudeClaimBuilder(const CWallet& wallet, const ResearcherPtr researcher)
        : m_wallet(wallet)
        , m_researcher(std::move(researcher))
    {
    }

    //!
    //! \brief Generate a magnitude claim.
    //!
    //! \param claim The object to fill with the claim.
    //!
    void BuildClaim(MagnitudeClaim& claim) const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        if (m_researcher->Magnitude().Scaled() == 0) {
            LogPrint(LogFlags::VOTE, "%s: skipped zero magnitude", __func__);
            return;
        }

        const std::optional<Beacon> beacon = m_researcher->TryBeacon();

        // Avoid building a claim for a beacon that will expire soon:
        if (!beacon || beacon->Expired(GetAdjustedTime() + 15 * 60)) {
            LogPrint(LogFlags::VOTE, "%s: skipped no active beacon", __func__);
            return;
        }

        const std::optional<uint256> beacon_txid = FindBeaconTxid(*beacon);

        if (!beacon_txid) {
            LogPrint(LogFlags::VOTE, "%s: beacon tx not found", __func__);
            return;
        }

        claim.m_mining_id = m_researcher->Id();
        claim.m_beacon_txid = *beacon_txid;
    }

    //!
    //! \brief Sign the claim with the beacon private key.
    //!
    //! \param claim   The claim object to sign.
    //! \param message The message to sign for the claim.
    //!
    //! \return \c false if an error occurs while signing the claim.
    //!
    bool SignClaim(MagnitudeClaim& claim, const ClaimMessage& message) const
    {
        if (claim.m_mining_id.Which() != MiningId::Kind::CPID) {
            LogPrint(LogFlags::VOTE, "%s: no beacon signature needed", __func__);
            return true;
        }

        const std::optional<Beacon> beacon = m_researcher->TryBeacon();

        if (!beacon) {
            // Should never happen:
            return error("%s: beacon disappeared", __func__);
        }

        CKey private_key;

        if (!m_wallet.GetKey(beacon->GetId(), private_key)) {
            return error("%s: failed to load beacon key from wallet", __func__);
        }

        if (!private_key.IsValid()) {
            return error("%s: invalid beacon private key", __func__);
        }

        if (!claim.Sign(private_key, message)) {
            claim.m_mining_id = MiningId::ForNoncruncher();
            return error("%s: failed to sign magnitude claim");
        }

        LogPrint(LogFlags::VOTE, "%s: signed magnitude claim for CPID %s",
            __func__,
            claim.m_mining_id.ToString());

        return true;
    }

private:
    const CWallet& m_wallet;          //!< Supplies the private signing key.
    const ResearcherPtr m_researcher; //!< Supplies beacon/magnitude context.

    //!
    //! \brief Get the hash of the transaction that contains the contract for
    //! the beacon used to sign the claim.
    //!
    //! \param beacon The beacon used to sign the claim for matching to the
    //! transaction.
    //!
    //! \return The hash of the transaction for the beacon contract if found.
    //!
    std::optional<uint256> FindBeaconTxid(const Beacon& beacon) const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        // TODO: This is rather slow, but we only need to do it once per vote.
        // Store a reference to a wallet's beacon transactions and rewrite the
        // lookup to avoid the chain scan:

        const CBlockIndex* pindex = pindexBest;

        if (!pindex) {
            return std::nullopt;
        }

        const int64_t max_time = FutureDrift(beacon.m_timestamp, 0);
        const int64_t min_time = pindex->nTime - Beacon::MAX_AGE;
        CBlock block;

        for (; pindex && pindex->nTime > max_time; pindex = pindex->pprev);

        for (; pindex && pindex->nTime > min_time; pindex = pindex->pprev) {
            if (!pindex->IsContract()) {
                continue;
            }

            if (!ReadBlockFromDisk(block, pindex, Params().GetConsensus())) {
                break;
            }

            for (const auto& tx : block.vtx) {
                for (const auto& contract : tx.GetContracts()) {
                    if (contract.m_type != ContractType::BEACON) {
                        continue;
                    }

                    auto payload = contract.SharePayloadAs<BeaconPayload>();

                    if (payload->m_beacon.m_public_key == beacon.m_public_key) {
                        return tx.GetHash();
                    }
                }
            }
        }

        return std::nullopt;
    }
}; // MagnitudeClaimBuilder

//!
//! \brief Constructs a provable poll eligibility claim.
//!
class PollClaimBuilder
{
public:
    //!
    //! \brief Initialize a poll claim builder.
    //!
    //! \param wallet Supplies balance context and signing keys.
    //!
    PollClaimBuilder(const CWallet& wallet) : m_wallet(wallet)
    {
    }

    //!
    //! \brief Generate a poll eligibility claim.
    //!
    //! \param poll Poll contract to generate the claim for.
    //!
    PollEligibilityClaim BuildClaim(const Poll& poll) const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        // The claim version of the next block, which is the block that can take the poll and the height the memory
        // pool judges it at.
        if (!IsPollMultiAddressEnabled(nBestHeight + 1)) {
            return BuildV1Claim(poll);
        }

        PollEligibilityClaim claim;
        claim.m_version = 2;

        // STEP 1: Get ALL outputs, sort GLOBALLY by size (same as votes)
        std::vector<COutput> outputs;
        m_wallet.AvailableCoins(outputs, true, nullptr, true);

        const auto descending_by_amount = [](const COutput& a, const COutput& b) {
            return a.tx->vout[a.i].nValue > b.tx->vout[b.i].nValue;
        };
        std::sort(outputs.begin(), outputs.end(), descending_by_amount);

        LogPrint(LogFlags::VOTE, "%s: wallet supplied %" PRIszu " outputs",
            __func__, outputs.size());

        // STEP 2: Select top outputs (up to MAX_OUTPOINTS)
        std::map<CKeyID, std::vector<COutPoint>> selected_by_address;
        CAmount total_amount = 0;
        size_t outpoint_count = 0;

        for (const auto& output : outputs) {
            if (outpoint_count >= PollEligibilityClaim::MAX_OUTPOINTS) break;

            // Filter same as votes: >= 1 GRC, confirmed, P2PKH only
            if (output.tx->vout[output.i].nValue < COIN) break;
            if (!output.tx->IsTrusted() || !output.tx->IsConfirmed()) continue;

            CTxDestination dest;
            if (!ExtractDestination(output.tx->vout[output.i].scriptPubKey, dest)) continue;

            const CKeyID* keyId = std::get_if<CKeyID>(&dest);
            if (!keyId) continue;

            // Add to selection
            selected_by_address[*keyId].emplace_back(output.tx->GetHash(), output.i);
            total_amount += output.tx->vout[output.i].nValue;
            ++outpoint_count;

            LogPrint(LogFlags::VOTE,
                "  selected output for address %s: %s GRC (total: %s)",
                EncodeDestination(*keyId),
                FormatMoney(output.tx->vout[output.i].nValue),
                FormatMoney(total_amount));

            // Early exit if we have enough
            if (total_amount >= POLL_REQUIRED_BALANCE) break;
        }

        // STEP 3: Verify sufficient balance
        if (total_amount < POLL_REQUIRED_BALANCE) {
            throw VotingError(strprintf(
                _("Wallet balance of %s in top %s UTXOs is less than required %s GRC."),
                FormatMoney(total_amount),
                ToString(outpoint_count),
                FormatMoney(POLL_REQUIRED_BALANCE)));
        }

        // STEP 4: Build address claims (same as votes)
        const AddressClaimBuilder address_builder(m_wallet);
        claim.m_balance_claim.m_address_claims.reserve(selected_by_address.size());

        for (auto& [key_id, outpoints] : selected_by_address) {
            AddressOutputs addr_outputs(key_id);
            addr_outputs.m_outpoints = std::move(outpoints);

            if (auto claim_option = address_builder.TryBuildClaim(std::move(addr_outputs))) {
                claim.m_balance_claim.m_address_claims.emplace_back(
                    std::move(*claim_option));
            }
        }

        // STEP 5: Sort address claims
        std::sort(claim.m_balance_claim.m_address_claims.begin(),
                  claim.m_balance_claim.m_address_claims.end());

        LogPrint(LogFlags::VOTE, "%s: Built poll claim: %s addresses, %s UTXOs, %s GRC",
            __func__,
            ToString(claim.m_balance_claim.m_address_claims.size()),
            ToString(outpoint_count),
            FormatMoney(total_amount));

        return claim;
    }

    //!
    //! \brief Sign the claim with each of the private keys.
    //!
    //! \param payload The poll to sign the claim for.
    //! \param tx      The transaction that will contain the poll.
    //!
    //! \return \c false if an error occurs while signing the claim.
    //!
    bool SignClaim(PollPayload& payload, const CWalletTx& tx) const
    {
        const ClaimMessage message = PackPollMessage(payload.m_poll, tx);
        const AddressClaimBuilder builder(m_wallet);

        // Sign all address claims in the balance claim
        for (auto& address_claim : payload.m_claim.m_balance_claim.m_address_claims) {
            if (!builder.SignClaim(address_claim, message)) {
                return false;
            }
        }

        return true;
    }

private:
    const CWallet& m_wallet; //!< Supplies balance context and signing keys.

    //!
    //! \brief Generate a v1 single-address poll eligibility claim.
    //!
    //! Used before PollMultiAddressHeight activation to maintain backward
    //! compatibility with nodes that do not understand v2 claims.
    //!
    //! \param poll Poll contract to generate the claim for.
    //!
    //! Build a v1 (pre-gate) single-address poll eligibility claim.
    //!
    //! The selection is dust-resistant per address:
    //!
    //!   1. Group available outputs by address, recording each address's total
    //!      balance.
    //!   2. Filter to candidate addresses (per-address total >= threshold).
    //!      Without this filter, the post-sort walk in step 4 might include
    //!      every address; with it, the loop only runs over plausible
    //!      candidates.
    //!   3. Sort candidates by total balance DESCENDING. Larger-balance
    //!      addresses are most likely to reach the threshold in the fewest
    //!      outpoints, so try them first.
    //!   4. For each candidate, sort its outpoints by value DESCENDING and
    //!      accumulate top-down until either (a) the running sum reaches
    //!      POLL_REQUIRED_BALANCE -- success, claim is built from the
    //!      accumulated outpoints, or (b) the outpoint count hits
    //!      MAX_OUTPOINTS without reaching the threshold -- move to the next
    //!      candidate. Walking largest-first means the included outpoints
    //!      actually sum to the threshold (the previous implementation kept
    //!      the FIRST 250 in wallet-iteration order, which on a dust-heavy
    //!      address could build a claim the verifier would reject).
    //!   5. If no candidate succeeds, the wallet is fragmented enough that
    //!      no single address can attest to the threshold in 250 or fewer
    //!      outpoints; report that with a hint to consolidate.
    //!
    //! No up-front m_wallet.GetBalance() check is performed here. GetBalance()
    //! excludes immature coinstake outputs, but AvailableCoins(.., fIncludeStakingCoins=true)
    //! below includes them and the verifier does not enforce maturity. An
    //! up-front balance check with the wrong source produces false-negative
    //! rejections on borderline wallets, so the per-walk checks above are the
    //! authoritative ones.
    PollEligibilityClaim BuildV1Claim(const Poll& poll) const
    {
        std::vector<COutput> outputs;
        m_wallet.AvailableCoins(outputs, true, nullptr, true);

        // Group outputs by address, populating both m_outpoints and m_amounts
        // via AddressOutputs::Add() (parallel arrays kept consistent).
        std::map<CKeyID, AddressOutputs> outputs_by_address;
        for (const auto& output : outputs) {
            if (output.tx->vout[output.i].nValue < COIN) continue;
            if (!output.tx->IsTrusted() || !output.tx->IsConfirmed()) continue;

            CTxDestination dest;
            if (!ExtractDestination(output.tx->vout[output.i].scriptPubKey, dest)) continue;

            const CKeyID* keyId = std::get_if<CKeyID>(&dest);
            if (!keyId) continue;

            auto it = outputs_by_address.find(*keyId);
            if (it == outputs_by_address.end()) {
                it = outputs_by_address.emplace(*keyId, AddressOutputs(*keyId)).first;
            }
            it->second.Add(output);
        }

        // 2. Filter to candidates: per-address total >= POLL_REQUIRED_BALANCE.
        std::vector<AddressOutputs> candidates;
        for (auto& [key_id, addr] : outputs_by_address) {
            if (addr.m_total_amount >= POLL_REQUIRED_BALANCE) {
                candidates.push_back(std::move(addr));
            }
        }

        if (candidates.empty()) {
            throw VotingError(strprintf(
                _("No single address has the required %s GRC balance to create a poll."),
                FormatMoney(POLL_REQUIRED_BALANCE)));
        }

        // 3. Sort candidate addresses descending by total balance.
        std::sort(candidates.begin(), candidates.end(), std::greater<AddressOutputs>());

        // 4. For each candidate, sort its outpoints by amount descending and
        //    accumulate top-down until threshold or cap.
        const AddressClaimBuilder address_builder(m_wallet);

        for (auto& cand : candidates) {
            // Sort this address's outpoints by amount descending, keeping the
            // parallel m_amounts array in lockstep. Indices are co-sorted via
            // a temporary index vector to avoid the parallel-sort hazard.
            const size_t n = cand.m_outpoints.size();
            std::vector<size_t> idx(n);
            std::iota(idx.begin(), idx.end(), 0);
            std::sort(idx.begin(), idx.end(),
                [&cand](size_t a, size_t b) { return cand.m_amounts[a] > cand.m_amounts[b]; });

            AddressOutputs picked(cand.m_key_id);
            for (size_t k = 0; k < n; ++k) {
                if (picked.m_outpoints.size() >= PollEligibilityClaim::MAX_OUTPOINTS) {
                    break;
                }
                const size_t i = idx[k];
                picked.m_outpoints.push_back(cand.m_outpoints[i]);
                picked.m_amounts.push_back(cand.m_amounts[i]);
                picked.m_total_amount += cand.m_amounts[i];

                if (picked.m_total_amount >= POLL_REQUIRED_BALANCE) {
                    if (auto claim_option = address_builder.TryBuildClaim(std::move(picked))) {
                        PollEligibilityClaim claim;
                        claim.m_version = 1;
                        claim.m_balance_claim.m_address_claims.push_back(std::move(*claim_option));

                        LogPrint(LogFlags::VOTE,
                            "%s: Built v1 poll claim for address %s with %" PRIszu " UTXOs",
                            __func__,
                            EncodeDestination(cand.m_key_id),
                            claim.m_balance_claim.m_address_claims[0].m_outpoints.size());

                        return claim;
                    }
                    // Signing failed for this candidate; fall through to the next.
                    break;
                }
            }
            // Hit MAX_OUTPOINTS cap before reaching threshold; try next candidate.
        }

        // 5. All candidates exhausted: heavily dust-fragmented within each
        //    qualifying address. v2 (post-gate) would succeed because it can
        //    combine outpoints across addresses.
        throw VotingError(strprintf(
            _("No single address can attest to %s GRC in %u or fewer UTXOs. "
              "Consider consolidating UTXOs."),
            FormatMoney(POLL_REQUIRED_BALANCE),
            static_cast<unsigned>(PollEligibilityClaim::MAX_OUTPOINTS)));
    }
}; // PollClaimBuilder

//!
//! \brief Constructs a provable voting weight claim.
//!
class VoteClaimBuilder
{
public:
    //!
    //! \brief Initialize a vote claim builder.
    //!
    //! \param wallet     Supplies balance context and signing keys.
    //! \param researcher Supplies magnitude claim context.
    //!
    VoteClaimBuilder(const CWallet& wallet, const ResearcherPtr researcher)
        : m_balance_builder(wallet)
        , m_magnitude_builder(wallet, researcher)
    {
    }

    //!
    //! \brief Generate a voting weight claim.
    //!
    //! \param vote          Vote contract to generate the claim for.
    //! \param poll          Determines how to generate a magnitude claim.
    //! \param trace_origins Whether the balance claim may name the origins of
    //! the wallet's young recirculated outputs (CoinPicker::TraceOrigins).
    //! \param pending_vote_output Set to the young output of one of the
    //! wallet's own votes that the coin picker claimed by its origins,
    //! preferring one that comes from a vote on this poll
    //! (CoinPicker::PickCoins); left as it is if none.
    //!
    void BuildClaim(
        Vote& vote,
        const Poll& poll,
        const bool trace_origins,
        std::optional<COutPoint>& pending_vote_output) const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        VoteWeightClaim& claim = vote.m_claim;

        m_balance_builder.BuildClaim(claim.m_balance_claim, trace_origins, vote.m_poll_txid, pending_vote_output);

        if (poll.IncludesMagnitudeWeight()) {
            m_magnitude_builder.BuildClaim(claim.m_magnitude_claim);
        }
    }

    //!
    //! \brief Generate a voting weight claim from an explicit output list.
    //!
    //! \param vote      Vote contract to generate the claim for.
    //! \param outpoints Outputs the balance claim lists.
    //!
    void BuildClaimFromOutpoints(Vote& vote, const std::vector<COutPoint>& outpoints) const
    {
        m_balance_builder.BuildClaimFromOutpoints(vote.m_claim.m_balance_claim, outpoints);
    }

    //!
    //! \brief Sign the claim with each of the private keys.
    //!
    //! \param vote The vote to sign the claim for.
    //! \param tx   The transaction that will contain the vote.
    //!
    //! \return \c false if an error occurs while signing the claim.
    //!
    bool SignClaim(Vote& vote, const CWalletTx& tx) const
    {
        const ClaimMessage message = PackVoteMessage(vote, tx);
        VoteWeightClaim& claim = vote.m_claim;

        return m_balance_builder.SignClaim(claim.m_balance_claim, message)
            && m_magnitude_builder.SignClaim(claim.m_magnitude_claim, message);
    }

private:
    BalanceClaimBuilder m_balance_builder;     //!< Constructs balance claims.
    MagnitudeClaimBuilder m_magnitude_builder; //!< Constructs magnitude claims.
}; // VoteClaimBuilder

//!
//! \brief Select the outputs to spend for the voting transaction.
//!
//! This produces a dummy transaction used to sign the voting claims. The
//! resulting transaction contains inputs selected from the wallet that a
//! final transaction will contain. We take these input UTXOs to sign the
//! claims in the original transaction. By embedding those input contexts
//! in the claim signatures, we prevent replay of the voting contracts.
//!
//! TODO: refactor the wallet API to avoid the need to build and sign the
//! intermediate transaction.
//!
//! \param wallet Wallet to create the mock transaction from.
//! \param tx     Template with a contract to base the mock on.
//!
//! \throws VotingError When the wallet does contain an available balance
//! large enough to settle the cost of the transaction or when the wallet
//! encounters an error while building the transaction.
//!
//! Pick a CCoinControl set of fee-paying inputs for a contract tx.
//!
//! The default wallet coin-selection for contract txs (SelectSmallestCoins
//! in CWallet::SelectCoins) prefers the smallest available UTXOs in order
//! to preserve large coins for staking. That heuristic is fine when the
//! wallet has a modest UTXO set, but it pathologically blows up the tx
//! size on dust-fragmented wallets -- on a wallet with tens of thousands
//! of dust outputs, the resulting tx can easily exceed the 100 KB standard
//! transaction size limit and be rejected by CreateTransaction with an
//! opaque "tx size too large" error.
//!
//! For contract txs (poll-creation, voting), the right policy is the
//! opposite: pick the smallest set of LARGEST UTXOs that suffices. A
//! single sufficiently-large UTXO produces a minimum-size tx; if no single
//! UTXO is large enough, accumulate top-down by amount.
//!
//! \param wallet The wallet whose UTXOs are eligible for fee payment.
//! \param needed The minimum amount the selection must cover (burn fee
//! plus a small headroom for the tx-fee that CreateTransaction will
//! itself add).
//! \param explicit_outpoint If set, the user has pinned an exact UTXO
//! (coin-control). Use only that outpoint; fail if it cannot cover the
//! requirement.
//! \param pending_vote_output If set, a young output of one of the wallet's own
//! votes that a vote's claim names by its origins. When no outpoint is pinned
//! and it alone covers the requirement, it is the only selection.
//! \return A CCoinControl with the selected outpoints pinned. If no
//! selection covers \p needed the returned CCoinControl contains a partial
//! selection; the downstream CreateTransaction call reports the
//! insufficient-funds error via its existing path. The helper itself never
//! throws.
inline CCoinControl PickLargestFirstFeeInputs(
    CWallet& wallet,
    const CAmount& needed,
    const std::optional<COutPoint>& explicit_outpoint,
    const std::optional<COutPoint>& pending_vote_output)
{
    CCoinControl coin_control;

    if (explicit_outpoint) {
        // User-specified — pin exactly this UTXO. If it can't cover, the
        // downstream CreateTransaction call will fail and we throw the
        // existing insufficient-funds path. Don't validate the outpoint
        // here (it could be unconfirmed, conflicted, not ours, spent, etc.)
        // — let AvailableCoins filter it out and let the wallet report.
        coin_control.Select(*explicit_outpoint);
        return coin_control;
    }

    // The outputs AvailableCoins returns point into the wallet's transactions,
    // so the wallet stays locked while they are read.
    LOCK2(cs_main, wallet.cs_wallet);

    // Default: accumulate largest-first until covered. Pass a CCoinControl
    // with fAllowWatchOnly=false (the default) to AvailableCoins so the
    // candidate set excludes watch-only outputs -- selecting a large
    // watch-only UTXO would propagate to CreateTransaction and fail
    // signing because we don't hold the key.
    std::vector<COutput> available;
    CCoinControl filter;
    wallet.AvailableCoins(available, true, &filter, false);

    // When a vote's claim names the origins of an earlier vote's young change
    // and that change covers the requirement, the vote pays its fee from it.
    // The new vote is then the earlier vote's child and cannot confirm before
    // it. The tally gives a coin's weight to the newest vote that reaches it.
    if (pending_vote_output) {
        for (const auto& utxo : available) {
            if (COutPoint(utxo.tx->GetHash(), utxo.i) == *pending_vote_output
                && utxo.tx->vout[utxo.i].nValue >= needed)
            {
                coin_control.Select(*pending_vote_output);
                return coin_control;
            }
        }
    }

    std::sort(available.begin(), available.end(),
        [](const COutput& a, const COutput& b) {
            return a.tx->vout[a.i].nValue > b.tx->vout[b.i].nValue;
        });

    CAmount accumulated = 0;
    for (const auto& utxo : available) {
        coin_control.Select(COutPoint(utxo.tx->GetHash(), utxo.i));
        accumulated += utxo.tx->vout[utxo.i].nValue;
        if (accumulated >= needed) break;
    }

    // If we didn't find enough, return the partial selection anyway.
    // CreateTransaction will report Insufficient funds via the existing
    // error path -- consistent with the previous behavior for under-funded
    // wallets.
    return coin_control;
}

template <typename PayloadType>
void SelectFinalInputs(CWallet& wallet, CWalletTx& tx,
                       const std::optional<COutPoint>& explicit_fee_outpoint = std::nullopt,
                       const std::optional<COutPoint>& pending_vote_output = std::nullopt)
{
    CWalletTx mock_tx = tx;

    // Expand the incomplete claim signatures to provide a more realistic
    // transaction size that the wallet will base the input selection on.
    // SharePayload() returns a shared_ptr copy, so we can modify the payload
    // even though vContracts is const.
    //
    mock_tx.vContracts.back().SharePayload().As<PayloadType>().m_claim.ExpandDummySignatures();

    const CAmount burn_fee = mock_tx.vContracts.back().RequiredBurnAmount();
    CReserveKey reserve_key(&wallet); // unused
    // Initialised, not left to CreateTransaction. The failure branch below
    // reads it, and CreateTransaction can now refuse before it seeds the fee
    // (a staking-only wallet returns at the scope check), which would make that
    // read indeterminate.
    CAmount out_applied_fee = 0;

    // Pre-select fee-paying inputs largest-first (or honor the user's
    // explicit outpoint, or pay from an earlier vote's young change, as
    // PickLargestFirstFeeInputs describes). This avoids CreateTransaction's default
    // SelectSmallestCoins() path for contract txs, which on a dust-heavy
    // wallet selects hundreds of small UTXOs and blows past the 100 KB
    // standard tx size limit.
    //
    // Headroom = COIN (1 GRC) covers any reasonable per-byte fee that
    // CreateTransaction will add for a contract-sized tx. If the headroom
    // turns out to be insufficient (very unlikely on a normal fee
    // schedule), CreateTransaction will return false and we throw via the
    // existing Insufficient-funds path.
    const CAmount needed = burn_fee + COIN;
    CCoinControl coin_control = PickLargestFirstFeeInputs(
        wallet, needed, explicit_fee_outpoint, pending_vote_output);

    if (!wallet.CreateTransaction(
        CScript() << OP_RETURN,
        burn_fee,
        mock_tx,
        reserve_key,
        out_applied_fee,
        &coin_control))
    {
        if (burn_fee + out_applied_fee > wallet.GetBalance()) {
            throw VotingError(_("Insufficient funds."));
        }

        throw VotingError(_("Could not create transaction. See debug.log."));
    }

    // Copy the selected inputs back to the original tx
    CMutableTransaction mtx(tx);
    mtx.vin = CMutableTransaction(mock_tx).vin;
    static_cast<CTransaction&>(tx) = CTransaction(std::move(mtx));
}

//!
//! \brief Whether a poll was mined at or after the v15 height.
//!
//! Votes on such a poll may claim the wallet's recirculated change by its
//! origin (CoinPicker::TraceOrigins). This is the tally's decision for the
//! poll: the height of the block that contains the poll transaction.
//!
//! \param poll_txid Hash of the transaction that contains the poll.
//!
bool PollStartsAtOrAfterV15(const uint256& poll_txid) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    CTransaction tx;
    uint256 block_hash;

    if (!GetTransaction(poll_txid, tx, block_hash) || block_hash.IsNull()) {
        return false;
    }

    const auto iter = mapBlockIndex.find(block_hash);

    return iter != mapBlockIndex.end()
        && iter->second
        && iter->second->IsInMainChain()
        && IsV15Enabled(iter->second->nHeight);
}
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Global Functions
// -----------------------------------------------------------------------------

uint256 GRC::SendPollContract(PollBuilder builder)
{
    std::pair<CWalletTx, std::string> result_pair;

    {
        LOCK2(cs_main, pwalletMain->cs_wallet);

        uint32_t contract_version = IsV13Enabled(nBestHeight) ? 3 : 2;

        result_pair = SendContract(builder.BuildContractTx(pwalletMain, contract_version));
    }

    if (!result_pair.second.empty()) {
        throw VotingError(result_pair.second);
    }

    return result_pair.first.GetHash();
}

uint256 GRC::SendVoteContract(VoteBuilder builder)
{
    std::pair<CWalletTx, std::string> result_pair;

    {
        LOCK2(cs_main, pwalletMain->cs_wallet);

        uint32_t contract_version = IsV13Enabled(nBestHeight) ? 3 : 2;

        result_pair = SendContract(builder.BuildContractTx(pwalletMain, contract_version));
    }

    if (!result_pair.second.empty()) {
        throw VotingError(result_pair.second);
    }

    return result_pair.first.GetHash();
}

// -----------------------------------------------------------------------------
// Class: PollBuilder
// -----------------------------------------------------------------------------

PollBuilder::PollBuilder()
    : m_poll(std::make_unique<Poll>())
    , m_fee_outpoint(std::nullopt)
{
}

PollBuilder::PollBuilder(PollBuilder&& builder) = default;
PollBuilder::~PollBuilder() = default;
PollBuilder& PollBuilder::operator=(PollBuilder&& builder) = default;

PollBuilder PollBuilder::SetPayloadVersion(uint32_t version) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    // The payload version of the next block, which is the block that can take the poll.
    bool v3_enabled = IsPollV3Enabled(nBestHeight + 1);

    if ((v3_enabled && version < 3)
            || (!v3_enabled && version >= 3)) {
        throw VotingError(_("Wrong Payload version specified for current block height."));
    }

    m_poll_payload_version = version;

    return std::move(*this);
}

PollBuilder PollBuilder::SetType(const PollType type)
{
    if (type <= PollType::UNKNOWN || type >= PollType::OUT_OF_BOUND) {
        throw VotingError(_("Unknown poll type."));
    }

    m_poll->m_type = type;

    return std::move(*this);
}

PollBuilder PollBuilder::SetType(const int64_t type)
{
    return SetType(static_cast<PollType>(type));
}

PollBuilder PollBuilder::SetWeightType(const PollWeightType type)
{
    // Deprecated weighing methods are not supported by version 2+ polls in
    // block version 11 and greater:
    //
    switch (type) {
        case PollWeightType::BALANCE:
        case PollWeightType::BALANCE_AND_MAGNITUDE:
            break;
        case PollWeightType::MAGNITUDE:
            throw VotingError(_("Magnitude-only polls are not supported."));
        case PollWeightType::CPID_COUNT:
            throw VotingError(_("CPID count polls are not supported."));
        case PollWeightType::PARTICIPANT_COUNT:
            throw VotingError(_("Participant count polls are not supported."));
        default:
            throw VotingError(_("Unknown poll weight type."));
    }

    m_poll->m_weight_type = type;

    return std::move(*this);
}

PollBuilder PollBuilder::SetWeightType(const int64_t type)
{
    return SetWeightType(static_cast<PollWeightType>(type));
}

PollBuilder PollBuilder::SetResponseType(const PollResponseType type)
{
    if (type <= PollResponseType::UNKNOWN || type >= PollResponseType::OUT_OF_BOUND) {
        throw VotingError(_("Unknown poll response type."));
    }

    m_poll->m_response_type = type;

    return std::move(*this);
}

PollBuilder PollBuilder::SetResponseType(const int64_t type)
{
    return SetResponseType(static_cast<PollResponseType>(type));
}

PollBuilder PollBuilder::SetDuration(const uint32_t days)
{
    uint32_t min_duration_days = std::max(Poll::MIN_DURATION_DAYS,
                                          Poll::POLL_TYPE_RULES[m_poll->m_type.Raw()].m_mininum_duration);

    if (days < min_duration_days) {
        throw VotingError(strprintf(
            _("Poll duration must be at least %s days."),
            ToString(min_duration_days)));
    }

    // The protocol allows poll durations up to 180 days. To limit unhelpful
    // or unintentional poll durations, user-facing pieces discourage a poll
    // longer than:
    //
    constexpr uint32_t max_duration_days = 90;

    if (days > max_duration_days) {
        throw VotingError(strprintf(
            _("Poll duration cannot exceed %s days."),
            ToString(max_duration_days)));
    }

    m_poll->m_duration_days = days;

    return std::move(*this);
}

PollBuilder PollBuilder::SetTitle(std::string title)
{
    title = TrimString(title);

    if (title.empty()) {
        throw VotingError(_("Please enter a poll title."));
    }

    if (title.size() > Poll::MAX_TITLE_SIZE) {
        throw VotingError(strprintf(
            _("Poll title cannot exceed %s characters."),
            ToString(Poll::MAX_TITLE_SIZE)));
    }

    m_poll->m_title = title;

    return std::move(*this);
}

PollBuilder PollBuilder::SetUrl(std::string url)
{
    if (url.empty()) {
        throw VotingError(_("Please enter a poll discussion website URL."));
    }

    if (url.size() > Poll::MAX_URL_SIZE) {
        throw VotingError(strprintf(
            _("Poll discussion URL cannot exceed %s characters."),
            ToString(Poll::MAX_URL_SIZE)));
    }

    m_poll->m_url = url;

    return std::move(*this);
}

PollBuilder PollBuilder::SetQuestion(std::string question)
{
    if (question.size() > Poll::MAX_QUESTION_SIZE) {
        throw VotingError(strprintf(
            _("Poll question cannot exceed %s characters."),
            ToString(Poll::MAX_QUESTION_SIZE)));
    }

    m_poll->m_question = question;

    return std::move(*this);
}

PollBuilder PollBuilder::SetChoices(std::vector<std::string> labels)
{
    m_poll->m_choices = Poll::ChoiceList();

    return AddChoices(labels);
}

PollBuilder PollBuilder::AddChoices(std::vector<std::string> labels)
{
    for (auto& label : labels) {
        *this = AddChoice(label);
    }

    return std::move(*this);
}

PollBuilder PollBuilder::AddChoice(std::string label)
{
    label = TrimString(label);

    if (label.empty()) {
        throw VotingError(_("A poll choice cannot be empty."));
    }

    if (m_poll->m_choices.size() + 1 > POLL_MAX_CHOICES_SIZE) {
        throw VotingError(strprintf(
            _("Poll cannot contain more than %s choices."),
            ToString(POLL_MAX_CHOICES_SIZE)));
    }

    if (label.size() > Poll::Choice::MAX_LABEL_SIZE) {
        throw VotingError(strprintf(
            _("Poll choice \"%s\" exceeds %s characters."),
            label,
            ToString(Poll::Choice::MAX_LABEL_SIZE)));
    }

    if (m_poll->m_choices.LabelExists(label)) {
        throw VotingError(strprintf(_("Duplicate poll choice: %s"), label));
    }

    m_poll->m_choices.Add(label);

    return std::move(*this);
}

PollBuilder PollBuilder::SetAdditionalFields(std::vector<Poll::AdditionalField> fields)
{
    m_poll->m_additional_fields = Poll::AdditionalFieldList();

    return AddAdditionalFields(fields);
}

PollBuilder PollBuilder::SetAdditionalFields(Poll::AdditionalFieldList fields)
{
    m_poll->m_additional_fields = Poll::AdditionalFieldList();

    return AddAdditionalFields(fields);
}

PollBuilder PollBuilder::AddAdditionalFields(std::vector<Poll::AdditionalField> fields)
{
    for (auto& field : fields) {
        *this = AddAdditionalField(field);
    }

    if (!m_poll->m_additional_fields.WellFormed(m_poll->m_type.Value())) {
        throw VotingError(_("The field list is not well-formed."));
    }

    return std::move(*this);
}

PollBuilder PollBuilder::AddAdditionalFields(Poll::AdditionalFieldList fields)
{
    for (auto& field : fields) {
        *this = AddAdditionalField(field);
    }

    if (!m_poll->m_additional_fields.WellFormed(m_poll->m_type.Value())) {
        throw VotingError(_("The field list is not well-formed."));
    }

    return std::move(*this);
}

PollBuilder PollBuilder::AddAdditionalField(Poll::AdditionalField field)
{
    // Make sure there are no leading and trailing spaces.
    field.m_name = TrimString(field.m_name);
    field.m_value = TrimString(field.m_value);
    field.m_required = field.m_required;

    if (!field.WellFormed()) {
        throw VotingError(_("The field is not well-formed."));
    }

    if (m_poll->m_additional_fields.size() + 1 > POLL_MAX_ADDITIONAL_FIELDS_SIZE) {
        throw VotingError(strprintf(
                              _("Poll cannot contain more than %s additional fields"),
                              ToString(POLL_MAX_ADDITIONAL_FIELDS_SIZE)));
    }

    if (field.m_name.size() > Poll::AdditionalField::MAX_NAME_SIZE) {
        throw VotingError(strprintf(
                              _("Poll additional field name \"%s\" exceeds %s characters."),
                              field.m_name,
                              ToString(Poll::AdditionalField::MAX_NAME_SIZE)));
    }

    if (field.m_value.size() > Poll::AdditionalField::MAX_VALUE_SIZE) {
        throw VotingError(strprintf(
                              _("Poll additional field value \"%s\" for field name \"%s\" exceeds %s characters."),
                              field.m_value,
                              field.m_name,
                              ToString(Poll::AdditionalField::MAX_VALUE_SIZE)));
    }

    if (m_poll->m_additional_fields.FieldExists(field.m_name)) {
        throw VotingError(strprintf(_("Duplicate poll additional field: %s"), field.m_name));
    }

    m_poll->m_additional_fields.Add(field);

    return std::move(*this);
}

PollBuilder PollBuilder::SetFeeOutpoint(const COutPoint& outpoint)
{
    m_fee_outpoint = outpoint;
    return std::move(*this);
}

CWalletTx PollBuilder::BuildContractTx(CWallet* const pwallet, const uint32_t& contract_version) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    if (!pwallet) {
        throw VotingError(_("No wallet available."));
    }

    // Building the contract signs claims and the transaction with the wallet's
    // keys, so the wallet must be fully unlocked, or unlocked for staking only
    // with a capability scope open on this thread. One read of the scope
    // answers which refusal applies.
    switch (pwallet->GetUnlockScope()) {
    case UnlockScope::Locked:
        throw VotingError(_("Error: Wallet locked, unable to create transaction."));
    case UnlockScope::StakingOnly:
        if (!pwallet->CanUseKeys()) {
            throw VotingError(_("Error: Wallet unlocked for staking only, unable to create transaction."));
        }
        break;
    case UnlockScope::Full:
        break;
    }

    if (m_poll->m_response_type == PollResponseType::YES_NO_ABSTAIN) {
        if (!m_poll->m_choices.empty()) {
            throw VotingError(_(
                "A poll with a yes/no/abstain response type cannot include "
                "any additional custom choices."));
        }
    } else {
        if (m_poll->m_choices.size() < 2) {
            throw VotingError(_("Please enter at least two poll choices."));
        }
    }

    // Refuse a title whose key is taken: by a poll registered since V11, active or deleted (the title index),
    // or by a poll in the memory pool. From v15 a poll with a taken title is mined but registers nothing and its burn
    // is spent; before v15 it is registered by its txid. The registry lock is scoped to the lookup. A poll
    // that takes the title after this check is met by the memory pool's refusal, or by the eviction of same-title
    // polls when a block takes the title.
    const std::string title_key = PollTitleKey(m_poll->m_title);

    if (WITH_LOCK(GRC::PollRegistry::cs_poll_registry, return GRC::GetPollRegistry().TitleOwner(title_key).has_value())
        || mempool.HasPollForTitle(title_key)) {
        throw VotingError(_("Poll with that title already exists. Please choose another title."));
    }

    CWalletTx tx;

    // Not necessary but allows the RPC to report expiration:
    m_poll->m_timestamp = tx.nTime;

    const PollClaimBuilder claim_builder(*pwallet);
    PollEligibilityClaim claim = claim_builder.BuildClaim(*m_poll);

    {
        CMutableTransaction mtx(tx);
        mtx.vContracts.emplace_back(MakeContract<PollPayload>(
                                       contract_version,
                                       ContractAction::ADD,
                                       std::move(m_poll_payload_version),
                                       std::move(*m_poll),
                                       std::move(claim)));
        static_cast<CTransaction&>(tx) = CTransaction(std::move(mtx));
    }

    SelectFinalInputs<PollPayload>(*pwallet, tx, m_fee_outpoint);
    PollPayload& poll_payload = tx.vContracts.back().SharePayload().As<PollPayload>();

    LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: tx contract payload claim with %" PRIszu " address(es), poll title %s.",
             __func__,
             poll_payload.m_claim.m_balance_claim.m_address_claims.size(),
             poll_payload.m_poll.m_title
             );

    if (!claim_builder.SignClaim(poll_payload, tx)) {
        throw VotingError(_("Poll signature failed. See debug.log."));
    }

    // Ensure that the client code passed all of the necessary data to the poll
    // builder:
    //
    if (!poll_payload.WellFormed()) {
        throw VotingError("Poll incomplete. This is probably a bug.");
    }

    // Validate poll
    int DoS = 0; // unused here
    if (!GetPollRegistry().Validate(tx.vContracts.back(), tx, DoS)) {
        throw VotingError("Poll invalid.");
    }

    return tx;
}

// -----------------------------------------------------------------------------
// Class: VoteBuilder
// -----------------------------------------------------------------------------

VoteBuilder::VoteBuilder(const Poll& poll, const uint256 poll_txid)
    : m_poll(&poll)
    , m_vote(std::make_unique<Vote>())
{
    m_vote->m_poll_txid = poll_txid;
}

VoteBuilder::VoteBuilder(VoteBuilder&& builder) = default;
VoteBuilder::~VoteBuilder() = default;

VoteBuilder& VoteBuilder::operator=(VoteBuilder&& builder)
{
    m_poll = builder.m_poll;
    m_vote = std::move(builder.m_vote);
    m_claim_outpoints = std::move(builder.m_claim_outpoints);
    m_fee_outpoint = std::move(builder.m_fee_outpoint);

    return *this;
}

VoteBuilder VoteBuilder::SetClaimOutpoints(std::vector<COutPoint> outpoints)
{
    m_claim_outpoints = std::move(outpoints);

    return std::move(*this);
}

VoteBuilder VoteBuilder::SetFeeOutpoint(const COutPoint& outpoint)
{
    m_fee_outpoint = outpoint;

    return std::move(*this);
}

VoteBuilder VoteBuilder::ForPoll(const Poll& poll, const uint256 poll_txid)
{
    // Judged by the wall clock on purpose, not by blocks: a vote built now is mined after now, so
    // once the clock passes the end it would land in the first block past the end or later, where it does not count.
    if (poll.Expired(GetAdjustedTime())) {
        throw VotingError(_("Poll has already finished."));
    }

    return VoteBuilder(poll, poll_txid);
}

VoteBuilder VoteBuilder::SetResponses(const std::vector<uint8_t>& offsets)
{
    m_vote->m_responses.clear();

    return AddResponses(offsets);
}

VoteBuilder VoteBuilder::SetResponses(const std::vector<std::string>& labels)
{
    m_vote->m_responses.clear();

    return AddResponses(labels);
}

VoteBuilder VoteBuilder::AddResponses(const std::vector<uint8_t>& offsets)
{
    for (const auto& offset : offsets) {
        *this = AddResponse(offset);
    }

    return std::move(*this);
}

VoteBuilder VoteBuilder::AddResponses(const std::vector<std::string>& labels)
{
    for (const auto& label : labels) {
        *this = AddResponse(label);
    }

    return std::move(*this);
}

VoteBuilder VoteBuilder::AddResponse(const uint8_t offset)
{
    if (!m_vote->m_responses.empty() && !m_poll->AllowsMultipleChoices()) {
        throw VotingError(_("Poll only allows a single choice."));
    }

    if (!m_poll->Choices().OffsetInRange(offset)) {
        throw VotingError(strprintf(
            _("\"%s\" is not a valid poll choice."),
            ToString(static_cast<int>(offset))));
    }

    if (m_vote->ResponseExists(offset)) {
        throw VotingError(strprintf(
            _("Duplicate response for poll choice: %s"),
            ToString(static_cast<int>(offset))));
    }

    // This is effectively handled by the previous conditions. We'll leave
    // it here for reference:
    //
    if (m_vote->m_responses.size() + 1 > m_poll->Choices().size()) {
        throw VotingError(strprintf(
            _("Exceeded the number of choices in the poll: %s"),
            ToString(m_poll->Choices().size())));
    }

    m_vote->m_responses.emplace_back(offset);

    return std::move(*this);
}

VoteBuilder VoteBuilder::AddResponse(const std::string& label)
{
    if (std::optional<uint8_t> offset = m_poll->Choices().OffsetOf(label)) {
        return AddResponse(*offset);
    }

    throw VotingError(strprintf(_("\"%s\" is not a valid poll choice."), label));
}

CWalletTx VoteBuilder::BuildContractTx(CWallet* const pwallet, const uint32_t& contract_version) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    if (!pwallet) {
        throw VotingError(_("No wallet available."));
    }

    // Building the contract signs claims and the transaction with the wallet's
    // keys, so the wallet must be fully unlocked, or unlocked for staking only
    // with a capability scope open on this thread. One read of the scope
    // answers which refusal applies.
    switch (pwallet->GetUnlockScope()) {
    case UnlockScope::Locked:
        throw VotingError(_("Error: Wallet locked, unable to create transaction."));
    case UnlockScope::StakingOnly:
        if (!pwallet->CanUseKeys()) {
            throw VotingError(_("Error: Wallet unlocked for staking only, unable to create transaction."));
        }
        break;
    case UnlockScope::Full:
        break;
    }

    if (m_vote->m_responses.empty()) {
        throw VotingError(_("Please enter at least one response."));
    }

    CWalletTx tx;

    const VoteClaimBuilder claim_builder(*pwallet, Researcher::Get());
    std::optional<COutPoint> pending_vote_output;
    if (m_claim_outpoints) {
        claim_builder.BuildClaimFromOutpoints(*m_vote, *m_claim_outpoints);
    } else {
        claim_builder.BuildClaim(*m_vote, *m_poll, PollStartsAtOrAfterV15(m_vote->m_poll_txid), pending_vote_output);
    }

    {
        CMutableTransaction mtx(tx);
        mtx.vContracts.emplace_back(
            MakeContract<Vote>(contract_version, ContractAction::ADD, std::move(*m_vote)));
        static_cast<CTransaction&>(tx) = CTransaction(std::move(mtx));
    }

    SelectFinalInputs<Vote>(*pwallet, tx, m_fee_outpoint, pending_vote_output);
    Vote& vote = tx.vContracts.back().SharePayload().As<Vote>();

    if (!claim_builder.SignClaim(vote, tx)) {
        throw VotingError(_("Vote signature failed. See debug.log."));
    }

    // Ensure that the client code passed all of the necessary data to the vote
    // builder:
    //
    if (!vote.WellFormed()) {
        throw VotingError("Vote incomplete. This is probably a bug.");
    }

    return tx;
}
