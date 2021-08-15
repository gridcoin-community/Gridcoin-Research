// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "init.h"
#include "main.h"
#include "gridcoin/beacon.h"
#include "gridcoin/claim.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/project.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/superblock.h"
#include "gridcoin/support/block_finder.h"
#include "gridcoin/tx_message.h"
#include "gridcoin/voting/payloads.h"
#include "node/blockstorage.h"
#include "policy/policy.h"
#include "policy/fees.h"
#include "primitives/transaction.h"
#include "protocol.h"
#include "server.h"
#include "streams.h"
#include "txdb.h"
#include <util/string.h>
#include "validation.h"
#include "wallet/coincontrol.h"
#include "wallet/wallet.h"
#include <script.h>

#include <queue>

using namespace GRC;
using namespace std;

std::vector<std::pair<std::string, std::string>> GetTxStakeBoincHashInfo(const CMerkleTx& mtx)
{
    assert(mtx.IsCoinStake() || mtx.IsCoinBase());
    std::vector<std::pair<std::string, std::string>> res;

    // Fetch BlockIndex for tx block
    CBlockIndex* pindex = nullptr;
    CBlock block;
    {
        BlockMap::iterator mi = mapBlockIndex.find(mtx.hashBlock);
        if (mi == mapBlockIndex.end())
        {
            res.push_back(std::make_pair(_("ERROR"), _("Block not in index")));
            return res;
        }

        pindex = mi->second;

        if (!ReadBlockFromDisk(block, pindex, Params().GetConsensus()))
        {
            res.push_back(std::make_pair(_("ERROR"), _("Block read failed")));
            return res;
        }
    }

    const GRC::Claim& claim = block.GetClaim();
    const GRC::MintSummary mint = block.GetMint();

    res.push_back(std::make_pair(_("Height"), ToString(pindex->nHeight)));
    res.push_back(std::make_pair(_("Block Version"), ToString(block.nVersion)));
    res.push_back(std::make_pair(_("Difficulty"), RoundToString(GRC::GetBlockDifficulty(block.nBits),8)));
    res.push_back(std::make_pair(_("CPID"), claim.m_mining_id.ToString()));
    res.push_back(std::make_pair(_("Interest"), FormatMoney(claim.m_block_subsidy)));

    if (pindex->Magnitude() > 0)
    {
        res.push_back(std::make_pair(_("Boinc Reward"), FormatMoney(claim.m_research_subsidy)));
        res.push_back(std::make_pair(_("Magnitude"), RoundToString(pindex->Magnitude(), 8)));
    }

    res.push_back(std::make_pair(_("Fees Collected"), FormatMoney(mint.m_fees)));
    res.push_back(std::make_pair(_("Is Superblock"), (claim.ContainsSuperblock() ? "Yes" : "No")));

    if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE))
    {
        if (claim.ContainsSuperblock())
            res.push_back(std::make_pair(_("Superblock Binary Size"), ToString(GetSerializeSize(claim.m_superblock, 1, 1))));

        res.push_back(std::make_pair(_("Quorum Hash"), claim.m_quorum_hash.ToString()));
        res.push_back(std::make_pair(_("Client Version"), claim.m_client_version));
        res.push_back(std::make_pair(_("Organization"), claim.m_organization));
    }

    return res;
}

void ScriptPubKeyToJSON(const CScript& scriptPubKey, UniValue& out, bool fIncludeHex)
{
    txnouttype type;
    vector<CTxDestination> addresses;
    int nRequired;

    out.pushKV("asm", scriptPubKey.ToString());

    if (fIncludeHex)
        out.pushKV("hex", HexStr(scriptPubKey.begin(), scriptPubKey.end()));

    if (!ExtractDestinations(scriptPubKey, type, addresses, nRequired))
    {
        out.pushKV("type", GetTxnOutputType(type));
        return;
    }

    out.pushKV("reqSigs", nRequired);
    out.pushKV("type", GetTxnOutputType(type));

    UniValue a(UniValue::VARR);
    for (auto const& addr : addresses)
        a.push_back(CBitcoinAddress(addr).ToString());
    out.pushKV("addresses", a);
}

namespace {
UniValue LegacyContractPayloadToJson(const GRC::ContractPayload& payload)
{
    UniValue out(UniValue::VOBJ);

    out.pushKV("key", payload->LegacyKeyString());
    out.pushKV("value", payload->LegacyValueString());

    return out;
}

UniValue BeaconToJson(const GRC::ContractPayload& payload)
{
    const auto& beacon = payload.As<GRC::BeaconPayload>();

    UniValue out(UniValue::VOBJ);

    out.pushKV("version", (int)beacon.m_version);
    out.pushKV("cpid", beacon.m_cpid.ToString());
    out.pushKV("public_key", beacon.m_beacon.m_public_key.ToString());

    return out;
}

UniValue RawClaimToJson(const GRC::ContractPayload& payload)
{
    const auto& claim = payload.As<GRC::Claim>();

    UniValue json(UniValue::VOBJ);

    json.pushKV("version", (int)claim.m_version);
    json.pushKV("mining_id", claim.m_mining_id.ToString());
    json.pushKV("client_version", claim.m_client_version);
    json.pushKV("organization", claim.m_organization);
    json.pushKV("block_subsidy", ValueFromAmount(claim.m_block_subsidy));
    json.pushKV("research_subsidy", ValueFromAmount(claim.m_research_subsidy));
    json.pushKV("magnitude", claim.m_magnitude);
    json.pushKV("magnitude_unit", claim.m_magnitude_unit);
    json.pushKV("signature", EncodeBase64(claim.m_signature.data(), claim.m_signature.size()));
    json.pushKV("quorum_hash", claim.m_quorum_hash.ToString());
    json.pushKV("quorum_address", claim.m_quorum_address);

    return json;
}

UniValue MessagePayloadToJson(const GRC::ContractPayload& payload)
{
    const auto& tx_message = payload.As<GRC::TxMessage>();

    return tx_message.m_message;
}

UniValue PollPayloadToJson(const GRC::ContractPayload& payload)
{
    const auto& poll = payload.As<GRC::PollPayload>();

    // Note: we don't include the claim data here to avoid dumping potentially
    // large output for clients that don't need it. Use the getvotingclaim RPC
    // to fetch the claim.
    //
    UniValue out(UniValue::VOBJ);

    out.pushKV("version", (int)poll.m_version);
    out.pushKV("title", poll.m_poll.m_title);
    out.pushKV("question", poll.m_poll.m_question);
    out.pushKV("url", poll.m_poll.m_url);
    out.pushKV("type", (int)poll.m_poll.m_type.Raw());
    out.pushKV("weight_type", (int)poll.m_poll.m_weight_type.Raw());
    out.pushKV("response_type", (int)poll.m_poll.m_response_type.Raw());
    out.pushKV("duration_days", (int)poll.m_poll.m_duration_days);

    UniValue choices(UniValue::VARR);

    for (size_t i = 0; i < poll.m_poll.Choices().size(); ++i) {
        UniValue choice(UniValue::VOBJ);
        choice.pushKV("id", (int)i);
        choice.pushKV("label", poll.m_poll.Choices().At(i)->m_label);

        choices.push_back(choice);
    }

    out.pushKV("choices", choices);

    return out;
}

UniValue ProjectToJson(const GRC::ContractPayload& payload)
{
    const auto& project = payload.As<GRC::Project>();

    UniValue out(UniValue::VOBJ);

    out.pushKV("version", (int)project.m_version);
    out.pushKV("name", project.m_name);
    out.pushKV("url", project.m_url);

    return out;
}

UniValue VotePayloadToJson(const GRC::ContractPayload& payload)
{
    const auto& vote = payload.As<GRC::Vote>();

    UniValue responses(UniValue::VARR);

    for (const auto& offset : vote.m_responses) {
        responses.push_back((int)offset);
    }

    // Note: we don't include the claim data here to avoid dumping potentially
    // large output for clients that don't need it. Use the getvotingclaim RPC
    // to fetch the claim.
    //
    UniValue out(UniValue::VOBJ);

    out.pushKV("version", (int)vote.m_version);
    out.pushKV("poll_txid", vote.m_poll_txid.ToString());
    out.pushKV("responses", responses);

    return out;
}

UniValue LegacyVotePayloadToJson(const GRC::ContractPayload& payload)
{
    const auto& vote = payload.As<GRC::LegacyVote>();

    UniValue out(UniValue::VOBJ);

    out.pushKV("key", vote.m_key);
    out.pushKV("mining_id", vote.m_mining_id.ToString());
    out.pushKV("amount", vote.m_amount);
    out.pushKV("magnitude", vote.m_magnitude);
    out.pushKV("responses", vote.m_responses);

    return out;
}
} // Anonymous namespace

