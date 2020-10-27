// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "init.h"
#include "main.h"
#include "gridcoin/beacon.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/contract/message.h"
#include "gridcoin/researcher.h"
#include "gridcoin/voting/builders.h"
#include "gridcoin/voting/claims.h"
#include "gridcoin/voting/payloads.h"
#include "ui_interface.h"
#include "wallet/wallet.h"

#include <boost/algorithm/string/trim.hpp>

using namespace GRC;
using LogFlags = BCLog::LogFlags;

namespace {
//!
//! \brief Get the string representation of an output destination for logging.
//!
//! \param dest Destination to convert to an address string.
//!
//! \return Base58-encoded string of the address.
//!
std::string DestinationToAddressString(const CTxDestination dest)
{
    CBitcoinAddress address;
    address.Set(dest);

    return address.ToString();
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
}; // AddressOutputs

using OutputAddresses = std::vector<AddressOutputs>;

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
    //! \return Unspent outputs grouped by address in desecending order by
    //! the total amount of the outputs in each address grouping.
    //!
    //! \throws VotingError If the wallet contains no unspent outputs eligible
    //! for a claim.
    //!
    OutputAddresses PickCoins() const
    {
        std::vector<COutput> outputs;
        m_wallet.AvailableCoins(outputs, true, nullptr, true);

        const auto descending_by_amount = [](const COutput& a, const COutput& b) {
            return a.tx->vout[a.i].nValue > b.tx->vout[b.i].nValue;
        };

        std::sort(outputs.begin(), outputs.end(), descending_by_amount);

        LogPrint(LogFlags::VOTE,
            "%s: wallet supplied %" PRIszu " tx output candidates",
            __func__,
            outputs.size());

        OutputAddressMap by_address = FilterOutputs(outputs);

        LogPrint(LogFlags::VOTE,
            "%s: filtered tx output set contains %" PRIszu " addresses",
            __func__,
            by_address.size());

        if (by_address.empty()) {
            throw VotingError(_("No eligible outputs greater than 1 GRC."));
        }

        return SortAddressesByAmount(std::move(by_address));
    }

private:
    const CWallet& m_wallet; //!< The wallet to fetch outputs from.

    using OutputAddressMap = std::map<CKeyID, AddressOutputs>;

    //!
    //! \brief Adds outputs for an address only when an address represents
    //! a pay-to-public-key or pay-to-public-key-hash destination.
    //!
    class PubkeyDestinationFilter : public boost::static_visitor<void>
    {
    public:
        //!
        //! \brief Initialize a filter.
        //!
        //! \param by_address A set of outputs grouped by address to add the
        //! filtered outputs to.
        //!
        PubkeyDestinationFilter(OutputAddressMap& by_address)
            : m_by_address(by_address)
        {
        }

        void operator()(const CKeyID& keyId, const COutput& output) const
        {
            LogPrint(LogFlags::VOTE, "  added output for address: %s",
                DestinationToAddressString(keyId));

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
                DestinationToAddressString(scriptId));
        }

