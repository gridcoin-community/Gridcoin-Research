// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "main.h"
#include "server.h"
#include "txmempool.h"
#include "gridcoin/contract/contract.h"

#include <rpc/util.h>
#include <univalue.h>

//! Render a mempool entry's cached metadata as a JSON object.
static UniValue MempoolEntryToJSON(const CTxMemPoolEntry& entry)
{
    UniValue obj(UniValue::VOBJ);

    obj.pushKV("size", (int64_t)entry.GetTxSize());
    obj.pushKV("fee", ValueFromAmount(entry.GetFee()));
    obj.pushKV("feerate", ValueFromAmount(entry.GetFeeRate()));
    obj.pushKV("time", entry.GetTime());
    obj.pushKV("height", entry.GetHeight());

    UniValue types(UniValue::VARR);
    for (const GRC::ContractType type : entry.GetContractTypes()) {
        types.push_back(GRC::Contract::Type::ToString(type));
    }
    obj.pushKV("contract_types", types);

    if (entry.HasMRC()) {
        obj.pushKV("mrc_cpid", entry.GetMRCCpid().ToString());
        obj.pushKV("mrc_fee", ValueFromAmount(entry.GetMRCFee()));
    }
    if (entry.HasBeacon()) {
        obj.pushKV("beacon_cpid", entry.GetBeaconCpid().ToString());
    }

    return obj;
}

static const RPCHelpMan getrawmempool_help{
    "getrawmempool",
    "Returns all transaction ids in the memory pool, or detailed information with verbose set.",
    {
        {"verbose", RPCArg::Type::BOOL, RPCArg::Optional::OMITTED,
            "True for a json object keyed by transaction id, false for an array of ids. Default: false."},
    },
    {
        RPCResult{"for verbose = false",
            RPCResult::Type::ARR, "", "",
            {
                {RPCResult::Type::STR_HEX, "", "Transaction id, hex-encoded."},
            }},
        RPCResult{"for verbose = true",
            RPCResult::Type::OBJ_DYN, "", "",
            {
                {RPCResult::Type::OBJ, "transactionid", "",
                    {
                        {RPCResult::Type::NUM, "size", "Serialized transaction size in bytes."},
                        {RPCResult::Type::STR_AMOUNT, "fee", "Transaction fee."},
                        {RPCResult::Type::STR_AMOUNT, "feerate", "Fee per 1000 bytes."},
                        {RPCResult::Type::NUM_TIME, "time", "Time the transaction entered the pool."},
                        {RPCResult::Type::NUM, "height", "Block height when the transaction entered the pool."},
                        {RPCResult::Type::ARR, "contract_types", "Gridcoin contract types in the transaction.",
                            {{RPCResult::Type::STR, "", "Contract type."}}},
                        {RPCResult::Type::STR, "mrc_cpid", /*optional=*/true, "CPID of the MRC contract (MRC transactions only)."},
                        {RPCResult::Type::STR_AMOUNT, "mrc_fee", /*optional=*/true, "Fee burned by the MRC contract (MRC transactions only)."},
                        {RPCResult::Type::STR, "beacon_cpid", /*optional=*/true, "CPID of the beacon advertisement (beacon transactions only)."},
                    }},
            }},
    },
    RPCExamples{
        HelpExampleCli("getrawmempool", "true") +
        HelpExampleRpc("getrawmempool", "true")},
};
const RPCHelpMan& getrawmempool_helpman() { return getrawmempool_help; }

UniValue getrawmempool(const UniValue& params)
{
    const bool verbose = !params.empty() && !params[0].isNull() && params[0].get_bool();

    LOCK(mempool.cs);

    if (!verbose) {
        UniValue arr(UniValue::VARR);
        for (const auto& [hash, entry] : mempool.mapTx) {
            arr.push_back(hash.ToString());
        }
        return arr;
    }

    UniValue obj(UniValue::VOBJ);
    for (const auto& [hash, entry] : mempool.mapTx) {
        obj.pushKV(hash.ToString(), MempoolEntryToJSON(entry));
    }
    return obj;
}