UniValue ContractToJson(const GRC::Contract& contract)
{
    UniValue out(UniValue::VOBJ);

    out.pushKV("version", (int)contract.m_version);
    out.pushKV("type", contract.m_type.ToString());
    out.pushKV("action", contract.m_action.ToString());

    switch (contract.m_type.Value()) {
        case GRC::ContractType::BEACON:
            out.pushKV("body", BeaconToJson(contract.SharePayload()));
            break;
        case GRC::ContractType::CLAIM:
            out.pushKV("body", RawClaimToJson(contract.SharePayload()));
            break;
        case GRC::ContractType::MESSAGE:
            out.pushKV("body", MessagePayloadToJson(contract.SharePayload()));
            break;
        case GRC::ContractType::POLL:
            out.pushKV("body", PollPayloadToJson(contract.SharePayload()));
            break;
        case GRC::ContractType::PROJECT:
            out.pushKV("body", ProjectToJson(contract.SharePayload()));
            break;
        case GRC::ContractType::VOTE:
            if (contract.m_version >= 2) {
                out.pushKV("body", VotePayloadToJson(contract.SharePayload()));
            } else {
                out.pushKV("body", LegacyVotePayloadToJson(contract.SharePayload()));
            }
            break;
        default:
            out.pushKV("body", LegacyContractPayloadToJson(contract.SharePayload()));
            break;
    }

    return out;
}

void TxToJSON(const CTransaction& tx, const uint256 hashBlock, UniValue& entry)
{
    entry.pushKV("txid", tx.GetHash().GetHex());
    entry.pushKV("version", tx.nVersion);
    entry.pushKV("size", (int)::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION));
    entry.pushKV("time", (int)tx.nTime);
    entry.pushKV("locktime", (int)tx.nLockTime);
    entry.pushKV("hashboinc", tx.hashBoinc);

    UniValue contracts(UniValue::VARR);

    for (const auto& contract : tx.GetContracts()) {
        contracts.push_back(ContractToJson(contract));
    }

    entry.pushKV("contracts", contracts);

    UniValue vin(UniValue::VARR);
    for (auto const& txin : tx.vin)
    {
        UniValue in(UniValue::VOBJ);

        if (tx.IsCoinBase())
            in.pushKV("coinbase", HexStr(txin.scriptSig.begin(), txin.scriptSig.end()));

        else
        {
            in.pushKV("txid", txin.prevout.hash.GetHex());

            in.pushKV("vout", (int)txin.prevout.n);
            UniValue o(UniValue::VOBJ);
            o.pushKV("asm", txin.scriptSig.ToString());
            o.pushKV("hex", HexStr(txin.scriptSig.begin(), txin.scriptSig.end()));
            in.pushKV("scriptSig", o);
        }
        in.pushKV("sequence", (int)txin.nSequence);
        vin.push_back(in);
    }
    entry.pushKV("vin", vin);
    UniValue vout(UniValue::VARR);
    for (unsigned int i = 0; i < tx.vout.size(); i++)
    {
        const CTxOut& txout = tx.vout[i];
        UniValue out(UniValue::VOBJ);
        out.pushKV("value", ValueFromAmount(txout.nValue));
        out.pushKV("n", (int)i);
        UniValue o(UniValue::VOBJ);
        ScriptPubKeyToJSON(txout.scriptPubKey, o, true);
        out.pushKV("scriptPubKey", o);
        vout.push_back(out);
    }
    entry.pushKV("vout", vout);

    if (!hashBlock.IsNull())
    {
        entry.pushKV("blockhash", hashBlock.GetHex());
        BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
        if (mi != mapBlockIndex.end() && mi->second) {
            CBlockIndex* pindex = mi->second;
            if (pindex->IsInMainChain())
            {
                entry.pushKV("confirmations", 1 + nBestHeight - pindex->nHeight);
                entry.pushKV("time", (int)pindex->nTime);
                entry.pushKV("blocktime", (int)pindex->nTime);
            }
            else
                entry.pushKV("confirmations", 0);
        }
    }
}

UniValue getrawtransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
                "getrawtransaction <txid> [verbose=bool]\n"
                "\n"
                "If verbose is false, returns a string that is\n"
                "serialized, hex-encoded data for <txid>.\n"
                "If verbose is true, returns an Object\n"
                "with information about <txid>\n");

    uint256 hash;
    hash.SetHex(params[0].get_str());

    // Accept either a bool (true) or a num (>=1) to indicate verbose output. Adapted from Bitcoin 20180820.
    bool fVerbose = false;
    if (!params[1].isNull())
    {
        fVerbose = params[1].isNum() ? (params[1].get_int() != 0) : params[1].get_bool();
    }

    LOCK(cs_main);

    CTransaction tx;
    uint256 hashBlock;
    if (!GetTransaction(hash, tx, hashBlock))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available about transaction");

    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << tx;
    string strHex = HexStr(ssTx.begin(), ssTx.end());

    if (!fVerbose)
        return strHex;

    UniValue result(UniValue::VOBJ);
    result.pushKV("hex", strHex);
    TxToJSON(tx, hashBlock, result);
    return result;
}

UniValue listunspent(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error(
                "listunspent [minconf=1] [maxconf=9999999]  [\"address\",...]\n"
                "\n"
                "Returns array of unspent transaction outputs\n"
                "with between minconf and maxconf (inclusive) confirmations.\n"
                "Optionally filtered to only include txouts paid to specified addresses.\n"
                "Results are an array of Objects, each of which has:\n"
                "{txid, vout, scriptPubKey, amount, confirmations}\n");

    RPCTypeCheck(params, { UniValue::VNUM, UniValue::VNUM, UniValue::VARR });

    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();

    int nMaxDepth = 9999999;
    if (params.size() > 1)
        nMaxDepth = params[1].get_int();

    set<CBitcoinAddress> setAddress;
    if (params.size() > 2)
    {
        UniValue inputs = params[2].get_array();
        for (unsigned int ix = 0; ix < inputs.size(); ix++)
        {
            CBitcoinAddress address(inputs[ix].get_str());
            if (!address.IsValid())
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("Invalid Gridcoin address: ")+inputs[ix].get_str());
            if (setAddress.count(address))
                throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, duplicated address: ")+inputs[ix].get_str());
           setAddress.insert(address);
        }
    }

    UniValue results(UniValue::VARR);

    vector<COutput> vecOutputs;

    pwalletMain->AvailableCoins(vecOutputs, false, nullptr, false);

    LOCK(pwalletMain->cs_wallet);

    for (auto const& out : vecOutputs)
    {
        if (out.nDepth < nMinDepth || out.nDepth > nMaxDepth)
            continue;

        if(setAddress.size())
        {
            CTxDestination address;
            if(!ExtractDestination(out.tx->vout[out.i].scriptPubKey, address))
                continue;

            if (!setAddress.count(address))
                continue;
        }

        int64_t nValue = out.tx->vout[out.i].nValue;
        const CScript& pk = out.tx->vout[out.i].scriptPubKey;
        UniValue entry(UniValue::VOBJ);
        entry.pushKV("txid", out.tx->GetHash().GetHex());
        entry.pushKV("vout", out.i);
        CTxDestination address;
        if (ExtractDestination(out.tx->vout[out.i].scriptPubKey, address))
        {
            entry.pushKV("address", CBitcoinAddress(address).ToString());

            auto item = pwalletMain->mapAddressBook.find(address);

            if (item != pwalletMain->mapAddressBook.end())
            {
                entry.pushKV("label", item->second);

                if (gArgs.GetBoolArg("-enableaccounts", false))
                    entry.pushKV("account", item->second);
            }
        }
        entry.pushKV("scriptPubKey", HexStr(pk.begin(), pk.end()));
        entry.pushKV("amount", ValueFromAmount(nValue));
        entry.pushKV("confirmations", out.nDepth);
        results.push_back(entry);
    }

    return results;
}


