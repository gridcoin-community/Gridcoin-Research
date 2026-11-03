// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "gridcoin/beacon.h"
#include "gridcoin/contract/message.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/cpid.h"
#include "gridcoin/sidestake.h"
#include "policy/fees.h"
#include "script.h"
#include "script/standard.h"
#include "net_processing.h"
#include "wallet/wallet.h"

#include "wallet/coincontrol.h"
#include "txmempool.h"

using namespace GRC;

extern CWallet* pwalletMain;

namespace {
//! The largest signature push a scriptSig carries: a low-S DER signature and its
//! sighash byte are at most 72 bytes, and the push opcode adds one. CKey signs
//! low-S only.
constexpr unsigned int MAX_SIGNATURE_PUSH_SIZE = 73;

//! The serialized size of pushing \p n bytes onto a script.
unsigned int PushSize(const size_t n)
{
    return n + (n < OP_PUSHDATA1 ? 1 : n <= 0xff ? 2 : 3);
}

//!
//! \brief The largest scriptSig the wallet's signer writes to spend an output.
//!
//! \param script_pub_key The output's script.
//! \param wallet         Holds the public keys and redeem scripts the scriptSig
//!                       carries.
//! \param nested         Whether \p script_pub_key is a pay-to-script-hash
//!                       output's redeem script, which cannot itself be one.
//!
//! \return The size, or nullopt for an output the signer does not write a
//! standard scriptSig for.
//!
std::optional<unsigned int> MaxScriptSigSize(const CScript& script_pub_key, const CWallet& wallet, const bool nested)
{
    txnouttype type;
    std::vector<std::vector<unsigned char>> solutions;

    if (!Solver(script_pub_key, type, solutions)) {
        return std::nullopt;
    }

    switch (type) {
    case TX_PUBKEY:
        return MAX_SIGNATURE_PUSH_SIZE;
    case TX_PUBKEYHASH:
    {
        CPubKey public_key;

        if (!wallet.GetPubKey(CKeyID(uint160(solutions[0])), public_key)) {
            return std::nullopt;
        }

        return MAX_SIGNATURE_PUSH_SIZE + PushSize(public_key.size());
    }
    case TX_MULTISIG:
        // OP_0, then a signature for each key the script requires.
        return 1 + solutions.front()[0] * MAX_SIGNATURE_PUSH_SIZE;
    case TX_SCRIPTHASH:
    {
        CScript redeem_script;

        if (nested || !wallet.GetCScript(CScriptID(uint160(solutions[0])), redeem_script)) {
            return std::nullopt;
        }

        const std::optional<unsigned int> redeem_size = MaxScriptSigSize(redeem_script, wallet, /*nested=*/true);

        if (!redeem_size) {
            return std::nullopt;
        }

        // The redeem script's own scriptSig, then the redeem script pushed.
        return *redeem_size + PushSize(redeem_script.size());
    }
    case TX_NONSTANDARD:
    case TX_NULL_DATA:
        return std::nullopt;
    } // no default case, so the compiler can warn about missing cases

    return std::nullopt;
}

//!
//! \brief Configure coin control for a transaction to select inputs from the
//! set of UTXOs associated with the master key address and to send the change
//! back to the same address.
//!
//! \param coin_control Stores the selected input/output configuration.
//!
//! \return \c true if the wallet owns UTXOs for the master key address.
//!
bool SelectMasterInputOutput(CCoinControl& coin_control) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    // The master key rotates by height, and the transaction being built here is
    // validated against the block it lands in -- the next one at the earliest.
    // Selecting for the tip would, in the block before a rotation, spend the
    // OUTGOING key's UTXOs and send change to its address, and HasMasterKeyInput
    // would then reject the transaction against the incoming key.
    const CTxDestination master_address = CWallet::MasterAddress(pindexBest->nHeight + 1);

    // Send change back to the master address:
    coin_control.destChange = master_address;

    std::vector<COutput> unspent_coins;
    pwalletMain->AvailableCoins(unspent_coins, true, &coin_control, false);

    for (const auto& coin : unspent_coins) {
        CTxDestination dest;

        if (!ExtractDestination(coin.tx->vout[coin.i].scriptPubKey, dest)) {
            continue;
        }

        // Select all UTXOs for the master address as inputs to consolidate the
        // change back into one output:
        if (dest == master_address) {
            COutPoint out_point(coin.tx->GetHash(), coin.i);
            coin_control.Select(out_point);
        }
    }

    return coin_control.HasSelected();
}