    private:
        OutputAddressMap& m_by_address; //!< Outputs grouped by address.
    }; // PubkeyDestinationFilter

    //!
    //! \brief Get a set of filtered outputs spent for public key addresses.
    //!
    //! \param outputs The set of outputs to filter. Sorted in descending order
    //! by amount.
    //!
    //! \return Filtered outputs grouped by address.
    //!
    static OutputAddressMap FilterOutputs(const std::vector<COutput>& outputs)
    {
        OutputAddressMap by_address;
        PubkeyDestinationFilter pubkey_filter(by_address);

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
                    DestinationToAddressString(dest));

                break; // The input set is sorted so the rest are too small.
            }

            if (!txo.tx->IsTrusted() || !txo.tx->IsConfirmed()) {
                LogPrint(LogFlags::VOTE, "  skipped unconfirmed output for %s",
                    DestinationToAddressString(dest));
                continue;
            }

            // Check that the output is P2PK or P2PKH. Voting does not support
            // more advanced redemption scripts like multisig yet:
            //
            auto filter = std::bind(pubkey_filter, std::placeholders::_1, txo);
            boost::apply_visitor(filter, dest);
        }

        return by_address;
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
    boost::optional<AddressClaim> TryBuildClaim(AddressOutputs address_outputs) const
    {
        AddressClaim claim(std::move(address_outputs.m_outpoints));

        if (!m_wallet.GetPubKey(address_outputs.m_key_id, claim.m_public_key)) {
            error("%s: failed to fetch public key for address %s",
                __func__,
                DestinationToAddressString(address_outputs.m_key_id));

            return boost::none;
        }

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
                DestinationToAddressString(key_id));
        }

        if (!private_key.IsValid()) {
            return error("%s: invalid private key for address %s",
                __func__,
                DestinationToAddressString(key_id));
        }

        if (!claim.Sign(private_key, message)) {
            return error("%s: failed to sign address claim for address %s",
                __func__,
                DestinationToAddressString(key_id));
        }

        LogPrint(LogFlags::VOTE,
            "%s: signed address claim for address: %s",
            __func__,
            DestinationToAddressString(key_id));

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
    //! \param claim The object to fill with the claim.
    //!
    void BuildClaim(BalanceClaim& claim) const
    {
        const AddressClaimBuilder builder(m_wallet);
        OutputAddresses outputs_by_address = CoinPicker(m_wallet).PickCoins();

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
                DestinationToAddressString(address.m_key_id));

            address.m_outpoints.pop_back();
            bytes_estimate -= txo_bytes;

            if (address.m_outpoints.empty()) {
                LogPrint(LogFlags::VOTE, "%s: trimmed address claim %s",
                    __func__,
                    DestinationToAddressString(address.m_key_id));

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
    void BuildClaim(MagnitudeClaim& claim) const
    {
        if (m_researcher->Magnitude().Scaled() == 0) {
            LogPrint(LogFlags::VOTE, "%s: skipped zero magnitude", __func__);
            return;
        }

        const boost::optional<Beacon> beacon = m_researcher->TryBeacon();

        // Avoid building a claim for a beacon that will expire soon:
        if (!beacon || beacon->Expired(GetAdjustedTime() + 15 * 60)) {
            LogPrint(LogFlags::VOTE, "%s: skipped no active beacon", __func__);
            return;
        }

        const boost::optional<uint256> beacon_txid = FindBeaconTxid(*beacon);

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

        const boost::optional<Beacon> beacon = m_researcher->TryBeacon();

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
            claim.m_mining_id = MiningId::ForInvestor();
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
    boost::optional<uint256> FindBeaconTxid(const Beacon& beacon) const
    {
        // TODO: This is rather slow, but we only need to do it once per vote.
        // Store a reference to a wallet's beacon transactions and rewrite the
        // lookup to avoid the chain scan:

        const CBlockIndex* pindex = pindexBest;

        if (!pindex) {
            return boost::none;
        }

        const int64_t max_time = FutureDrift(beacon.m_timestamp, 0);
        const int64_t min_time = pindex->nTime - Beacon::MAX_AGE;
        CBlock block;

        for (; pindex && pindex->nTime > max_time; pindex = pindex->pprev);

        for (; pindex && pindex->nTime > min_time; pindex = pindex->pprev) {
            if (!pindex->IsContract()) {
                continue;
            }

            if (!block.ReadFromDisk(pindex)) {
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

        return boost::none;
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
    PollEligibilityClaim BuildClaim(const Poll& poll) const
    {
        const AddressClaimBuilder builder(m_wallet);
        PollEligibilityClaim claim;

        for (auto& address : CoinPicker(m_wallet).PickCoins()) {
            // Addresses are sorted in descending order by amount. We can exit
            // early when we find an address less than the required amount:
            if (address.m_total_amount < POLL_REQUIRED_BALANCE) {
                break;
            }

            if (!TryTrimAddress(address)) {
                continue;
            }

            if (auto claim_option = builder.TryBuildClaim(std::move(address))) {
                claim.m_address_claim = std::move(*claim_option);
                return claim;
            }
        }

        throw VotingError(strprintf(
            _("No address contains %s GRC in %s UTXOs or fewer."),
            FormatMoney(POLL_REQUIRED_BALANCE),
            std::to_string(PollEligibilityClaim::MAX_OUTPOINTS)));
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

        return builder.SignClaim(payload.m_claim.m_address_claim, message);
    }

private:
    const CWallet& m_wallet; //!< Supplies balance context and signing keys.

    //!
    //! \brief Remove outputs from the address until the address fits into a
    //! poll address claim or until the total claimed amount for the address
    //! falls below the required threshold.
    //!
    //! \param address The intermediate address container to trim.
    //!
    //! \return \c false if the trimmed amount for the address does not meet
    //! the balance requirement for a poll.
    //!
    static bool TryTrimAddress(AddressOutputs& address)
    {
        std::vector<COutPoint>& outpoints = address.m_outpoints;
        std::vector<CAmount>& amounts = address.m_amounts;

        while (outpoints.size() > PollEligibilityClaim::MAX_OUTPOINTS) {
            address.m_total_amount -= amounts.back();

            if (address.m_total_amount < POLL_REQUIRED_BALANCE) {
                LogPrint(LogFlags::VOTE, "%s: exceeded max outputs for %s",
                    __func__,
                    DestinationToAddressString(address.m_key_id));

                return false;
            }

            outpoints.pop_back();
            amounts.pop_back();
        }

        // Trim any remaining outputs that we don't need to satisfy the balance
        // requirement for the poll:
        //
        while (address.m_total_amount - amounts.back() > POLL_REQUIRED_BALANCE) {
            outpoints.pop_back();
            amounts.pop_back();

            address.m_total_amount -= amounts.back();
        }

        return true;
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
    //! \param vote Vote contract to generate the claim for.
    //! \param poll Determines how to generate a magnitude claim.
    //!
    void BuildClaim(Vote& vote, const Poll& poll) const
    {
        VoteWeightClaim& claim = vote.m_claim;

        m_balance_builder.BuildClaim(claim.m_balance_claim);

        if (poll.IncludesMagnitudeWeight()) {
            m_magnitude_builder.BuildClaim(claim.m_magnitude_claim);
        }
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
template <typename PayloadType>
void SelectFinalInputs(CWallet& wallet, CWalletTx& tx)
{
    CWalletTx mock_tx = tx;
    Contract& contract = mock_tx.vContracts.back();

    // Expand the incomplete claim signatures to provide a more realistic
    // transaction size that the wallet will base the input selection on:
    //
    contract.SharePayload().As<PayloadType>().m_claim.ExpandDummySignatures();

    const CAmount burn_fee = contract.RequiredBurnAmount();
    CReserveKey reserve_key(&wallet); // unused
    CAmount out_applied_fee;

    if (!wallet.CreateTransaction(
        CScript() << OP_RETURN,
        burn_fee,
        mock_tx,
        reserve_key,
        out_applied_fee,
        nullptr))
    {
        if (burn_fee + out_applied_fee > wallet.GetBalance()) {
            throw VotingError(_("Insufficient funds."));
        }

        throw VotingError(_("Could not create transaction. See debug.log."));
    }

    tx.vin = std::move(mock_tx.vin);
}
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Global Functions
// -----------------------------------------------------------------------------

void GRC::SendPollContract(PollBuilder builder)
{
    std::pair<CWalletTx, std::string> result_pair;

    {
        LOCK2(cs_main, pwalletMain->cs_wallet);
        result_pair = SendContract(builder.BuildContractTx(pwalletMain));
    }

    if (!result_pair.second.empty()) {
        throw VotingError(result_pair.second);
    }
}

void GRC::SendVoteContract(VoteBuilder builder)
{
    std::pair<CWalletTx, std::string> result_pair;

    {
        LOCK2(cs_main, pwalletMain->cs_wallet);
        result_pair = SendContract(builder.BuildContractTx(pwalletMain));
    }

    if (!result_pair.second.empty()) {
        throw VotingError(result_pair.second);
    }
}

// -----------------------------------------------------------------------------
// Class: PollBuilder
// -----------------------------------------------------------------------------

PollBuilder::PollBuilder() : m_poll(MakeUnique<Poll>())
{
}

PollBuilder::PollBuilder(PollBuilder&& builder) = default;
PollBuilder::~PollBuilder() = default;
PollBuilder& PollBuilder::operator=(PollBuilder&& builder) = default;

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
    if (days < Poll::MIN_DURATION_DAYS) {
        throw VotingError(strprintf(
            _("Poll duration must be at least %s days."),
            std::to_string(Poll::MIN_DURATION_DAYS)));
    }

    // The protocol allows poll durations up to 180 days. To limit unhelpful
    // or unintentional poll durations, user-facing pieces discourage a poll
    // longer than:
    //
    constexpr uint32_t max_duration_days = 90;

    if (days > max_duration_days) {
        throw VotingError(strprintf(
            _("Poll duration cannot exceed %s days."),
            std::to_string(max_duration_days)));
    }

    m_poll->m_duration_days = days;

    return std::move(*this);
}

PollBuilder PollBuilder::SetTitle(std::string title)
{
    boost::trim(title);

    if (title.empty()) {
        throw VotingError(_("Please enter a poll title."));
    }

    if (title.size() > Poll::MAX_TITLE_SIZE) {
        throw VotingError(strprintf(
            _("Poll title cannot exceed %s characters."),
            std::to_string(Poll::MAX_TITLE_SIZE)));
    }

    m_poll->m_title = std::move(title);

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
            std::to_string(Poll::MAX_URL_SIZE)));
    }

    m_poll->m_url = std::move(url);

    return std::move(*this);
}

PollBuilder PollBuilder::SetQuestion(std::string question)
{
    if (question.size() > Poll::MAX_QUESTION_SIZE) {
        throw VotingError(strprintf(
            _("Poll question cannot exceed %s characters."),
            std::to_string(Poll::MAX_QUESTION_SIZE)));
    }

    m_poll->m_question = std::move(question);

    return std::move(*this);
}

PollBuilder PollBuilder::SetChoices(std::vector<std::string> labels)
{
    m_poll->m_choices = Poll::ChoiceList();

    return AddChoices(std::move(labels));
}

PollBuilder PollBuilder::AddChoices(std::vector<std::string> labels)
{
    for (auto& label : labels) {
        *this = AddChoice(std::move(label));
    }

    return std::move(*this);
}

PollBuilder PollBuilder::AddChoice(std::string label)
{
    boost::trim(label);

    if (label.empty()) {
        throw VotingError(_("A poll choice cannot be empty."));
    }

    if (m_poll->m_choices.size() + 1 > POLL_MAX_CHOICES_SIZE) {
        throw VotingError(strprintf(
            _("Poll cannot contain more than %s choices."),
            std::to_string(POLL_MAX_CHOICES_SIZE)));
    }

    if (label.size() > Poll::Choice::MAX_LABEL_SIZE) {
        throw VotingError(strprintf(
            _("Poll choice \"%s\" exceeds %s characters."),
            label,
            std::to_string(Poll::Choice::MAX_LABEL_SIZE)));
    }

    if (m_poll->m_choices.LabelExists(label)) {
        throw VotingError(strprintf(_("Duplicate poll choice: %s"), label));
    }

    m_poll->m_choices.Add(std::move(label));

    return std::move(*this);
}

CWalletTx PollBuilder::BuildContractTx(CWallet* const pwallet)
{
    if (!pwallet) {
        throw VotingError(_("No wallet available."));
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

    CWalletTx tx;

    // Not necessary but allows the RPC to report expiration:
    m_poll->m_timestamp = tx.nTime;

    const PollClaimBuilder claim_builder(*pwallet);
    PollEligibilityClaim claim = claim_builder.BuildClaim(*m_poll);

    tx.vContracts.emplace_back(MakeContract<PollPayload>(
        ContractAction::ADD,
        std::move(*m_poll),
        std::move(claim)));

    SelectFinalInputs<PollPayload>(*pwallet, tx);
    PollPayload& poll_payload = tx.vContracts.back().SharePayload().As<PollPayload>();

    if (!claim_builder.SignClaim(poll_payload, tx)) {
        throw VotingError(_("Poll signature failed. See debug.log."));
    }

    // Ensure that the client code passed all of the necessary data to the poll
    // builder:
    //
    if (!poll_payload.WellFormed()) {
        throw VotingError("Poll incomplete. This is probably a bug.");
    }

    return tx;
}

// -----------------------------------------------------------------------------
// Class: VoteBuilder
// -----------------------------------------------------------------------------

VoteBuilder::VoteBuilder(const Poll& poll, const uint256 poll_txid)
    : m_poll(&poll)
    , m_vote(MakeUnique<Vote>())
{
    m_vote->m_poll_txid = poll_txid;
}

VoteBuilder::VoteBuilder(VoteBuilder&& builder) = default;
VoteBuilder::~VoteBuilder() = default;

VoteBuilder& VoteBuilder::operator=(VoteBuilder&& builder)
{
    m_poll = builder.m_poll;
    m_vote = std::move(builder.m_vote);

    return *this;
}

VoteBuilder VoteBuilder::ForPoll(const Poll& poll, const uint256 poll_txid)
{
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
            std::to_string(offset)));
    }

    if (m_vote->ResponseExists(offset)) {
        throw VotingError(strprintf(
            _("Duplicate response for poll choice: %s"),
            std::to_string(offset)));
    }

    // This is effectively handled by the previous conditions. We'll leave
    // it here for reference:
    //
    if (m_vote->m_responses.size() + 1 > m_poll->Choices().size()) {
        throw VotingError(strprintf(
            _("Exceeded the number of choices in the poll: %s"),
            std::to_string(m_poll->Choices().size())));
    }

    m_vote->m_responses.emplace_back(offset);

    return std::move(*this);
}

VoteBuilder VoteBuilder::AddResponse(const std::string& label)
{
    if (boost::optional<uint8_t> offset = m_poll->Choices().OffsetOf(label)) {
        return AddResponse(*offset);
    }

    throw VotingError(strprintf(_("\"%s\" is not a valid poll choice."), label));
}

CWalletTx VoteBuilder::BuildContractTx(CWallet* const pwallet)
{
    if (!pwallet) {
        throw VotingError(_("No wallet available."));
    }

    if (m_vote->m_responses.empty()) {
        throw VotingError(_("Please enter at least one response."));
    }

    CWalletTx tx;

    const VoteClaimBuilder claim_builder(*pwallet, Researcher::Get());
    claim_builder.BuildClaim(*m_vote, *m_poll);

    tx.vContracts.emplace_back(
        MakeContract<Vote>(ContractAction::ADD, std::move(*m_vote)));

    SelectFinalInputs<Vote>(*pwallet, tx);
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