UniValue consolidateunspent(const UniValue& params, bool fHelp)
{
    std::stringstream error_strm;

    error_strm << "consolidateunspent <address> [UTXO size] [maximum number of inputs] [sweep all addresses] [sweep change]\n"
                  "\n"
                  "<address>:                  The Gridcoin address target for consolidation.\n"
                  "\n"
                  "[UTXO size]:                Optional parameter for target consolidation output size.\n"
                  "\n"
                  "[maximum number of inputs]: Defaults and clamped to "
               << ToString(GetMaxInputsForConsolidationTxn())
               << " maximum to prevent transaction failures.\n"
                  "\n"
                  "[sweep all addresses]:      Boolean to indicate whether all addresses should be used for inputs to the\n"
                  "                            consolidation. If true, the source of the consolidation is all addresses and\n"
                  "                            the output will be to the specified address, otherwise inputs will only be\n"
                  "                            sourced from the same address.\n"
                  "\n"
                  "[sweep change]:             Boolean to indicate whether change associated with the address should be\n"
                  "                            consolidated. If [sweep all addresses] is true then this is also forced true.\n"
                  "\n"
                  "consolidateunspent performs a single transaction to consolidate UTXOs to/on a given address. The optional\n"
                  "parameter of UTXO size will result in consolidating UTXOs to generate the largest output possible less\n"
                  "than that size or the total value of the specified maximum number of smallest inputs, whichever is less.\n"
                  "\n"
                  "The script is designed to be run repeatedly and will become a no-op if the UTXO's are consolidated such\n"
                  "that no more meet the specified criteria. This is ideal for automated periodic scripting.\n"
                  "\n"
                  "To consolidate the entire wallet to one address do something like:\n"
                  "\n"
                  "consolidateunspent <address> <amount equal or larger than balance> 200 true repeatedly until there are\n"
                  "no more UTXOs to consolidate.\n"
                  "\n"
                  "In all cases the address MUST exist in your wallet beforehand. If doing this for the purpose of creating\n"
                  "a new smaller wallet, create a new address beforehand to serve as the target of the consolidation.\n";

    if (fHelp || params.size() < 1 || params.size() > 5)
        throw runtime_error(error_strm.str());

    UniValue result(UniValue::VOBJ);

    std::string sAddress = params[0].get_str();
    CBitcoinAddress OptimizeAddress(sAddress);

    int64_t nConsolidateLimit = 0;
    unsigned int nInputNumberLimit = GetMaxInputsForConsolidationTxn();

    bool sweep_all_addresses = false;

    // Note this value is ignored if sweep_all_addresses is set to true.
    bool sweep_change = false;

    if (params.size() > 1) nConsolidateLimit = AmountFromValue(params[1]);

    if (params.size() > 2) nInputNumberLimit = params[2].get_int();

    if (params.size() > 3) sweep_all_addresses = params[3].get_bool();

    if (params.size() > 4 && !sweep_all_addresses) sweep_change = params[4].get_bool();

    // Clamp InputNumberLimit to GetMaxInputsForConsolidationTxn(). Above that number of inputs risks an invalid transaction
    // due to the size.
    nInputNumberLimit = std::min<unsigned int>(nInputNumberLimit, GetMaxInputsForConsolidationTxn());

    if (!OptimizeAddress.IsValid())
    {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("Invalid Gridcoin address: ") + sAddress);
    }

    // Set the consolidation transaction address to the specified output address.
    CScript scriptDestPubKey;
    scriptDestPubKey.SetDestination(OptimizeAddress.Get());

    std::vector<COutput> vecInputs;

    // A convenient way to do a sort without the bother of writing a comparison operator.
    // The map does it for us! It must be a multimap, because it is highly likely one or
    // more UTXO's will have the same nValue.
    std::multimap<int64_t, COutput> mInputs;

    // Have to lock both main and wallet.
    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Get the current UTXO's.
    pwalletMain->AvailableCoins(vecInputs, false, nullptr, false);

    // Filter outputs in the wallet that are the candidate inputs by matching address and insert into sorted multimap.
    for (auto const& out : vecInputs)
    {
        CTxDestination out_address;

        int64_t nOutValue = out.tx->vout[out.i].nValue;

        // If we can't resolve the address, then move on.
        if (!ExtractDestination(out.tx->vout[out.i].scriptPubKey, out_address)) continue;

        // If the UTXO matches the consolidation address or all sweep_all_addresses is true then add to the inputs
        // map for consolidation. Note that the value of sweep_change is ignored and all change will be swept.
        if (CBitcoinAddress(out_address) == OptimizeAddress || sweep_all_addresses)
        {
            mInputs.insert(std::make_pair(nOutValue, out));
        }
        // If sweep_change is true... and ...
        // If the UTXO is change (we already know it "ISMINE") then iterate through the inputs to the transaction
        // that created the change. This loop has the effect of matching the change to the address of the first
        // input's address of the transaction that created the change. It is possible that the change could have been
        // created from a transaction whose inputs can from multiple addresses within one's wallet. In this case,
        // a choice has to be made. This is similar to listaddressgroupings and is a decent approach...
        else if (sweep_change && pwalletMain->IsChange(out.tx->vout[out.i]))
        {
            // Iterate through the inputs of the transaction that created the change. Note that this has to be implemented
            // as a work queue, because change can stake, and so the input here could still be change, etc. You have to
            // continue walking up the tree of transactions until you get to a non-change source address. If the non-change
            // source address matches the consolidation address, the UTXO is included.

            // The work queue.
            std::queue<std::vector<CTxIn>> vin_queue;

            // The initial population of the queue is the input vector of the transaction that created the change UTXO.
            vin_queue.push(out.tx->vin);


            // Keep track of the vin vectors processed on the queue and limit to a reasonable value of 25 to prevent
            // excessively long execution times. This introduces the possibility of failing to resolve a change parent
            // address that has been through many stakes, but I am concerned about the processing time.
            unsigned int vins_emplaced = 0;

            while (!vin_queue.empty() && vins_emplaced <= 25)
            {
                // Pull the first unit of work.
                std::vector<CTxIn> v_change_input = vin_queue.front();
                vin_queue.pop();

                for (const auto& change_input : v_change_input)
                {
                    // Get the COutPoint of this transaction input.
                    COutPoint prevout = change_input.prevout;

                    CTxDestination change_input_address;

                    // Get the transaction that generated this output, which was the input to
                    // the transaction that created the change.
                    CWalletTx tx_change_input = pwalletMain->mapWallet[prevout.hash];

                    // Get the corresponding output of that transaction (this is the same as the input to the
                    // referenced transaction). We need this to resolve the address.
                    CTxOut prev_ctxout = tx_change_input.vout[prevout.n];

                    // If this is still change then place the input vector of this transaction onto the queue.
                    if (pwalletMain->IsChange(prev_ctxout))
                    {
                        vin_queue.emplace(tx_change_input.vin);
                        ++vins_emplaced;
                    }

                    // If not change, then get the address of that output and if it matches the OptimizeAddress add the UTXO
                    // to the inputs map for consolidation.
                    if (ExtractDestination(prev_ctxout.scriptPubKey, change_input_address))
                    {
                        if (CBitcoinAddress(change_input_address) == OptimizeAddress)
                        {
                            // Insert the ORIGINAL change UTXO into the input map for the consolidation.
                            mInputs.insert(std::make_pair(nOutValue, out));

                            // We do not need/want to add it more than once, and we do not need to continue processing the
                            // queue if a linkage is found.
                            break;
                        }
                    }
                } // for (const auto& change_input : v_change_input)
            } // while (!vin_queue.empty())
        } // else if (pwalletMain->IsChange(out.tx->vout[out.i]))
    } // for (auto const& out : vecInputs)

    CWalletTx wtxNew;

    set<pair<const CWalletTx*,unsigned int>> setCoins;

    unsigned int iInputCount = 0;
    CAmount nAmount = 0;
    unsigned int nBytesInputs = 0;

    // Construct the inputs to the consolidation transaction. Either all of the inputs from above, or 200,
    // or when the total reaches/exceeds nConsolidateLimit, whichever is more limiting. The map allows us
    // to elegantly select the UTXO's from the smallest upwards.
    for (auto const& out : mInputs)
    {
        int64_t nUTXOValue = out.second.tx->vout[out.second.i].nValue;

        if (iInputCount == nInputNumberLimit || ((nAmount + nUTXOValue) > nConsolidateLimit && nConsolidateLimit != 0)) break;

        // This has been moved after the break to change the behavior so that the
        // consolidation is limited to the set of UTXO's SMALLER than the nConsolidateLimit
        // in the case that the next UTXO would go OVER the limit. The prior behavior
        // would include that next UTXO and produce a UTXO larger than desired.
        // Both methods have undesirable corner cases:
        // Old behavior corner case... UTXO 1 to n gets to nConsolidateLimit - epsilon,
        // and the next one is the rest of the value on the address, which causes
        // undesired complete consolidation of the address.
        // New behavior corner case... UTXO 1 to n are all very small and total value
        // adds up to a small fraction of nConsolidateLimit. Next UTXO is similar to
        // nConsolidateLimit, but is not included, which means the output UTXO is limited
        // to the consolidation of the smaller UTXO's and produces a UTXO that is much
        // smaller than desired. To consolidate the next UTXO would require changing the
        // input parameters to the function.
        // Feedback from users indicate the latter is preferable to the former. The only way
        // to solve both is to include a "change" UTXO to true up the mismatch. This is
        // overly complex and not worth the implementation time.

        nAmount += nUTXOValue;

        LogPrint(BCLog::LogFlags::VERBOSE, "INFO: consolidateunspent: input value = %f, confirmations = %" PRId64,
                 ((double) out.first) / (double) COIN, out.second.nDepth);

        setCoins.insert(make_pair(out.second.tx, out.second.i));

        ++iInputCount;

        // For fee calculation. This is similar to the calculation in coincontroldialog.cpp.
        CTxDestination address;

        if (ExtractDestination(out.second.tx->vout[out.second.i].scriptPubKey, address))
        {
            CPubKey pubkey;
            try {
                if (pwalletMain->GetPubKey(std::get<CKeyID>(address), pubkey))
                {
                    nBytesInputs += (pubkey.IsCompressed() ? 148 : 180);
                }
                // in all error cases, simply assume 148 here
                else
                {
                    nBytesInputs += 148;
                }
            } catch (const std::bad_variant_access&) {
                nBytesInputs += 148;
            }
        }
        else nBytesInputs += 148;
    }

    // If number of inputs that meet criteria is less than two, then do nothing.
    if (iInputCount < 2)
    {
        result.pushKV("result", false);
        result.pushKV("UTXOs consolidated", (uint64_t) 0);

        return result;
    }

    CReserveKey reservekey(pwalletMain);


    // Fee calculation to avoid change.

    CTransaction txDummy;

    // Bytes
    // ----- The inputs to the tx - The one output.
    int64_t nBytes = nBytesInputs + 2 * 34 + 10;

    // Min Fee
    int64_t nMinFee = GetMinFee(txDummy, 1000, GMF_SEND, nBytes);

    int64_t nFee = nTransactionFee * (1 + (int64_t) nBytes / 1000);

    int64_t nFeeRequired = std::max(nMinFee, nFee);


    if (pwalletMain->IsLocked())
    {
        string strError = _("Error: Wallet locked, unable to create transaction.");
        LogPrintf("consolidateunspent: %s", strError);
        return strError;
    }

    if (fWalletUnlockStakingOnly)
    {
        string strError = _("Error: Wallet unlocked for staking only, unable to create transaction.");
        LogPrintf("consolidateunspent: %s", strError);
        return strError;
    }

    vector<pair<CScript, int64_t> > vecSend;

    // Reduce the out value for the transaction by nFeeRequired from the total of the inputs to provide a fee
    // to the staker. The fee has been calculated so that no change should be produced from the CreateTransaction
    // call. Just in case, the input address is specified as the return address via coincontrol.
    vecSend.push_back(std::make_pair(scriptDestPubKey, nAmount - nFeeRequired));

    CCoinControl coinControl;

    // Send the change back to the same address.
    coinControl.destChange = OptimizeAddress.Get();

    if (!pwalletMain->CreateTransaction(vecSend, setCoins, wtxNew, reservekey, nFeeRequired, &coinControl))
    {
        string strError;
        if (nAmount + nFeeRequired > pwalletMain->GetBalance())
            strError = strprintf(_("Error: This transaction requires a transaction fee of at least %s because of its amount,"
                                   " complexity, or use of recently received funds "), FormatMoney(nFeeRequired));
        else
            strError = _("Error: Transaction creation failed  ");
        LogPrintf("consolidateunspent: %s", strError);
        return strError;
    }

    if (!pwalletMain->CommitTransaction(wtxNew, reservekey))
        return _("Error: The transaction was rejected.  This might happen if some of the coins in your wallet were already"
                 " spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent"
                 " here.");

    result.pushKV("result", true);
    result.pushKV("utxos_consolidated", (uint64_t) iInputCount);
    result.pushKV("output_utxo_value", FormatMoney(nAmount - nFeeRequired));
    result.pushKV("fee", FormatMoney(nFeeRequired));
    result.pushKV("txid", wtxNew.GetHash().GetHex());

    return result;
}