static const RPCHelpMan getmempoolentry_help{
    "getmempoolentry",
    "Returns mempool data for the given transaction.",
    {
        {"txid", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The transaction id (must be in the mempool)."},
    },
    RPCResult{RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::NUM, "size", "Serialized transaction size in bytes."},
            {RPCResult::Type::STR_AMOUNT, "fee", "Transaction fee."},
            {RPCResult::Type::STR_AMOUNT, "feerate", "Fee per 1000 bytes."},
            {RPCResult::Type::NUM_TIME, "time", "Time the transaction entered the pool."},
            {RPCResult::Type::NUM, "height", "Block height when the transaction entered the pool."},
            {RPCResult::Type::ARR, "contract_types", "Gridcoin contract types in the transaction.",
                {{RPCResult::Type::STR, "", "Contract type."}}},
            {RPCResult::Type::STR, "mrc_cpid", /*optional=*/true, "CPID of the MRC contract (MRC transactions only)."},
            {RPCResult::Type::STR_AMOUNT, "mrc_fee", /*optional=*/true, "Fee burned by the MRC contract (MRC transactions only)."},
            {RPCResult::Type::STR, "beacon_cpid", /*optional=*/true, "CPID of the beacon advertisement (beacon transactions only)."},
        }},
    RPCExamples{
        HelpExampleCli("getmempoolentry", "\"mytxid\"") +
        HelpExampleRpc("getmempoolentry", "\"mytxid\"")},
};
const RPCHelpMan& getmempoolentry_helpman() { return getmempoolentry_help; }

UniValue getmempoolentry(const UniValue& params)
{
    const uint256 hash = uint256S(params[0].get_str());

    LOCK(mempool.cs);

    const auto it = mempool.mapTx.find(hash);
    if (it == mempool.mapTx.end()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Transaction not in mempool");
    }

    return MempoolEntryToJSON(it->second);
}

static const RPCHelpMan getmempoolinfo_help{
    "getmempoolinfo",
    "Returns details on the active state of the transaction memory pool.",
    {},
    RPCResult{RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::NUM, "size", "Current transaction count."},
            {RPCResult::Type::NUM, "bytes", "Sum of all transaction serialized sizes."},
            {RPCResult::Type::NUM, "usage", "Total estimated memory usage of the mempool."},
            {RPCResult::Type::NUM, "maxmempool", "Maximum memory usage for the mempool, in bytes."},
            {RPCResult::Type::NUM, "mrc_count", "Number of MRC contract transactions in the pool."},
            {RPCResult::Type::NUM, "beacon_count", "Number of distinct CPIDs with a pending beacon advertisement in the pool (duplicate same-CPID advertisements collapse to one)."},
            {RPCResult::Type::NUM, "mandatory_sidestake_count", "Number of mandatory sidestake transactions in the pool."},
        }},
    RPCExamples{
        HelpExampleCli("getmempoolinfo", "") +
        HelpExampleRpc("getmempoolinfo", "")},
};
const RPCHelpMan& getmempoolinfo_helpman() { return getmempoolinfo_help; }

UniValue getmempoolinfo(const UniValue& params)
{
    const CTxMemPoolInfo info = mempool.GetMempoolInfo();

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("size", (int64_t)info.tx_count);
    obj.pushKV("bytes", (int64_t)info.bytes);
    obj.pushKV("usage", (int64_t)info.usage);
    obj.pushKV("maxmempool", (int64_t)info.max_size);
    obj.pushKV("mrc_count", (int64_t)info.mrc_count);
    obj.pushKV("beacon_count", (int64_t)info.beacon_count);
    obj.pushKV("mandatory_sidestake_count", (int64_t)info.mandatory_sidestake_count);

    return obj;
}