//!
//! \brief Configure a contract transaction and set up inputs and outputs.
//!
//! \param wtx_new        A new transaction with a contract.
//! \param reserve_key    Key reserved for any change.
//! \param burn_fee       Total burn fee required for contracts in the transaction.
//! \param renewal_access The automated beacon renewal's access, or nullptr.
//!
//! \return \c true if coin selection succeeded.
//!
bool CreateContractTx(CWalletTx& wtx_out, CReserveKey& reserve_key, CAmount burn_fee,
                      const StakingKeyAccess* renewal_access) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    CCoinControl coin_control_out;
    CAmount applied_fee_out = 0; // Unused, but the builder can return before seeding it
    bool admin = false;
    bool contract_change_to_input_address = gArgs.GetBoolArg("-contractchangetoinputaddress", false);
    CTxDestination out_address {CNoDestination()};

    // If the input transaction already selected some inputs, ensure that we
    // pick those inputs again when creating the final transaction:
    for (auto& txin : wtx_out.vin) {
        coin_control_out.Select(txin.prevout);

        // If contract_change_to_input_address is false or once the first already selected input is encountered
        // that has a valid address, select that for change and then skip over further address selection for change.
        if (!contract_change_to_input_address
                || !std::get_if<CNoDestination>(&out_address)
                || !ExtractDestination(txin.scriptSig, out_address)) {
            continue;
        }

        coin_control_out.destChange = out_address;

        LogPrintf("INFO: %s: Change sent to %s for contract transaction per contractchangetoinputaddress setting.",
                  __func__, EncodeDestination(coin_control_out.destChange));
    }

    for (const auto& contract : wtx_out.vContracts) {
        admin |= contract.RequiresMasterKey();
    }

    // Configure inputs/outputs for the address associated with the master key.
    // Nodes validate administrative contracts by checking that the containing
    // transactions include an input signed by the master key, so select coins
    // from the master address and send any change back to it:
    if (admin && !SelectMasterInputOutput(coin_control_out)) {
        return false;
    }

    // Burn the output. For reference, the old burn addresses for contracts are:
    //
    //   mainnet: S67nL4vELWwdDVzjgtEP4MxryarTZ9a8GB
    //   testnet: mk1e432zWKH1MW57ragKywuXaWAtHy1AHZ
    //
    CScript scriptPubKey;
    scriptPubKey << OP_RETURN;

    return pwalletMain->CreateTransaction(
        { std::make_pair(std::move(scriptPubKey), burn_fee) },
        wtx_out,
        reserve_key,
        applied_fee_out,
        &coin_control_out, contract_change_to_input_address,
        renewal_access);
}

//!
//! \brief Send a transaction that contains a contract.
//!
//! \param wtx_new A new transaction with a contract.
//!
//! \return An empty string when successful or a description of the error that
//! occurred. TODO: Refactor to remove string-based signaling.
//!
std::string SendContractTx(CWalletTx& wtx_new, const StakingKeyAccess* renewal_access) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    // Declared before the lock below, so the announcement flushes after this
    // function's cs_wallet is released (#3391), as in CWallet::SendMoney.
    DeferredRelay relay;

    // Held from the scope check through CommitTransaction, as in
    // CWallet::SendMoney: a narrowing to staking-only waits for this send to
    // finish instead of landing between building and committing it (#3389).
    // cs_main is already held (above); cs_wallet is recursive, so callers that
    // hold it already are unaffected. A caller that holds cs_wallet itself
    // still announces under it when the relay flushes, which is safe: see the
    // lock rule on CConnman::ForEachNode (#3443).
    LOCK(pwalletMain->cs_wallet);

    CReserveKey reserve_key(pwalletMain);

    if (pwalletMain->IsLocked()) {
        std::string strError = _("Error: Wallet locked, unable to create transaction.");
        LogPrintf("%s: %s", __func__, strError);
        return strError;
    }

    // The builder refuses too, but only as "Transaction creation failed"; this
    // reports why, as CWallet::SendMoney does. Under the lock above, so a
    // narrowing that lands while this send waits for the wallet is seen here.
    if (pwalletMain->IsUnlockedForStakingOnly() && !renewal_access) {
        std::string strError = _("Error: Wallet unlocked for staking only, unable to create transaction.");
        LogPrintf("%s: %s", __func__, strError);
        return strError;
    }

    CAmount balance = pwalletMain->GetBalance();
    CAmount burn_fee = 0;

    for (const auto& contract : wtx_new.vContracts) {
        burn_fee += contract.RequiredBurnAmount();
    }

    // The renewal pays the minimum fee, never nTransactionFee, which settxfee
    // sets without an unlock.
    const CAmount configured_fee = renewal_access ? 0 : nTransactionFee;

    if (balance < COIN || balance < burn_fee + configured_fee) {
        std::string strError = _("Balance too low to create a contract.");
        error("%s: %s", __func__, strError);
        return strError;
    }

    if (!CreateContractTx(wtx_new, reserve_key, burn_fee, renewal_access)) {
        std::string strError = _("Error: Transaction creation failed.");
        error("%s: %s", __func__, strError);
        return strError;
    }

    // An O(1) index check (replaced a full mempool scan) enforces at most one
    // mandatory sidestake transaction in the pool at a time.
    if (mempool.HasMandatorySidestake()) {
        std::string strError = _(
            "Error: The mandatory sidestake transaction was rejected. "
            "There is already a mandatory sidestake transaction in the mempool. "
            "Wait until that transaction is bound in a block.");
        error("%s: %s", __func__, strError);
        return strError;
    }

    if (!pwalletMain->CommitTransaction(wtx_new, reserve_key, &relay)) {
        std::string strError = _(
            "Error: The transaction was rejected. This might happen if some of "
            "the coins in your wallet were already spent, such as if you used "
            "a copy of wallet.dat and coins were spent in the copy but not "
            "marked as spent here.");

        error("%s: %s", __func__, strError);
        return strError;
    }

    for (const auto& contract : wtx_new.GetContracts()) {
        LogPrintf(
            "%s: %s %s in %s",
            __func__,
            contract.m_action.ToString(),
            contract.m_type.ToString(),
            wtx_new.GetHash().ToString());
    }

    return "";
}
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Functions
// -----------------------------------------------------------------------------