/* MultiSig Tool for consolidating multisig transactions
 *
 * In order to do the best possible calculation and prediction of end result you need to understand transaction serialization.
 * It was brought to my attention that no one could understand where the numbers actually came from. With multisig transactions
 * there are many similarities to standard transactions. Here I'll break down all aspects of a serialized hex transaction.
 * I plan to break it down into sections for easier understanding since its quite detailed. Some details I'm unaware of though.
 * It is important to note that padding at new areas start with OP_0 aka 0x00 and padding at end of an area ends with OP_INVALIDOPCODE 0xff.
 * Last information to know is that I will use byte size in calculations but explain in hex sizes here. Byte size is typically Hex size / 2.
 *
 * Much of the space in the transaction is used up by signatures and redeemscript. I'll show the layout of both for the curious.
 *
 * Canonical Standard Per Signature:
 *  47  30  44  02  20 (..) 02  20 (..) 01
 * <sl><ch><tl><dp><rl><rd><dp><sl><sd><st>
 *
 * sl: Canonical signature data length (typically 71-73 bytes in length as the ECDSA is 32 bytes and in some cases it can be longer).
 * ch: 0x30 hex signifies Canonical signature.
 * tl: Total length in hex of combined data padding, R & S data.
 * dp: Padding of 0x02 between data points.
 * rl: R length in hex. R is used for Public Key Recovery.
 * rd: R Data; Typically 32 bytes in length.
 * sl: S length in hex. S is signature
 * sd: S Data; Typically 32 bytes in length.
 * st: Signature type.
 *
 * Note: I'm not going into excessive details of the canonical signatures as there are many references to explain more in depth then needed for this function.
 * We will say that the base size of a single signature is 1 + 73
 * After the signatures are 2 OP codes (2 bytes) for a multisignature tx.
 *
 * Base Signatures formula simplified: BSS = (74 * number_signatures_required) + 2
 *
 * Redeemscript:
 *  52  21 (..) 21 (..) 21 (..)... 53  ae
 * <sr><pl><pk><pl><pk><pl><pk>...<ts><cm>
 *
 * sr: Signatures required in OP code
 * pl: Pubkey length
 * pk: Pubkey
 * ts: Total signable pubkeys in OP code
 * cm: OP code OP_CHECKMULTISIG
 *
 * The wallet/user will provide us with the redeemscript so we will know the size of the script in bytes and we add 4 bytes padding in the tx.
 *
 * Standard transaction begin with:
 *   02    000000   ########    01
 * <txver><padding><MISCDATA><#ofvins>
 *
 * The start of a transaction contains the tx version of the transaction followed by 3 bytes of OP_0 which is padding.
 * MISCDATA then is placed (likely the transaction time etc) followed by the amount of inputs in the transaction.
 * We can count on the size to be unchanged since no other data will be placed in that area.
 *
 * The start of the transaction will be 9 bytes.
 *
 * Vin data in a transaction is stored as the following:
 * (....)  02    000000     fc00   (.......)(................)ffffffff
 * <txid><vout#><padding><MISCDATA><sigdata><redeemscriptdata><padding>
 *
 * txid: the txid on the input being used
 * vout#: the vout on the txid
 * padding: 3 bytes of 0x00 at beginning of vin and 4 bytes of 0xff to signify sequence at end of each vin (
 * MISCDATA: Unknown but consistent across the board.
 * sigdata: Signature data from canonical signature as mentioned above.
 * redeemscript: redeemscript of the multisignature address so the wallet/network knows which addresses are involved in multisignature address.
 *
 * From this we can gather that the vin size will be 32 + 1 + 3 + 2 + BSS + RSS + 4.
 * Or simplified to total vin size of all vins: TVIS = (38 + BSS + RSS + 4) * number_inputs
 *
 * Vout data in transaction is simply put since it is returning to the same address.
 *     1     (...10...) 000000   17  a9  14 (......) 87 0000000000
 * <#ofvouts><MISCDATA><padding><ss><op><ds><script><op><padding>
 *
 * MISCDATA: From testing; This can be bigger but only when multiple outputs are present. Here it is 5 bytes.
 * padding: Start is 3 bytes of 0x00 / end is 5 bytes of 0x00
 * ss: Size of the script with op codes. 17 is 23 bytes
 * op: OP code a9 meaning OP_HASH160 / Op code 87 means OP_EQUAL
 * ds: Size of script. 14 is 20 bytes
 * script: Script for the output
 *
 * From this we can gather that the vout size will be TVOUTS = 1 + 5 + 3 + 23 + 5
 * Or simplified TVOUTS = 37 bytes
 *
 * We can calculate the max amount of inputs that can be used in a consolidation transaction.
 * We have to respect that the gui has a limit of 32767 max characters. To be safe set max hex size of 30000 or 15000 bytes.
 *
 * Formula:
 * 15000 = ((38 + BSS + RSS + 4) * X) + 37
 *
 * Example of a 2 of 3 multisig.
 * RSS = 105 bytes
 * BSS = 150 bytes
 *
 * 15000 = ((38 + 150 + 105 + 4) * X) + 37
 * 15000 = (297 * X) + 37
 * 15000 = 297X + 37
 * 15000 - 37 = 297X + 37 - 37
 * 14963 = 297X
 * -----   ----
 * 297     297
 * 50.38047138 = X
 *
 * We will round up since we will be within the 32766 comfortably
 * 51 = X = max inputs
*/
UniValue consolidatemsunspent(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 5)
        throw runtime_error(
                "consolidatemsunspent <address> <block-start> <block-end> [max-grc] [max-inputs]\n"
                "\n"
                "Searches a block range for a multisig address with unspent utxos\n"
                "and consolidates them into a transaction ready for signing to\n"
                "return to the same address in an consolidated amount.\n"
                "consolidatemsunspent will also determine multi-sig type and ensure\n"
                "the transaction does not exceed any size limits due to inputs.\n"
                "\n"
                "<address> ------------> Multi-signature address\n"
                "<block-start> --------> Block number to start search from\n"
                "<block-end> ----------> Block number to end search on\n"
                "[max-grc] ------------> Highest uxto value to include in search results in halfords (0 is default)\n"
                "[max-inputs] ---------> Maximum inputs desired. (If the calculated max inputs is less than defined here; argument is overridden)\n");

    UniValue result(UniValue::VOBJ);

    // Variables & Parameters
    int nBlockCurrent = 0;
    CAmount nMaxValue = 0;
    int nMaxInputs = 0;
    int nBase = 0;
    double dEqResult = 0;
    unsigned int nRedeemScriptSize = 0;
    std::unordered_multimap<int64_t, std::pair<uint256, unsigned int>> umultimapInputs;
    std::vector<int> vOpCodes;

    std::string sAddress = params[0].get_str();
    int nBlockStart = params[1].get_int();
    int nBlockEnd = params[2].get_int();

    if (params.size() > 3)
        nMaxValue = params[3].get_int64();

    if (params.size() > 4)
        nMaxInputs = params[4].get_int();

    // Parameter Sanity Check
    if (nBlockStart < 1 || nBlockStart > nBestHeight || nBlockStart > nBlockEnd)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid block-start");

    if (nBlockEnd < 1 || nBlockEnd > nBestHeight || nBlockEnd <= nBlockStart)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid block-end");

    if (nMaxValue < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Value must not be less than 0");

    CBitcoinAddress Address(sAddress);

    // Check if the address is valid
    if (!Address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gridcoin Address");

    {
        // This new method we should lock wallet as well with cs_main to make sure we don't deadlock
        LOCK2(cs_main, pwalletMain->cs_wallet);

        // Check The OP codes to determine how many signatures are needed.
        // redeemScript is formatted as such <OP_CODE>21<pubkey>21<pubkey>..........<OP_CODE><OP_CHECKMULTISIG>
        // We are doing that here with this code since it's already in the wallet and verified. This saves cycles.
        bool fOPCodeSuccess = false;
        CScript CSDestSubscript;

        // Gather the Redeemscript for calculations
        CScript CSDest;
        txnouttype WhichType;
        vector<valtype> vSolutions;

        CSDest.SetDestination(Address.Get());

        // Solve the solutions for Destination script
        if (!Solver(CSDest, WhichType, vSolutions))
            throw JSONRPCError(RPC_WALLET_ERROR, "Solver failed to get solutions for CScript() of Gridcoin Address");

        // Get the CScript subscript aka redeemScript from wallet which requires CScriptID supplied by vSolutions[0]
        if (!pwalletMain->GetCScript(CScriptID(uint160(vSolutions[0])), CSDestSubscript))
            throw JSONRPCError(RPC_WALLET_ERROR, "Failed to retrieve redeemScript from wallet.");

        // Set the redeemScript size in bytes.
        nRedeemScriptSize = CSDestSubscript.size();

        try
        {
            CScript::const_iterator script = CSDestSubscript.begin();
            CScript::const_iterator scriptend = CSDestSubscript.end();
            opcodetype Whatopcode;
            valtype vValue;

            while (script < scriptend)
            {
                if (!CSDestSubscript.GetOp(script, Whatopcode, vValue))
                    break;

                // We just care about the OP Codes here for signatures required and total signatures for informational purposes
                switch (Whatopcode)
                {
                case OP_1:
                case OP_2:
                case OP_3:
                case OP_4:
                case OP_5:
                case OP_6:
                case OP_7:
                case OP_8:
                case OP_9:
                case OP_10:
                case OP_11:
                case OP_12:
                case OP_13:
                case OP_14:
                case OP_15:
                case OP_16:
                {
                    vOpCodes.push_back(((int)Whatopcode - (int)(OP_1 - 1)));
                }
                    break;

                default:
                    break;
                }
            }

            // Pretty specific expected behaviour and we should fail under any other circumstance.
            if (!vOpCodes.empty() && vOpCodes.size() == 2 && (vOpCodes[0] <= vOpCodes[1]))
                fOPCodeSuccess = true;
        }

        catch (...)
        {
            throw JSONRPCError(RPC_WALLET_ERROR, "Exception: Unable to retrieve OP codes from redeemScript");
        }

        if (!fOPCodeSuccess)
            throw JSONRPCError(RPC_WALLET_ERROR, "Failure: Unable to retrieve OP codes from redeemScript. Redeemscript was not found in wallet.");

        // We need to determine max inputs; Max ser size we will use is 15000 which is 30000 hex size.
        // Not adding the vout of 37 bytes here since it must be subtracted from both side of equation to balance.
        nBase = (38 + ((74 * vOpCodes[0]) + 2) + ((int)nRedeemScriptSize + 4));
        dEqResult = (15000 - 37) / (double)nBase;

        // Round up
        int nEqMaxInputs = std::ceil(dEqResult);

        if (nMaxInputs == 0 || nMaxInputs > nEqMaxInputs)
            nMaxInputs = nEqMaxInputs;

        GRC::BlockFinder blockfinder;

        CBlockIndex* pblkindex = blockfinder.FindByHeight((nBlockStart - 1));

        if (!pblkindex)
            throw JSONRPCError(RPC_PARSE_ERROR, "Block not found");

        bool fComplete = false;

        while (pblkindex->nHeight < nBlockEnd)
        {
            if (fComplete)
                break;

            pblkindex = pblkindex->pnext;
            nBlockCurrent = pblkindex->nHeight;

            CBlock block;

            if (!ReadBlockFromDisk(block, pblkindex, Params().GetConsensus()))
                throw JSONRPCError(RPC_PARSE_ERROR, "Unable to read block from disk!");

            for (unsigned int i = 1; i < block.vtx.size(); i++)
            {
                if (fComplete)
                    break;

                // Load Transaction
                CTransaction tx;
                CTxDB txdb("r");
                CTxIndex txindex;
                uint256 hash;

                hash = block.vtx[i].GetHash();

                // In case a fail here we can just continue though it shouldn't happen
                if (!ReadTxFromDisk(tx, txdb, COutPoint(hash, 0), txindex))
                    continue;

                // Extract the address from the transaction
                for (unsigned int j = 0; j < tx.vout.size(); j++)
                {
                    if (fComplete)
                        break;

                    const CTxOut& txout = tx.vout[j];
                    CTxDestination txaddress;

                    // Pass failures here though we shouldn't have any failures
                    if (!ExtractDestination(txout.scriptPubKey, txaddress))
                        continue;

                    // If we found a match to multisig address do our work
                    if (CBitcoinAddress(txaddress) == Address)
                    {
                        // Check if this output is already spent
                        COutPoint dummy = COutPoint(tx.GetHash(), j);

                        // This is spent so move along
                        if (!txindex.vSpent[dummy.n].IsNull())
                            continue;

                        // Check if the value exceeds the max-grc range we requested
                        if (nMaxValue != 0 && txout.nValue > nMaxValue)
                            continue;

                        // Add to our input list
                        umultimapInputs.insert(std::make_pair(txout.nValue, std::make_pair(tx.GetHash(), j)));

                        // shouldn't ever surpass this but let's just be safe!
                        if (umultimapInputs.size() >= (unsigned int) nMaxInputs)
                            fComplete = true;
                    }
                }
            }
        }
    }

    if (umultimapInputs.empty())
        throw JSONRPCError(RPC_INVALID_REQUEST, "Search resulted in no results");

    // Parse the inputs and make a raw transaction
    CTransaction rawtx;
    CAmount nTotal = 0;

    // Inputs
    for (const auto& inputs : umultimapInputs)
    {
        nTotal += inputs.first;

        CTxIn in(COutPoint(inputs.second.first, inputs.second.second));

        rawtx.vin.push_back(in);
    }

    CAmount nFee = 0;
    CAmount nMinFee = 0;
    CAmount nTxFee = 0;
    CAmount nOutput = 0;
    int64_t nInputs = umultimapInputs.size();
    // Add vout to the nBase amount
    int64_t nBytes = (nBase * nInputs) + 37;
    nMinFee = GetMinFee(rawtx, 1000, GMF_SEND, nBytes);
    nFee = nTransactionFee * (1 + nBytes / 1000);
    nTxFee = std::max(nMinFee, nFee);
    nOutput = nTotal - nTxFee;
    std::string sHash = "";

    // Make the output
    CScript scriptPubKey;

    scriptPubKey.SetDestination(Address.Get());

    CTxOut out(nOutput, scriptPubKey);

    rawtx.vout.push_back(out);

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);

    ss << rawtx;

    sHash = HexStr(ss.begin(), ss.end());
    std::string sMultisigtype = ToString(vOpCodes[0]);
    sMultisigtype.append("_of_");
    sMultisigtype.append(ToString(vOpCodes[1]));

    result.push_back(std::make_pair("multi_sig_type", sMultisigtype));
    result.push_back(std::make_pair("block_start", nBlockStart));
    result.push_back(std::make_pair("block_end", nBlockEnd));
    // Let rpc caller know this was the last block we were in especially if the target amount of inputs was met before end block
    result.push_back(std::make_pair("last_block_checked", nBlockCurrent));
    result.push_back(std::make_pair("number_of_inputs", nInputs));
    result.push_back(std::make_pair("maximum_possible_inputs", nMaxInputs));
    result.push_back(std::make_pair("total_grc_in", ValueFromAmount(nTotal)));
    result.push_back(std::make_pair("fee", nTxFee));
    result.push_back(std::make_pair("output_amount", ValueFromAmount(nOutput)));
    result.push_back(std::make_pair("estimated_signed_hex_size", (nBytes * 2)));
    result.push_back(std::make_pair("estimated_signed_binary_size", nBytes));
    result.push_back(std::make_pair("rawtx", sHash));

    return result;
}

