// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
#include "protocol.h"
#include "server.h"
#include "streams.h"
#include "txdb.h"
#include "wallet/coincontrol.h"
#include "wallet/wallet.h"

#include <queue>

using namespace GRC;
using namespace std;

std::vector<std::pair<std::string, std::string>> GetTxStakeBoincHashInfo(const CMerkleTx& mtx)
{
    assert(mtx.IsCoinStake() || mtx.IsCoinBase());
    std::vector<std::pair<std::string, std::string>> res;

    // Fetch BlockIndex for tx block
    CBlockIndex* pindex = NULL;
    CBlock block;
    {
        BlockMap::iterator mi = mapBlockIndex.find(mtx.hashBlock);
        if (mi == mapBlockIndex.end())
        {
            res.push_back(std::make_pair(_("ERROR"), _("Block not in index")));
            return res;
        }

        pindex = (*mi).second;

        if (!block.ReadFromDisk(pindex))
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
        if (mi != mapBlockIndex.end() && (*mi).second)
        {
            CBlockIndex* pindex = (*mi).second;
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

    pwalletMain->AvailableCoins(vecOutputs, false, NULL, false);

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

                if (GetBoolArg("-enableaccounts", false))
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
    if (fHelp || params.size() < 1 || params.size() > 5)
        throw runtime_error(
                "consolidateunspent <address> [UTXO size] [maximum number of inputs] [sweep all addresses] [sweep change]\n"
                "\n"
                "<address>:                  The Gridcoin address target for consolidation.\n"
                "\n"
                "[UTXO size]:                Optional parameter for target consolidation output size.\n"
                "\n"
                "[maximum number of inputs]: Defaults to 50, clamped to 200 maximum to prevent transaction failures.\n"
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
                "a new smaller wallet, create a new address beforehand to serve as the target of the consolidation.\n");

    UniValue result(UniValue::VOBJ);

    std::string sAddress = params[0].get_str();
    CBitcoinAddress OptimizeAddress(sAddress);

    int64_t nConsolidateLimit = 0;
    // Set default maximum consolidation to 50 inputs if it is not specified. This is based
    // on performance tests on the Pi to ensure the transaction returns within a reasonable time.
    // The performance tests on the Pi show about 3 UTXOs/second. Intel machines should do
    // about 3x that. The GUI will not be responsive during the transaction due to locking.
    unsigned int nInputNumberLimit = 50;

    bool sweep_all_addresses = false;

    // Note this value is ignored if sweep_all_addresses is set to true.
    bool sweep_change = false;

    if (params.size() > 1) nConsolidateLimit = AmountFromValue(params[1]);

    if (params.size() > 2) nInputNumberLimit = params[2].get_int();

    if (params.size() > 3) sweep_all_addresses = params[3].get_bool();

    if (params.size() > 4 && !sweep_all_addresses) sweep_change = params[4].get_bool();

    // Clamp InputNumberLimit to 200. Above 200 risks an invalid transaction due to the size.
    nInputNumberLimit = std::min<unsigned int>(nInputNumberLimit, 200);

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
    pwalletMain->AvailableCoins(vecInputs, false, NULL, false);

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

            // The inital population of the queue is the input vector of the transaction that created the change UTXO.
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

        if(ExtractDestination(out.second.tx->vout[out.second.i].scriptPubKey, address))
        {
            CPubKey pubkey;
            CKeyID *keyid = boost::get<CKeyID>(&address);
            if (keyid && pwalletMain->GetPubKey(*keyid, pubkey))
            {
                nBytesInputs += (pubkey.IsCompressed() ? 148 : 180);
            }
            // in all error cases, simply assume 148 here
            else
            {
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
    int64_t nMinFee = txDummy.GetMinFee(1000, GMF_SEND, nBytes);

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

// MultiSig Tools
UniValue consolidatemsunspent(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 5)
        throw runtime_error(
                "consolidatemsunspent <address> <multi-sig-type> <block-start> <block-end> <max-grc> <max-inputs>\n"
                "\n"
                "Searches a block range for a multisig address with unspent utxos\n"
                "and consolidates them into a transaction ready for signing to\n"
                "return to the same address in an consolidated amount\n"
                "\n"
                "All parameters required.\n"
                "<address> --------> Multi-signature address\n"
                "<multi-sig-type> -> Type of multi-signature address\n"
                "<multi-sig-type> -> 1 = 2 of 3 (2 signatures required of 3)\n"
                "<multi-sig-type> -> 2 = 3 of 4 (3 signatures required of 4)\n"
                "<multi-sig-type> -> 3 = 3 of 5 (3 signatures required of 5)\n"
                "<multi-sig-type> -> 4 = 4 of 5 (4 signatures required of 5)\n"
                "<block-start> ----> Block number to start search from\n"
                "<block-end> ------> Block number to end search on\n"
                "<max-grc> --------> Highest uxto value to include in search results in halfords (0 is no limit)\n"
                "<max-inputs> -----> Maximum inputs allowed (hard limit on supported multisig types)\n"
                "<max-inputs> -----> Hard limit for 2 of 3 signatures is 40\n"
                "<max-inputs> -----> Hard limit for 3 of 4/5 signatures is 26\n"
                "<max-inputs> -----> Hard limit for 4 of 5 signatures is 20\n");

    UniValue result(UniValue::VOBJ);

    // Parameters
    std::string sAddress = params[0].get_str();
    int nReqSigsType = params[1].get_int();
    int nBlockStart = params[2].get_int();
    int nBlockEnd = params[3].get_int();
    int64_t nMaxValue = params[4].get_int64();
    int nMaxInputs = params[5].get_int();
    std::unordered_multimap<int64_t, std::pair<uint256, unsigned int>> umultimapInputs;

    // Parameter Sanity Check
    if (nBlockStart < 1 || nBlockStart > nBestHeight || nBlockStart > nBlockEnd)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid block-start");

    if (nBlockEnd < 1 || nBlockEnd > nBestHeight || nBlockEnd <= nBlockStart)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid block-end");

    if (nMaxInputs < 1)
        nMaxInputs = 1;

    else if (nMaxValue < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Value must not be less then 0");

    int nReqSigs = 0;
    int64_t nRedeemScriptSize = 0;

    if (nReqSigsType < 1 || nReqSigsType > 4)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid type of multi-signature address chosen");

    else if (nReqSigsType == 1)
    {
        nReqSigs = 2;
        nRedeemScriptSize = 210;

        if (nMaxInputs > 40)
            nMaxInputs = 40;
    }

    else if (nReqSigsType == 2)
    {
        nReqSigs = 3;
        nRedeemScriptSize = 278;

        if (nMaxInputs > 30)
            nMaxInputs = 30;
    }

    else if (nReqSigsType == 3)
    {
        nReqSigs = 3;
        nRedeemScriptSize = 346;

        if (nMaxInputs > 30)
            nMaxInputs = 30;
    }

    else if (nReqSigsType == 4)
    {
        nReqSigs = 4;
        nRedeemScriptSize = 346;

        if (nMaxInputs > 20)
            nMaxInputs = 20;
    }

    CBitcoinAddress Address(sAddress);

    if (!Address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gridcoin Address");

    LOCK(cs_main);

    GRC::BlockFinder blockfinder;

    CBlockIndex* pblkindex = blockfinder.FindByHeight((nBlockStart - 1));

    if (!pblkindex)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    bool fComplete = false;

    while (pblkindex->nHeight < nBlockEnd)
    {
        if (fComplete)
            break;

        pblkindex = pblkindex->pnext;

        CBlock block;

        if (!block.ReadFromDisk(pblkindex, true))
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

            // In case a fail here we can just continue thou it shouldn't happen
            if (!tx.ReadFromDisk(txdb, COutPoint(hash, 0), txindex))
                continue;

            // Extract the address from the transaction
            for (unsigned int j = 0; j < tx.vout.size(); j++)
            {
                if (fComplete)
                    break;

                const CTxOut& txout = tx.vout[j];
                CTxDestination txaddress;

                // Pass failures here thou we shouldn't have any failures
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

                    // shouldn't ever surpass this but lets just be safe!
                    if (umultimapInputs.size() >= (unsigned int) nMaxInputs)
                        fComplete = true;
                }
            }
        }
    }

    if (umultimapInputs.empty())
        throw JSONRPCError(RPC_INVALID_REQUEST, "Search resulted in no results");

    // Parse the inputs and make a raw transaction
    CTransaction rawtx;
    int64_t nTotal = 0;

    // Inputs
    for (const auto& inputs : umultimapInputs)
    {
        nTotal += inputs.first;

        CTxIn in(COutPoint(inputs.second.first, inputs.second.second));

        rawtx.vin.push_back(in);
    }

    /*
     * Fee factoring is important as size affects fees needed for the transaction
     * Fees are paid on the serialized size of a said transaction.
     * We don't know the serialized size of this transaction since we don't sign here
     * We cannot sign here as the signing potentially takes place by multiple wallets.
     * Serialize size can be equal or more then half of the hex size of completely signed transaction
     *
     * What we do know:
     * Redeemscript sizes (R):
     * 2 of 3 = 210
     * 3 of 4 = 278
     * 3 of 5 = 346
     * 4 of 5 = 346
     * Each signature size is between 146 to 148 from testing
     * S will be amount of signatures required
     * N will be the number of inputs
     *
     * To get estimated hex size for this is as follows:
     * sighexsize = (R + (S * 148)) * N
     *
     * From investigating i've determined that some of the hex's even with signature are predictable which helps refine the byte
     * calculation for estimation.
     *
     * Some parts of the hex transaction always start with 01 followed by padding of 000000
     * This occurs at beginning of the transaction before vin
     * This occurs after vin and before the signature
     * This occurs before vout as well except it contains no padding
     *
     * After a signature the hex contains 00ffffffff
     *
     * After the transaction the hex is padded with 0000000000
     *
     * The first vin is 74 in hex and every one after that is 64 in hex so we will assume they all 10 + (64 * inputs)
     *
     * This is very useful information for calculations on the transactions
     *
     * So can assume the base transaction will always have the 01000000vindata01000000sigdata00ffffffff01voutdata0000000000
     * This helps with calculations. We can assume some formulas:
     *
     * Total vin size will calculate as follows:
     * VINHEXSIZE V = (64 * N) + 10
     *
     * Total vin signatures size will calculate as follows:
     * SIGHEXSIZE H = (R + (S * 148)) * N
     *
     * Padding for vins will be calculated as follows:
     * VINP = (8 + 8 + 10) * N (To Shorten we will assume 26 * N)
     *
     * Total vout size we will assume is 70 since that's the biggest it appears to be able to be as a base size with max money
     * VOUTP = 2 + 10 (To Shorten we will assume 12)
     *
     * So in esscense the formula for all this will be:
     *
     * Potentialhexsize PHS = V + H + VINP + VOUTP
     *
     * Potentialbytesuze PBS = PHS / 2
     *
     * Note: this will keep the size pretty close to the real size.
     * This also leaves buffer room in case and this should always be an overestimation of the actual sizes
     * Sizes vary by the behaviour of the hex/serialization of the hex as well.
     *
    */

    int64_t nFee = 0;
    int64_t nMinFee = 0;
    int64_t nTxFee = 0;
    int64_t nBytes = 0;
    int64_t nOutput = 0;
    int64_t nEstHexSize = 0;
    int64_t nVInHexSize = 0;
    int64_t nSigHexSize = 0;
    int64_t nVInPadding = 0;
    int64_t nInputs = umultimapInputs.size();

    nVInHexSize = (64 * nInputs) + 10;
    nSigHexSize = (nRedeemScriptSize + (nReqSigs * 148)) * nInputs;
    nVInPadding = 26 * nInputs;
    nEstHexSize = nVInHexSize + nSigHexSize + nVInPadding + 12;

    nBytes = nEstHexSize / 2;
    nMinFee = rawtx.GetMinFee(1000, GMF_SEND, nBytes);
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

    result.push_back(std::make_pair("Block Start", nBlockStart));
    result.push_back(std::make_pair("Block End", nBlockEnd));
    // Let rpc caller know this was the last block we were in especially if the target amount of inputs was met before this End Block
    result.push_back(std::make_pair("Last Block Checked before target inputs amount", pblkindex->nHeight));
    result.push_back(std::make_pair("Amount of Inputs", nInputs));
    result.push_back(std::make_pair("Total GRC In", ValueFromAmount(nTotal)));
    result.push_back(std::make_pair("Fee", nTxFee));
    result.push_back(std::make_pair("Output Amount", ValueFromAmount(nOutput)));
    result.push_back(std::make_pair("Estimated signed hex size", nEstHexSize));
    result.push_back(std::make_pair("Estimated Serialized size", nBytes));
    result.push_back(std::make_pair("RawTX", sHash));

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

            if (!block.ReadFromDisk(pblkindex, true))
                throw JSONRPCError(RPC_PARSE_ERROR, "Unable to read block from disk!");

            for (unsigned int i = 1; i < block.vtx.size(); i++)
            {
                // Load Transaction
                CTransaction tx;
                CTxDB txdb("r");
                CTxIndex txindex;
                uint256 hash;

                hash = block.vtx[i].GetHash();

                // In case a fail here we can just continue thou it shouldn't happen
                if (!tx.ReadFromDisk(txdb, COutPoint(hash, 0), txindex))
                    continue;

                // Extract the address from the transaction
                for (unsigned int j = 0; j < tx.vout.size(); j++)
                {
                    const CTxOut& txout = tx.vout[j];
                    CTxDestination txaddress;

                    // Pass failures here thou we shouldn't have any failures
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

            std::string backupdir = GetArg("-backupdir", "");

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
        tempTx.FetchInputs(txdb, unused, false, false, mapPrevTx, fInvalid);

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
        if (!AcceptToMemoryPool(mempool, tx, NULL))
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX rejected");

        SyncWithWallets(tx, NULL, true);
    }
    RelayTransaction(tx, hashTx);

    return hashTx.GetHex();
}