std::pair<CWalletTx, std::string> GRC::SendContract(Contract contract, const StakingKeyAccess* renewal_access)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    CMutableTransaction mtx;
    mtx.vContracts.emplace_back(std::move(contract));

    CWalletTx wtx;
    static_cast<CTransaction&>(wtx) = CTransaction(std::move(mtx));

    return SendContract(std::move(wtx), renewal_access);
}

std::pair<CWalletTx, std::string> GRC::SendContract(CWalletTx wtx, const StakingKeyAccess* renewal_access)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    if (wtx.vContracts.empty()) {
        return std::make_pair(std::move(wtx), "Transaction contains no contract.");
    }

    std::string error = SendContractTx(wtx, renewal_access);

    return std::make_pair(std::move(wtx), std::move(error));
}

std::optional<CAmount> GRC::RenewalMinimumFee(const CMutableTransaction& tx, const CWallet& wallet)
    EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet)
{
    AssertLockHeld(wallet.cs_wallet);

    if (tx.vin.empty()) {
        return std::nullopt;
    }

    // A copy whose scriptSigs are filler of the largest length each can have,
    // so one serialization gives the signed size.
    CMutableTransaction sized(tx);

    for (CTxIn& txin : sized.vin) {
        const auto prev = wallet.mapWallet.find(txin.prevout.hash);

        if (prev == wallet.mapWallet.end() || txin.prevout.n >= prev->second.vout.size()) {
            return std::nullopt;
        }

        const std::optional<unsigned int> script_sig_size = MaxScriptSigSize(
            prev->second.vout[txin.prevout.n].scriptPubKey, wallet, /*nested=*/false);

        if (!script_sig_size) {
            return std::nullopt;
        }

        // Raw filler bytes, not a push, so the scriptSig serializes to exactly
        // that many bytes of script.
        txin.scriptSig = CScript();
        txin.scriptSig.insert(txin.scriptSig.end(), *script_sig_size, 0x00);
    }

    const CTransaction sized_tx(sized);

    return GetMinFee(sized_tx, 1000, GMF_SEND, ::GetSerializeSize(sized_tx, SER_NETWORK, PROTOCOL_VERSION));
}

bool GRC::IsRenewalShape(
    const CMutableTransaction& tx,
    const Cpid& cpid,
    const CAmount burn_fee,
    const CAmount fee,
    const CWallet& wallet) EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet)
{
    AssertLockHeld(wallet.cs_wallet);

    // One contract, a beacon advertisement for the CPID.
    if (tx.vContracts.size() != 1) {
        return false;
    }

    const Contract& contract = tx.vContracts.front();

    if (contract.m_type != ContractType::BEACON
        || contract.m_action != ContractAction::ADD
        || contract.m_version < 2
        || contract.SharePayloadAs<BeaconPayload>()->m_cpid != cpid)
    {
        return false;
    }

    // The burn, and at most one change output that the wallet can spend. The
    // builder puts the change at a random position.
    const CScript burn_script = CScript() << OP_RETURN;
    bool burn_seen = false;
    bool change_seen = false;
    CAmount value_out = 0;

    for (const CTxOut& txout : tx.vout) {
        if (!MoneyRange(txout.nValue)) {
            return false;
        }

        value_out += txout.nValue;

        if (!burn_seen && txout.scriptPubKey == burn_script && txout.nValue == burn_fee) {
            burn_seen = true;
            continue;
        }

        if (!change_seen && txout.nValue > 0 && wallet.IsMine(txout) == ISMINE_SPENDABLE) {
            change_seen = true;
            continue;
        }

        return false;
    }

    if (!burn_seen || !MoneyRange(value_out)) {
        return false;
    }

    // Only the wallet's own coins in, and exactly the fee left over, which is the
    // minimum for the transaction's size.
    CAmount value_in = 0;

    for (const CTxIn& txin : tx.vin) {
        const auto prev = wallet.mapWallet.find(txin.prevout.hash);

        if (prev == wallet.mapWallet.end() || txin.prevout.n >= prev->second.vout.size()) {
            return false;
        }

        value_in += prev->second.vout[txin.prevout.n].nValue;

        if (!MoneyRange(value_in)) {
            return false;
        }
    }

    const std::optional<CAmount> minimum_fee = RenewalMinimumFee(tx, wallet);

    return minimum_fee && fee == *minimum_fee && value_in - value_out == fee;
}