UniValue scanforunspent(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 5 || params.size() == 4)
        throw runtime_error(
                "scanforunspent <address> <block-start> <block-end> [bool:export] [export-type]\n"
                "\n"
                "Searches a block range for a specified address with unspent utxos\n"
                "and displays them in a json response with the option of exporting\n"
                "to file\n"
                "\n"
                "Parameters required:\n"
                "<address> --------> Multi-signature address\n"
                "<block-start> ----> Block number to start search from\n"
                "<block-end> ------> Block number to end search on\n"
                "\n"
                "Optional:\n"
                "[export] ---------> Exports to a file in backup-dir/rpc in format of multisigaddress-datetime.type\n"
                "[type] -----------> Export to a file with file type (xml, txt or json -- Required if export true)");

    // Parameters
    bool fExport = false;

    std::string sAddress = params[0].get_str();
    int nBlockStart = params[1].get_int();
    int nBlockEnd = params[2].get_int();
    int nType = 0;

    if (params.size() > 3)
    {
        fExport = params[3].get_bool();

        if (params[4].get_str() == "xml")
            nType = 0;

        else if (params[4].get_str() == "txt")
            nType = 1;

        else if (params[4].get_str() == "json")
            nType = 2;

        else
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid export type");
    }

    // Parameter Sanity Check
    if (nBlockStart < 1 || nBlockStart > nBestHeight || nBlockStart > nBlockEnd)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid block-start");

    if (nBlockEnd < 1 || nBlockEnd > nBestHeight || nBlockEnd <= nBlockStart)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid block-end");

    CBitcoinAddress Address(sAddress);

    if (!Address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gridcoin Address");

    std::unordered_multimap<int64_t, std::pair<uint256, unsigned int>> uMultisig;

    {
        LOCK(cs_main);

        GRC::BlockFinder blockfinder;

        CBlockIndex* pblkindex = blockfinder.FindByHeight((nBlockStart - 1));

        if (!pblkindex)
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

        while (pblkindex->nHeight < nBlockEnd)
        {
            pblkindex = pblkindex->pnext;

            CBlock block;

            if (!ReadBlockFromDisk(block, pblkindex, Params().GetConsensus()))
                throw JSONRPCError(RPC_PARSE_ERROR, "Unable to read block from disk!");

            for (unsigned int i = 1; i < block.vtx.size(); i++)
            {
                // Load Transaction
                CTransaction tx;
                CTxDB txdb("r");
                CTxIndex txindex;
                uint256 hash;

                hash = block.vtx[i].GetHash();

                // In case a fail here we can just continue though it shouldn't happen
                if (!ReadTxFromDisk(tx, txdb, COutPoint(hash, 0), txindex))
                    continue;

                // Extract the address from the transaction
                for (unsigned int j = 0; j < tx.vout.size(); j++)
                {
                    const CTxOut& txout = tx.vout[j];
                    CTxDestination txaddress;

                    // Pass failures here though we shouldn't have any failures
                    if (!ExtractDestination(txout.scriptPubKey, txaddress))
                        continue;

                    // If we found a match to multisig address do our work
                    if (CBitcoinAddress(txaddress) == Address)
                    {
                        // Check if this output is already spent
                        COutPoint dummy = COutPoint(tx.GetHash(), j);

                        // This is spent so move along
                        if (!txindex.vSpent[dummy.n].IsNull())
                            continue;

                        // Add to multimap
                        uMultisig.insert(std::make_pair(txout.nValue, std::make_pair(tx.GetHash(), j)));
                    }
                }
            }
        }
    }

    UniValue result(UniValue::VARR);
    UniValue res(UniValue::VOBJ);
    UniValue txres(UniValue::VARR);

    res.pushKV("Block Start", nBlockStart);
    res.pushKV("Block End", nBlockEnd);
    // Check the end results
    if (uMultisig.empty())
        res.pushKV("Result", "No utxos found in specified range");

    else
    {
        std::stringstream exportoutput;
        std::string spacing = "  ";

        if (fExport)
        {
            if (nType == 0)
                exportoutput << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<id>\n";

            else if (nType == 1)
                exportoutput << "TXID / VOUT / Value\n";

        }

        int nCount = 0;
        int64_t nValue = 0;

        // Process the map
        for (const auto& data : uMultisig)
        {
            nCount++;

            nValue += data.first;

            UniValue txdata(UniValue::VOBJ);

            txdata.pushKV("txid", data.second.first.ToString());
            txdata.pushKV("vout", (int)data.second.second);
            txdata.pushKV("value", ValueFromAmount(data.first));

            txres.push_back(txdata);
            // Parse into type file here

            if (fExport)
            {
                if (nType == 0)
                {
                    exportoutput << spacing << "<tx id=\"" << nCount << "\">\n";
                    exportoutput << spacing << spacing << "<txid>" << data.second.first.ToString() << "</txid>\n";
                    exportoutput << spacing << spacing << "<vout>" << data.second.second << "</vout>\n";
                    exportoutput << spacing << spacing << "<value>" << std::fixed << setprecision(8) << data.first / (double)COIN << "</value>\n";
                    exportoutput << spacing << "</tx>\n";
                }

                else if (nType == 1)
                    exportoutput << data.second.first.ToString() << " / " << data.second.second << " / " << std::fixed << setprecision(8) << data.first / (double)COIN << "\n";
            }
        }

        res.pushKV("Block Start", nBlockStart);
        res.pushKV("Block End", nBlockEnd);
        res.pushKV("Total UTXO Count", nCount);
        res.pushKV("Total Value", ValueFromAmount(nValue));

        if (fExport)
        {
            // Complete xml file if its xml
            if (nType == 0)
                exportoutput << "</id>\n";

            fsbridge::ofstream dataout;

            // We will place this in wallet backups as a safer location then in main data directory
            fs::path exportpath;

            time_t biTime;
            struct tm * blTime;
            time (&biTime);
            blTime = localtime(&biTime);
            char boTime[200];
            strftime(boTime, sizeof(boTime), "%Y-%m-%dT%H-%M-%S", blTime);

            std::string exportfile = params[0].get_str() + "-" + std::string(boTime) + "." + params[4].get_str();

            std::string backupdir = gArgs.GetArg("-backupdir", "");

            if (backupdir.empty())
                exportpath = GetDataDir() / "walletbackups" / "rpc" / exportfile;

            else
                exportpath = fs::path(backupdir) / exportfile;

            fs::create_directory(exportpath.parent_path());

            dataout.open(exportpath);

            if (!dataout)
            {
                res.pushKV("Export failed", "Failed to open stream for export file");

                fExport = false;
            }

            else
            {
                if (nType == 0 || nType == 1)
                {
                    const std::string& out = exportoutput.str();

                    dataout << out;
                }

                else
                    dataout << txres.write(2);

                dataout.close();
            }

        }
    }

    if (!txres.empty())
        result.push_back(txres);

    result.push_back(res);

    return result;
}

UniValue createrawtransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
                "createrawtransaction [{\"txid\":\"id\",\"vout\":n},...] {\"address\":amount,\"data\":\"hex\",...}\n"
                "\nCreate a transaction spending the given inputs and creating new outputs.\n"
                "Outputs can be addresses or data.\n"
                "Returns hex-encoded raw transaction.\n"
                "Note that the transaction's inputs are not signed, and\n"
                "it is not stored in the wallet or transmitted to the network.\n"
                "\nArguments:\n"
                "1. \"transactions\"        (string, required) A json array of json objects\n"
                "     [\n"
                "       {\n"
                "         \"txid\":\"id\",    (string, required) The transaction id\n"
                "         \"vout\":n        (numeric, required) The output number\n"
                "       }\n"
                "       ,...\n"
                "     ]\n"
                "2. \"outputs\"             (string, required) a json object with outputs\n"
                "    {\n"
                "      \"address\": x.xxx   (numeric, required) The key is the bitcoin address, the value is the CURRENCY_UNIT amount\n"
                "      \"data\": \"hex\",     (string, required) The key is \"data\", the value is hex encoded data\n"
                "      ...\n"
                "    }\n"
                "\nResult:\n"
                "\"transaction\"            (string) hex string of the transaction\n"
                "\nExamples\n"
                "createrawtransaction \"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"{\\\"address\\\":0.01} "
                "createrawtransaction \"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"{\\\"data\\\":\\\"00010203\\\"} "
                "createrawtransaction \"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\", \"{\\\"address\\\":0.01} "
                "createrawtransaction \"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\", \"{\\\"data\\\":\\\"00010203\\\"} \n"
                );

    RPCTypeCheck(params, { UniValue::VARR, UniValue::VOBJ });

    UniValue inputs = params[0].get_array();
    UniValue sendTo = params[1].get_obj();

    CTransaction rawTx;

    for (unsigned int idx = 0; idx<inputs.size(); idx++)
    {
        const UniValue& input = inputs[idx];
        const UniValue& o = input.get_obj();

        const UniValue& txid_v = find_value(o, "txid");
        if (!txid_v.isStr())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing txid key");
        string txid = txid_v.get_str();
        if (!IsHex(txid))
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected hex txid");

        const UniValue& vout_v = find_value(o, "vout");
        if (!vout_v.isNum())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing vout key");
        int nOutput = vout_v.get_int();
        if (nOutput < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout must be positive");

        CTxIn in(COutPoint(uint256S(txid), nOutput));
        rawTx.vin.push_back(in);
    }

    set<CBitcoinAddress> setAddress;
    vector<string> addrList = sendTo.getKeys();
    for (auto const& name_: addrList)
    {
        CBitcoinAddress address(name_);
        if (name_ == "data")
        {
            std::vector<unsigned char> data = ParseHexV(sendTo[name_],"Data");
            CTxOut out(0, CScript() << OP_RETURN << data);
            rawTx.vout.push_back(out);
        }
        else
        {
            CBitcoinAddress address(name_);
            if (!address.IsValid())
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("Invalid Gridcoin address: ")+name_);

            if (setAddress.count(address))
                throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, duplicated address: ")+name_);
            setAddress.insert(address);

            CScript scriptPubKey;
            scriptPubKey.SetDestination(address.Get());
            int64_t nAmount = AmountFromValue(sendTo[name_]);

            CTxOut out(nAmount, scriptPubKey);
            rawTx.vout.push_back(out);
        }
    }

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << rawTx;
    return HexStr(ss.begin(), ss.end());
}

UniValue decoderawtransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "decoderawtransaction <hex string>\n"
                "\n"
                "Return a JSON object representing the serialized, hex-encoded transaction\n");

    RPCTypeCheck(params, { UniValue::VSTR });

    vector<unsigned char> txData(ParseHex(params[0].get_str()));

    LOCK(cs_main);

    CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
    CTransaction tx;
    try {
        ssData >> tx;
    }
    catch (std::exception &e) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }

    UniValue result(UniValue::VOBJ);
    TxToJSON(tx, uint256(), result);

    return result;
}

UniValue decodescript(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "decodescript <hex string>\n"
                "\n"
                "Decode a hex-encoded script.\n");

    RPCTypeCheck(params, { UniValue::VSTR });

    UniValue r(UniValue::VOBJ);
    CScript script;
    if (params[0].get_str().size() > 0){
        vector<unsigned char> scriptData(ParseHexV(params[0], "argument"));
        script = CScript(scriptData.begin(), scriptData.end());
    } else {
        // Empty scripts are valid
    }
    ScriptPubKeyToJSON(script, r, false);

    r.pushKV("p2sh", CBitcoinAddress(script.GetID()).ToString());
    return r;
}

UniValue signrawtransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 4)
        throw runtime_error(
                "signrawtransaction <hex string> [{\"txid\":txid,\"vout\":n,\"scriptPubKey\":hex},...] [<privatekey1>,...] [sighashtype=\"ALL\"]\n"
                "\n"
                "Sign inputs for raw transaction (serialized, hex-encoded).\n"
                "Second optional argument (may be null) is an array of previous transaction outputs that\n"
                "this transaction depends on but may not yet be in the blockchain.\n"
                "Third optional argument (may be null) is an array of base58-encoded private\n"
                "keys that, if given, will be the only keys used to sign the transaction.\n"
                "Fourth optional argument is a string that is one of six values; ALL, NONE, SINGLE or\n"
                "ALL|ANYONECANPAY, NONE|ANYONECANPAY, SINGLE|ANYONECANPAY.\n"
                "Returns json object with keys:\n"
                "  hex : raw transaction with signature(s) (hex-encoded string)\n"
                "  complete : 1 if transaction has a complete set of signature (0 if not)\n"
                + HelpRequiringPassphrase());

    RPCTypeCheck(params, { UniValue::VSTR, UniValue::VARR, UniValue::VARR, UniValue::VSTR }, true);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    vector<unsigned char> txData(ParseHex(params[0].get_str()));
    CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
    vector<CTransaction> txVariants;
    while (!ssData.empty())
    {
        try {
            CTransaction tx;
            ssData >> tx;
            txVariants.push_back(tx);
        }
        catch (std::exception &e) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
        }
    }

    if (txVariants.empty())
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Missing transaction");

    // mergedTx will end up with all the signatures; it
    // starts as a clone of the rawtx:
    CTransaction mergedTx(txVariants[0]);
    bool fComplete = true;

    // Fetch previous transactions (inputs):
    map<COutPoint, CScript> mapPrevOut;
    for (unsigned int i = 0; i < mergedTx.vin.size(); i++)
    {
        CTransaction tempTx;
        MapPrevTx mapPrevTx;
        CTxDB txdb("r");
        map<uint256, CTxIndex> unused;
        bool fInvalid;

        // FetchInputs aborts on failure, so we go one at a time.
        tempTx.vin.push_back(mergedTx.vin[i]);
        FetchInputs(tempTx, txdb, unused, false, false, mapPrevTx, fInvalid);

        // Copy results into mapPrevOut:
        for (auto const& txin : tempTx.vin)
        {
            const uint256& prevHash = txin.prevout.hash;
            if (mapPrevTx.count(prevHash) && mapPrevTx[prevHash].second.vout.size()>txin.prevout.n)
                mapPrevOut[txin.prevout] = mapPrevTx[prevHash].second.vout[txin.prevout.n].scriptPubKey;
        }
    }

    // Add previous txouts given in the RPC call:
    if (params.size() > 1 && !params[1].isNull())
    {
        UniValue prevTxs = params[1].get_array();
        for (unsigned int idx = 0; idx < prevTxs.size(); idx++)
        {
            const UniValue&p = prevTxs[idx];
            if (!p.isObject())
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "expected object with {\"txid'\",\"vout\",\"scriptPubKey\"}");

            UniValue prevOut = p.get_obj();

            RPCTypeCheckObj(prevOut, {
                { "txid", UniValue::VSTR },
                { "vout", UniValue::VNUM },
                { "scriptPubKey", UniValue::VSTR },
            });

            string txidHex = find_value(prevOut, "txid").get_str();
            if (!IsHex(txidHex))
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "txid must be hexadecimal");
            uint256 txid;
            txid.SetHex(txidHex);

            int nOut = find_value(prevOut, "vout").get_int();
            if (nOut < 0)
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "vout must be positive");

            string pkHex = find_value(prevOut, "scriptPubKey").get_str();
            if (!IsHex(pkHex))
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "scriptPubKey must be hexadecimal");
            vector<unsigned char> pkData(ParseHex(pkHex));
            CScript scriptPubKey(pkData.begin(), pkData.end());

            COutPoint outpoint(txid, nOut);
            if (mapPrevOut.count(outpoint))
            {
                // Complain if scriptPubKey doesn't match
                if (mapPrevOut[outpoint] != scriptPubKey)
                {
                    string err("Previous output scriptPubKey mismatch:\n");
                    err = err + mapPrevOut[outpoint].ToString() + "\nvs:\n"+
                        scriptPubKey.ToString();
                    throw JSONRPCError(RPC_DESERIALIZATION_ERROR, err);
                }
            }
            else
                mapPrevOut[outpoint] = scriptPubKey;
        }
    }

    bool fGivenKeys = false;
    CBasicKeyStore tempKeystore;
    if (params.size() > 2 && !params[2].isNull())
    {
        fGivenKeys = true;
        UniValue keys = params[2].get_array();
        for (unsigned int idx = 0; idx < keys.size(); idx++)
        {
            UniValue k = keys[idx];
            CBitcoinSecret vchSecret;
            bool fGood = vchSecret.SetString(k.get_str());
            if (!fGood)
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,"Invalid private key");
            CKey key;
            bool fCompressed;
            CSecret secret = vchSecret.GetSecret(fCompressed);
            key.SetSecret(secret, fCompressed);
            tempKeystore.AddKey(key);
        }
    }
    else
        EnsureWalletIsUnlocked();

    const CKeyStore& keystore = (fGivenKeys ? tempKeystore : *pwalletMain);

    int nHashType = SIGHASH_ALL;
    if (params.size() > 3 && !params[3].isNull())
    {
        static std::map<std::string, int> mapSigHashValues = {
            { "ALL"                , SIGHASH_ALL                           },
            { "ALL|ANYONECANPAY"   , SIGHASH_ALL | SIGHASH_ANYONECANPAY    },
            { "NONE"               , SIGHASH_NONE                          },
            { "NONE|ANYONECANPAY"  , SIGHASH_NONE | SIGHASH_ANYONECANPAY   },
            { "SINGLE"             , SIGHASH_SINGLE                        },
            { "SINGLE|ANYONECANPAY", SIGHASH_SINGLE | SIGHASH_ANYONECANPAY },
        };

        string strHashType = params[3].get_str();
        if (mapSigHashValues.count(strHashType))
            nHashType = mapSigHashValues[strHashType];
        else
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid sighash param");
    }

    bool fHashSingle = ((nHashType & ~SIGHASH_ANYONECANPAY) == SIGHASH_SINGLE);

    // Sign what we can:
    for (unsigned int i = 0; i < mergedTx.vin.size(); i++)
    {
        CTxIn& txin = mergedTx.vin[i];
        if (mapPrevOut.count(txin.prevout) == 0)
        {
            fComplete = false;
            continue;
        }
        const CScript& prevPubKey = mapPrevOut[txin.prevout];

        txin.scriptSig.clear();
        // Only sign SIGHASH_SINGLE if there's a corresponding output:
        if (!fHashSingle || (i < mergedTx.vout.size()))
            SignSignature(keystore, prevPubKey, mergedTx, i, nHashType);

        // ... and merge in other signatures:
        for (auto const& txv : txVariants)
        {
            txin.scriptSig = CombineSignatures(prevPubKey, mergedTx, i, txin.scriptSig, txv.vin[i].scriptSig);
        }
        if (!VerifyScript(txin.scriptSig, prevPubKey, mergedTx, i, 0))
            fComplete = false;
    }

    UniValue result(UniValue::VOBJ);
    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << mergedTx;
    result.pushKV("hex", HexStr(ssTx.begin(), ssTx.end()));
    result.pushKV("complete", fComplete);

    return result;
}

UniValue sendrawtransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1)
        throw runtime_error(
                "sendrawtransaction <hex string>\n"
                "\n"
                "Submits raw transaction (serialized, hex-encoded) to local node and network\n");

    RPCTypeCheck(params, { UniValue::VSTR });

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // parse hex string from parameter
    vector<unsigned char> txData(ParseHex(params[0].get_str()));
    CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
    CTransaction tx;

    // deserialize binary data stream
    try {
        ssData >> tx;
    }
    catch (std::exception &e) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }
    uint256 hashTx = tx.GetHash();

    // See if the transaction is already in a block
    // or in the memory pool:
    CTransaction existingTx;
    uint256 hashBlock;
    if (GetTransaction(hashTx, existingTx, hashBlock))
    {
        if (!hashBlock.IsNull())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("transaction already in block ")+hashBlock.GetHex());
        // Not in block, but already in the memory pool; will drop
        // through to re-relay it.
    }
    else
    {
        // push to local node
        if (!AcceptToMemoryPool(mempool, tx, nullptr))
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX rejected");

        SyncWithWallets(tx, nullptr, true);
    }
    RelayTransaction(tx, hashTx);

    return hashTx.GetHex();
}
