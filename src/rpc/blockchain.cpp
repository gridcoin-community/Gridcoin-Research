// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "blockchain.h"
#include "gridcoin/protocol.h"
#include "gridcoin/scraper/scraper_registry.h"
#include "gridcoin/sidestake.h"
#include "node/blockstorage.h"
#include <util/string.h>
#include "gridcoin/mrc.h"
#include "gridcoin/support/block_finder.h"

#include <univalue.h>
#include <stdexcept>

extern CCriticalSection cs_ConvergedScraperStatsCache;
extern ConvergedScraperStats ConvergedScraperStatsCache;

using namespace std;

bool AskForOutstandingBlocks(uint256 hashStart);
bool ForceReorganizeToHash(uint256 NewHash);
extern UniValue MagnitudeReport(const GRC::Cpid cpid);
extern UniValue SuperblockReport(int lookback = 14, bool displaycontract = false, std::string cpid = "");
extern GRC::Superblock ScraperGetSuperblockContract(bool bStoreConvergedStats = false,
                                                    bool bContractDirectFromStatsUpdate = false,
                                                    bool bFromHousekeeping = false);
extern ScraperPendingBeaconMap GetPendingBeaconsForReport();
extern ScraperPendingBeaconMap GetVerifiedBeaconsForReport(bool from_global = false);
extern UniValue GetJSONVersionReport(const int64_t lookback, const bool full_version);
arith_uint256 GetChainTrust(const CBlockIndex* pindex);
double CoinToDouble(double surrogate);
extern void TxToJSON(const CTransaction& tx, const uint256 hashBlock, UniValue& entry);
UniValue ContractToJson(const GRC::Contract& contract);

UniValue MRCToJson(const GRC::MRC& mrc) {
    UniValue json(UniValue::VOBJ);

    json.pushKV("version", (int)mrc.m_version);

    json.pushKV("cpid", mrc.m_mining_id.ToString());
    json.pushKV("client_version", mrc.m_client_version);
    json.pushKV("organization", mrc.m_organization);

    json.pushKV("research_subsidy", ValueFromAmount(mrc.m_research_subsidy));
    json.pushKV("fee", ValueFromAmount(mrc.m_fee));

    json.pushKV("magnitude", mrc.m_magnitude);
    json.pushKV("magnitude_unit", mrc.m_magnitude_unit);

    json.pushKV("last_block_hash", mrc.m_last_block_hash.GetHex());
    json.pushKV("signature", EncodeBase64(mrc.m_signature.data(), mrc.m_signature.size()));

    return json;
}

UniValue ClaimToJson(const GRC::Claim& claim, const CBlockIndex* const pindex)
{
    UniValue json(UniValue::VOBJ);

    json.pushKV("version", (int)claim.m_version);
    json.pushKV("mining_id", claim.m_mining_id.ToString());
    json.pushKV("client_version", claim.m_client_version);
    json.pushKV("organization", claim.m_organization);

    json.pushKV("block_subsidy", ValueFromAmount(claim.m_block_subsidy));
    json.pushKV("research_subsidy", ValueFromAmount(claim.m_research_subsidy));

    // Version 11 blocks remove magnitude and magnitude unit from claims:
    if (pindex->nVersion >= 11) {
        json.pushKV("magnitude", pindex->Magnitude());
        json.pushKV("magnitude_unit", GRC::Tally::GetMagnitudeUnit(pindex));
    } else {
        json.pushKV("magnitude", claim.m_magnitude);
        json.pushKV("magnitude_unit", claim.m_magnitude_unit);
    }

    json.pushKV("m_mrc_tx_map_size", (uint64_t) claim.m_mrc_tx_map.size());
    UniValue mrcs(UniValue::VARR);
    if (claim.m_version >= 4) {
        CTxDB txdb("r");

        for (const auto& [_, tx_hash] : claim.m_mrc_tx_map) {
            CTxIndex txindex;
            if (!txdb.ReadTxIndex(tx_hash, txindex)) {
                continue;
            }

            CTransaction tx;
            if (!ReadTxFromDisk(tx, txindex.pos)) {
                continue;
            }

            for (const auto& contract : tx.GetContracts()) {
                if (contract.m_type != GRC::ContractType::MRC) {
                    continue;
                }

                mrcs.push_back(MRCToJson(contract.CopyPayloadAs<GRC::MRC>()));
            }
        }
    }

    json.pushKV("mrcs", mrcs);

    json.pushKV("signature", EncodeBase64(claim.m_signature.data(), claim.m_signature.size()));

    json.pushKV("quorum_hash", claim.m_quorum_hash.ToString());
    json.pushKV("quorum_address", claim.m_quorum_address);

    return json;
}

UniValue SuperblockToJson(const GRC::Superblock& superblock)
{
    UniValue magnitudes(UniValue::VOBJ);

    for (const auto& cpid_iter : superblock.m_cpids) {
        magnitudes.pushKV(cpid_iter.Cpid().ToString(), cpid_iter.Magnitude().Floating());
    }

    UniValue projects(UniValue::VOBJ);

    for (const auto& project_pair : superblock.m_projects) {
        UniValue project(UniValue::VOBJ);

        project.pushKV("average_rac", project_pair.second.m_average_rac);
        project.pushKV("rac", project_pair.second.m_rac);
        project.pushKV("total_credit", project_pair.second.m_total_credit);

        projects.pushKV(project_pair.first, project);
    }

    UniValue beacons(UniValue::VARR);

    for (const auto& key_id : superblock.m_verified_beacons.m_verified) {
        beacons.push_back(key_id.ToString());
    }

    UniValue json(UniValue::VOBJ);

    json.pushKV("version", (int)superblock.m_version);
    json.pushKV("magnitudes", std::move(magnitudes));
    json.pushKV("projects", std::move(projects));
    json.pushKV("beacons", std::move(beacons));

    return json;
}

UniValue SuperblockToJson(const GRC::SuperblockPtr& superblock)
{
    return SuperblockToJson(*superblock);
}

UniValue blockToJSON(const CBlock& block, const CBlockIndex* blockindex, bool fPrintTransactionDetail)
{
    UniValue result(UniValue::VOBJ);

    result.pushKV("hash", block.GetHash().GetHex());

    const GRC::MintSummary mint = block.GetMint();

    CMerkleTx txGen(block.vtx[0]);
    txGen.SetMerkleBranch(&block);

    result.pushKV("confirmations", txGen.GetDepthInMainChain());
    result.pushKV("size", (int)::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION));
    result.pushKV("height", blockindex->nHeight);
    result.pushKV("version", block.nVersion);
    result.pushKV("merkleroot", block.hashMerkleRoot.GetHex());
    result.pushKV("mint", ValueFromAmount(mint.m_total));
    result.pushKV("MoneySupply", ValueFromAmount(blockindex->nMoneySupply));
    result.pushKV("time", block.GetBlockTime());
    result.pushKV("nonce", (uint64_t)block.nNonce);
    result.pushKV("bits", strprintf("%08x", block.nBits));
    result.pushKV("difficulty", GRC::GetDifficulty(blockindex));
    result.pushKV("blocktrust", leftTrim(blockindex->GetBlockTrust().GetHex(), '0'));
    result.pushKV("chaintrust", leftTrim(GetChainTrust(blockindex).GetHex(), '0'));

    if (blockindex->pprev)
        result.pushKV("previousblockhash", blockindex->pprev->GetBlockHash().GetHex());
    if (blockindex->pnext)
        result.pushKV("nextblockhash", blockindex->pnext->GetBlockHash().GetHex());

    const GRC::Claim& claim = block.GetClaim();

    std::string PoRNarr = "";
    if (blockindex->IsProofOfStake() && claim.HasResearchReward()) {
        PoRNarr = "proof-of-research";
    }

    result.pushKV("flags", strprintf("%s%s",
        blockindex->IsProofOfStake() ? "proof-of-stake" : "proof-of-work",
        blockindex->GeneratedStakeModifier()? " stake-modifier": "") + " " + PoRNarr);

    result.pushKV("proofhash", blockindex->hashProof.GetHex());
    result.pushKV("entropybit", (int)blockindex->GetStakeEntropyBit());
    result.pushKV("modifier", strprintf("%016" PRIx64, blockindex->nStakeModifier));

    UniValue txinfo(UniValue::VARR);

    for (auto const& tx : block.vtx)
    {
        if (fPrintTransactionDetail)
        {
            UniValue entry(UniValue::VOBJ);

            entry.pushKV("txid", tx.GetHash().GetHex());
            TxToJSON(tx, uint256(), entry);

            txinfo.push_back(entry);
        }
        else
            txinfo.push_back(tx.GetHash().GetHex());
    }

    result.pushKV("tx", txinfo);

    if (block.IsProofOfStake())
        result.pushKV("signature", HexStr(block.vchBlockSig));

    result.pushKV("claim", ClaimToJson(block.GetClaim(), blockindex));

    if (LogInstance().WillLogCategory(BCLog::LogFlags::NET)) result.pushKV("BoincHash",block.vtx[0].hashBoinc);

    if (fPrintTransactionDetail && blockindex->IsSuperblock()) {
        result.pushKV("superblock", SuperblockToJson(block.GetSuperblock()));
    }

    result.pushKV("fees_collected", ValueFromAmount(mint.m_fees));

    if (block.nVersion >= 12) {
        GRC::MRCFees mrc_fees = block.GetMRCFees();

        result.pushKV("mrc_foundation_fees", ValueFromAmount(mrc_fees.m_mrc_foundation_fees));
        result.pushKV("mrc_staker_fees", ValueFromAmount(mrc_fees.m_mrc_staker_fees));
    }

    result.pushKV("IsSuperBlock", blockindex->IsSuperblock());
    result.pushKV("IsContract", blockindex->IsContract());

    return result;
}

UniValue dumpcontracts(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 5) {
        std::stringstream help;

        help << "dumpcontracts <contract_type> <file> [txids only] [low height] [high height]\n"
                "\n"
                "<contract_type> Contract type to gather data from. Use \"*\" for all.\n"
                "Valid contract types: ";

        // Skip UNKNOWN here.
        for (int type = static_cast<int>(GRC::ContractType::UNKNOWN) + 1;
             type < static_cast<int>(GRC::ContractType::OUT_OF_BOUND); ++type) {
            if (type > 1) help << ", ";
            help << GRC::Contract::Type(static_cast<GRC::ContractType>(type)).ToString();
        }

        help << ".\n\n"
                "<file>          Output file.\n"
                "<txids only>    Optional txids only. (If specified just output txids and other minimal info in text.)\n"
                "<low height>    Optional low height. (If not specified then from genesis.)\n"
                "<high height>   Optional high height. (If not specified then current head.)\n "
                "\n"
                "Dump serialized or minimal textual contract data gathered from the chain to a specified file.\n";

        throw runtime_error(help.str());
    }

    std::string contract_type_string = params[0].get_str();
    std::optional<GRC::Contract::Type> contract_type;

    if (contract_type_string != "*") {
        contract_type = GRC::Contract::Type::Parse(params[0].get_str());

        if (*contract_type == GRC::ContractType::UNKNOWN)
            throw runtime_error("Invalid contract type");
    }

    fs::path path = fs::path(params[1].get_str());
    if (path.empty()) throw runtime_error("Invalid path.");

    fs::path DefaultPathDataDir = GetDataDir();

    // If provided filename does not have a path, then append parent path, otherwise leave alone.
    if (path.parent_path().empty())
        path = DefaultPathDataDir / path;

    if (fs::exists(path))
    {
        throw runtime_error("File already exists at location. Please delete or rename old file first.");
    }

    bool txids_only = false;
    if (params.size() > 2) {
        txids_only = params[2].get_bool();
    }

    int low_height = 0;
    int high_height = 0;

    if (params.size() > 3)
    {
        low_height = params[3].get_int();
    }

    if (params.size() > 4)
    {
        high_height = std::max(low_height, params[4].get_int());
    }

    UniValue report(UniValue::VOBJ);
    int64_t high_height_time = 0;
    int num_blocks = 0;
    int num_contracts = 0;
    int num_verified_beacons = 0;

    CBlock block;

    LOCK(cs_main);

    // Set default high_height here if not specified above now that lock on cs_main is taken.
    if (!high_height)
    {
        high_height = pindexBest->nHeight;
    }

    CBlockIndex* pblockindex = pindexBest;

    // Rewind to high height to get time.
    for (; pblockindex; pblockindex = pblockindex->pprev)
    {
        if (pblockindex->nHeight == high_height) break;
    }

    high_height_time = pblockindex->nTime;

    // Continue rewinding to low height.
    for (; pblockindex; pblockindex = pblockindex->pprev)
    {
        if (pblockindex->nHeight == low_height) break;
    }

    // From this point we have to go down two somewhat different paths based on whether a minimalist text output
    // is desired or the full serialization output.
    if (txids_only) {
        fsbridge::ofstream file;
        file.open(path, std::ios_base::out | std::ios_base::app);

        std::stringstream ss;

        file << "high_height_time,low_height,high_height,num_blocks\n";
        file << high_height_time << ","
             << low_height << ","
             << high_height << ",";

        ss << "txid,contract_type,contract_version,contract_action\n";

        while (pblockindex != nullptr && pblockindex->nHeight <= high_height) {

            ReadBlockFromDisk(block, pblockindex, Params().GetConsensus());

            bool include_element_in_export = false;

            for (auto& tx : block.vtx) {
                for (const auto& contract : tx.GetContracts()) {
                    if (!contract_type || contract.m_type == contract_type) {
                        LogPrintf("INFO: %s: txid %s, contract type %s, contract version %u, contract action %s\n",
                                  __func__,
                                  tx.GetHash().GetHex(),
                                  contract.m_type.ToString(),
                                  contract.m_version,
                                  contract.m_action.ToString());
                        ss << tx.GetHash().GetHex() << ","
                           << contract.m_type.ToString() << ","
                           << contract.m_version << ","
                           << contract.m_action.ToString() << "\n";
                        include_element_in_export = true;
                        ++num_contracts;
                    }
                }
            }

            if (include_element_in_export) ++num_blocks;

            pblockindex = pblockindex->pnext;
        }

        file << num_blocks << "\n";
        file << ss.str();
    } else {
        CAutoFile file(fsbridge::fopen(path, "wb"), SER_DISK, PROTOCOL_VERSION);

        CDataStream ss(SER_DISK, PROTOCOL_VERSION);

        file << high_height_time
             << low_height
             << high_height;

        while (pblockindex != nullptr && pblockindex->nHeight <= high_height) {

            ReadBlockFromDisk(block, pblockindex, Params().GetConsensus());

            bool include_element_in_export = false;

            // Initialize a new export element from the current block index
            GRC::ExportContractElement element(pblockindex);

            // Put the contracts and the containing transactions in the export element.
            for (auto& tx : block.vtx) {
                for (const auto& contract : tx.GetContracts()) {
                    if (!contract_type || contract.m_type == contract_type) {
                        element.m_ctx.push_back(std::make_pair(contract, tx));
                        include_element_in_export = true;
                        ++num_contracts;
                    }
                }
            }

            if (pblockindex->IsSuperblock() && (contract_type == GRC::ContractType::BEACON || !contract_type)) {
                GRC::SuperblockPtr psuperblock = block.GetSuperblock(pblockindex);

                for (const auto& key : psuperblock->m_verified_beacons.m_verified) {
                    element.m_verified_beacons.push_back(key);
                    ++num_verified_beacons;
                }

                // We have to include superblocks in the export even if no beacons were activated, because
                // pending beacons are checked for expiration and marked expired on the same trigger.
                include_element_in_export = true;
            }

            if (include_element_in_export)
            {
                // Serialize the export element to the datastream.
                ss << element;

                ++num_blocks;
            }

            pblockindex = pblockindex->pnext;
        }

        file << num_blocks;
        file << ss;
    }

    // These are for informational purposes only to the caller.
    report.pushKV("timestamp", high_height_time);
    report.pushKV("low_height", low_height);
    report.pushKV("high_height", high_height);
    report.pushKV("contract_type", contract_type_string);
    report.pushKV("number_of_blocks_processed_containing_contract_type", num_blocks);
    report.pushKV("number_of_contracts_exported", num_contracts);
    if (contract_type == GRC::ContractType::BEACON) report.pushKV("number_of_beacons_verified", num_verified_beacons);
    report.pushKV("exported_to_file", path.string());

    return report;
}

UniValue getmrcinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 4)
        throw runtime_error(
                "getmrcinfo [detailed MRC info [CPID [low height [high height]]]]\n"
                "\n"
                "[detailed MRC info]: optional boolean to output MRC details.\n"
                "                     Defaults to false.\n"
                "[CPID]:              optional CPID. Defaults to current wallet CPID.\n"
                "                     Use \"*\" for all CPIDs (network wide).\n"
                "                     Note that block level mrc summary statistics are\n"
                "                     specific to the scope specified with CPID.\n"
                "[low height]:        optional low height for scope.\n"
                "                     Defaults to V12 block height.\n"
                "[high height]:       optional high height for scope.\n"
                "                     Defaults to current block.\n"
                );

    bool output_mrc_details = false;
    bool output_all_cpids = false;

    if (params.size() > 0) {
        output_mrc_details = params[0].get_bool();
    }

    GRC::MiningId mining_id;

    LOCK(cs_main);

    if (params.size() > 1) {
        std::string cpid_string = params[1].get_str();

        if (cpid_string == "*") {
            output_all_cpids = true;
        } else {
            mining_id = GRC::MiningId::Parse(cpid_string);
        }
    } else {
        mining_id = GRC::Researcher::Get()->Id();
    }

    if (!output_all_cpids && !mining_id.Valid()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid CPID.");
    }

    GRC::CpidOption cpid = mining_id.TryCpid();

    if (!output_all_cpids && !cpid) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "No data for investor.");
    }

    // No MRC's below V12 block height.
    int low_height = Params().GetConsensus().BlockV12Height;
    int high_height = 0;

    if (params.size() > 2) {
        // If specified low height is lower than V12 height, set to V12 height.
        low_height = std::max(params[2].get_int(), low_height);
    }

    if (params.size() > 3) {
        // High height can't be lower than the low height.
        high_height = std::max(low_height, params[3].get_int());
    }

    UniValue report(UniValue::VOBJ);
    UniValue block_output_array(UniValue::VARR);

    uint64_t total_mrcs_paid = 0;
    uint64_t total_mrcs_fee_boosted = 0;

    CAmount mrc_total_research_rewards = 0;
    CAmount mrc_total_foundation_fees = 0;
    CAmount mrc_total_staker_fees = 0;
    CAmount mrc_total_calculated_minimum_fees = 0;
    CAmount mrc_total_fee_boost = 0;

    CBlock block;
    UniValue block_output(UniValue::VOBJ);

    // Set default high_height here if not specified above now that lock on cs_main is taken.
    if (!high_height) {
        high_height = pindexBest->nHeight;
    }

    CBlockIndex* blockindex = pindexBest;

    // Rewind to low height.
    for (; blockindex; blockindex = blockindex->pprev) {
        if (blockindex->nHeight == low_height) break;
    }

    while (blockindex && blockindex->nHeight <= high_height) {
        CAmount mrc_research_rewards = 0;

        for (const auto& mrc_context : blockindex->m_mrc_researchers) {
            if (output_all_cpids || mrc_context->m_cpid == *cpid) {
                mrc_research_rewards += mrc_context->m_research_subsidy;
            }
        }

        if (!mrc_research_rewards) {
            blockindex = blockindex->pnext;
            continue;
        }

        ReadBlockFromDisk(block, blockindex, Params().GetConsensus());

        // Get the claim which is where MRCs are actually paid.
        GRC::Claim claim = block.GetClaim();

        GRC::MRCFees mrc_fees;
        CAmount mrc_fee_boost = 0;
        uint64_t mrcs_paid = 0;
        uint64_t mrcs_fee_boosted = 0;

        if (output_all_cpids) {
            mrc_fees = block.GetMRCFees();
            mrc_fee_boost = mrc_fees.m_mrc_foundation_fees
                    + mrc_fees.m_mrc_staker_fees
                    - mrc_fees.m_mrc_minimum_calc_fees;

            mrcs_paid = claim.m_mrc_tx_map.size(); // This also matches the size of the blockindex->m_mrc_researchers

            if (output_mrc_details) {
                UniValue mrc_requests_output_array(UniValue::VARR);
                uint64_t mrc_requests = 0;

                block_output.pushKV("hash", block.GetHash().GetHex());
                block_output.pushKV("height", blockindex->nHeight);
                block_output.pushKV("mrc_research_rewards", ValueFromAmount(mrc_research_rewards));
                block_output.pushKV("mrc_foundation_fees", ValueFromAmount(mrc_fees.m_mrc_foundation_fees));
                block_output.pushKV("mrc_staker_fees", ValueFromAmount(mrc_fees.m_mrc_staker_fees));
                block_output.pushKV("mrc_net_paid_to_researchers", ValueFromAmount(mrc_research_rewards
                                                                                   - mrc_fees.m_mrc_foundation_fees
                                                                                   - mrc_fees.m_mrc_staker_fees));
                block_output.pushKV("mrc_calculated_minimum_fees", ValueFromAmount(mrc_fees.m_mrc_minimum_calc_fees));
                block_output.pushKV("mrc_fee_boost", ValueFromAmount(mrc_fee_boost));
                block_output.pushKV("mrcs_paid", mrcs_paid);
                block_output.pushKV("claim", ClaimToJson(block.GetClaim(), blockindex));

                for (const auto& tx : block.vtx) {
                    for (const auto& contract : tx.GetContracts()) {
                        // We are only interested in MRC request contracts here.
                        if (contract.m_type != GRC::ContractType::MRC) continue;

                        GRC::MRC mrc = contract.CopyPayloadAs<GRC::MRC>();

                        ++mrc_requests;

                        CAmount mrc_calculated_min_fee = mrc.ComputeMRCFee();

                        UniValue mrc_output(UniValue::VOBJ);

                        mrc_output.pushKV("txid", tx.GetHash().GetHex());
                        mrc_output.pushKVs(MRCToJson(mrc));
                        mrc_output.pushKV("mrc_calculated_minimum_fee", ValueFromAmount(mrc_calculated_min_fee));

                        if (mrc.m_fee > mrc_calculated_min_fee) ++mrcs_fee_boosted;

                        mrc_requests_output_array.push_back(mrc_output);

                    } // contracts
                } // transaction

                block_output.pushKV("mrc_requests", mrc_requests_output_array);

                if (mrc_requests) {
                    block_output_array.push_back(block_output);
                }
            } else { // no details, but get the # of mrcs that had fee boosting
                for (const auto& tx : block.vtx) {
                    for (const auto& contract : tx.GetContracts()) {
                        // We are only interested in MRC request contracts here.
                        if (contract.m_type != GRC::ContractType::MRC) continue;

                        GRC::MRC mrc = contract.CopyPayloadAs<GRC::MRC>();

                        CAmount mrc_calculated_min_fee = mrc.ComputeMRCFee();

                        if (mrc.m_fee > mrc_calculated_min_fee) ++mrcs_fee_boosted;
                    } // contracts
                } // transaction
            } // output_mrc_details
        } else { // specific CPID
            UniValue mrc_requests_output_array(UniValue::VARR);

            for (const auto& tx : block.vtx) {
                for (const auto& contract : tx.GetContracts()) {
                    // We are only interested in MRC request contracts here.
                    if (contract.m_type != GRC::ContractType::MRC) continue;

                    GRC::MRC mrc = contract.CopyPayloadAs<GRC::MRC>();

                    if (mrc.m_mining_id != *cpid) continue;

                    ++mrcs_paid;

                    Fraction foundation_fee_fraction = FoundationSideStakeAllocation();

                    CAmount mrc_foundation_fee = mrc.m_fee * foundation_fee_fraction.GetNumerator()
                                                           / foundation_fee_fraction.GetDenominator();

                    mrc_fees.m_mrc_foundation_fees += mrc_foundation_fee;

                    mrc_fees.m_mrc_staker_fees += mrc.m_fee - mrc_foundation_fee;

                    CAmount mrc_calculated_min_fee = mrc.ComputeMRCFee();

                    mrc_fees.m_mrc_minimum_calc_fees += mrc_calculated_min_fee;
                    mrc_fee_boost += mrc.m_fee - mrc_calculated_min_fee;

                    UniValue mrc_output(UniValue::VOBJ);

                    mrc_output.pushKV("txid", tx.GetHash().GetHex());
                    mrc_output.pushKVs(MRCToJson(mrc));
                    mrc_output.pushKV("mrc_calculated_minimum_fee", ValueFromAmount(mrc_calculated_min_fee));

                    if (mrc.m_fee > mrc_calculated_min_fee) ++mrcs_fee_boosted;

                    mrc_requests_output_array.push_back(mrc_output);

                } // contracts
            } // transaction

            if (output_mrc_details) {
                block_output.pushKV("hash", block.GetHash().GetHex());
                block_output.pushKV("height", blockindex->nHeight);
                block_output.pushKV("mrc_research_rewards", ValueFromAmount(mrc_research_rewards));
                block_output.pushKV("mrc_foundation_fees", ValueFromAmount(mrc_fees.m_mrc_foundation_fees));
                block_output.pushKV("mrc_staker_fees", ValueFromAmount(mrc_fees.m_mrc_staker_fees));
                block_output.pushKV("mrc_net_paid_to_researchers", ValueFromAmount(mrc_research_rewards
                                                                                   - mrc_fees.m_mrc_foundation_fees
                                                                                   - mrc_fees.m_mrc_staker_fees));
                block_output.pushKV("mrc_calculated_minimum_fees", ValueFromAmount(mrc_fees.m_mrc_minimum_calc_fees));
                block_output.pushKV("mrc_fee_boost", ValueFromAmount(mrc_fee_boost));
                block_output.pushKV("mrcs_paid", mrcs_paid);
                block_output.pushKV("claim", ClaimToJson(block.GetClaim(), blockindex));

                block_output.pushKV("mrc_requests", mrc_requests_output_array);

                if (mrcs_paid) {
                    block_output_array.push_back(block_output);
                }
            }
        }

        mrc_total_foundation_fees += mrc_fees.m_mrc_foundation_fees;
        mrc_total_staker_fees += mrc_fees.m_mrc_staker_fees;
        mrc_total_calculated_minimum_fees += mrc_fees.m_mrc_minimum_calc_fees;
        mrc_total_fee_boost += mrc_fee_boost;
        total_mrcs_paid += mrcs_paid;
        total_mrcs_fee_boosted += mrcs_fee_boosted;
        mrc_total_research_rewards += mrc_research_rewards;
        blockindex = blockindex->pnext;
    } // while (pblockindex...)

    report.pushKV("total_mrcs_paid", total_mrcs_paid);
    report.pushKV("total_mrcs_fee_boosted", total_mrcs_fee_boosted);
    report.pushKV("mrc_total_research_rewards", ValueFromAmount(mrc_total_research_rewards));
    report.pushKV("mrc_total_foundation_fees", ValueFromAmount(mrc_total_foundation_fees));
    report.pushKV("mrc_total_staker_fees", ValueFromAmount(mrc_total_staker_fees));
    report.pushKV("mrc_total_net_paid_to_researchers", ValueFromAmount(mrc_total_research_rewards
                                                                       - mrc_total_foundation_fees
                                                                       - mrc_total_staker_fees));
    report.pushKV("mrc_total_calculated_minimum_fees", ValueFromAmount(mrc_total_calculated_minimum_fees));
    report.pushKV("mrc_total_fee_boost", ValueFromAmount(mrc_total_fee_boost));

    if (output_mrc_details) {
        report.pushKV("mrc_details_by_block", block_output_array);
    }

    return report;
}

UniValue showblock(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "showblock <index>\n"
                "\n"
                "<index> Block number\n"
                "\n"
                "Returns all information about the block at <index>\n");

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > nBestHeight)
        throw runtime_error("Block number out of range\n");

    LOCK(cs_main);

    CBlockIndex* pblockindex = GRC::BlockFinder::FindByHeight(nHeight);

    if (pblockindex == nullptr)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
    CBlock block;
    ReadBlockFromDisk(block, pblockindex, Params().GetConsensus());
    return blockToJSON(block, pblockindex, false);
}

UniValue getbestblockhash(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getbestblockhash\n"
                "\n"
                "Returns the hash of the best block in the longest block chain\n");

    LOCK(cs_main);

    return hashBestChain.GetHex();
}

UniValue getblockcount(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getblockcount\n"
                "\n"
                "Returns the number of blocks in the longest block chain\n");

    LOCK(cs_main);

    return nBestHeight;
}

UniValue getdifficulty(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getdifficulty\n"
                "\n"
                "Returns the difficulty as a multiple of the minimum difficulty\n");

    LOCK(cs_main);

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("current", GRC::GetCurrentDifficulty());
    obj.pushKV("target", GRC::GetTargetDifficulty());

    return obj;
}

UniValue settxfee(const UniValue& params, bool fHelp)
{
    LOCK(cs_main);

    CTransaction txDummy;

    // Min Fee
    CAmount nMinFee = GetBaseFee(txDummy, GMF_SEND);

    if (fHelp || params.size() < 1 || params.size() > 1 || AmountFromValue(params[0]) < nMinFee)
        throw runtime_error(
                "settxfee <amount>\n"
                "\n"
                "<amount> is a real and is rounded to the nearest 0.00000001\n"
                "\n"
                "Sets the txfee for transactions\n");

    nTransactionFee = AmountFromValue(params[0]);

    return true;
}

UniValue getrawmempool(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getrawmempool\n"
                "\n"
                "Returns all transaction ids in memory pool\n");


    vector<uint256> vtxid;

    {
        LOCK(mempool.cs);

        mempool.queryHashes(vtxid);
    }

    UniValue a(UniValue::VARR);
    for (auto const& hash : vtxid)
        a.push_back(hash.ToString());

    return a;
}

UniValue getblockhash(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "getblockhash <index>\n"
                "\n"
                "<index> Block number for requested hash\n"
                "\n"
                "Returns hash of block in best-block-chain at <index>\n");

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > nBestHeight)
        throw runtime_error("Block number out of range.");

    LogPrint(BCLog::LogFlags::NOISY, "Getblockhash %d", nHeight);

    LOCK(cs_main);

    CBlockIndex* RPCpblockindex = GRC::BlockFinder::FindByHeight(nHeight);

    return RPCpblockindex->phashBlock->GetHex();
}

UniValue getblock(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
                "getblock <hash> [bool:txinfo]\n"
                "\n"
                "[bool:txinfo] optional to print more detailed tx info\n"
                "\n"
                "Returns details of a block with given block-hash\n");

    std::string strHash = params[0].get_str();
    uint256 hash = uint256S(strHash);

    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    LOCK(cs_main);

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];
    ReadBlockFromDisk(block, pblockindex, Params().GetConsensus());

    return blockToJSON(block, pblockindex, params.size() > 1 ? params[1].get_bool() : false);
}

UniValue getblockbynumber(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
                "getblockbynumber <number> [bool:txinfo]\n"
                "\n"
                "[bool:txinfo] optional to print more detailed tx info\n"
                "\n"
                "Returns details of a block with given block-number\n");

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > nBestHeight)
        throw runtime_error("Block number out of range");

    LOCK(cs_main);

    CBlock block;

    CBlockIndex* pblockindex = GRC::BlockFinder::FindByHeight(nHeight);
    ReadBlockFromDisk(block, pblockindex, Params().GetConsensus());

    return blockToJSON(block, pblockindex, params.size() > 1 ? params[1].get_bool() : false);
}

UniValue getblockbymintime(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
                "getblockbymintime <timestamp> [bool:txinfo]\n"
                "\n"
                "[bool:txinfo] optional to print more detailed tx info\n"
                "\n"
                "Returns details of the block at or just after the given timestamp\n");

    int64_t nTimestamp = params[0].get_int64();

    if (nTimestamp < pindexGenesisBlock->nTime || nTimestamp > pindexBest->nTime)
        throw runtime_error("Timestamp out of range. Cannot be below the time of the genesis block or above the time of the latest block");

    LOCK(cs_main);

    CBlock block;

    CBlockIndex* pblockindex = GRC::BlockFinder::FindByMinTime(nTimestamp);
    ReadBlockFromDisk(block, pblockindex, Params().GetConsensus());

    return blockToJSON(block, pblockindex, params.size() > 1 ? params[1].get_bool() : false);
}

UniValue getblocksbatch(const UniValue& params, bool fHelp)
{
    g_timer.InitTimer(__func__, LogInstance().WillLogCategory(BCLog::LogFlags::RPC));

    if (fHelp || params.size() < 2 || params.size() > 3)
    {
        throw runtime_error(
                "getblocksbatch <starting block number or hash> <number of blocks> [bool:txinfo]\n"
                "\n"
                "<starting block number or hash> the block number or hash for the block at the\n"
                "start of the batch\n"
                "\n"
                "<number of blocks> the number of blocks to return in the batch, limited to 1000"
                "\n"
                "[bool:txinfo] optional to print more detailed tx info\n"
                "\n"
                "Returns a JSON array with details of the requested blocks starting with\n"
                "the given block-number or hash.\n");
    }

    UniValue result(UniValue::VOBJ);
    UniValue blocks(UniValue::VARR);

    int nHeight = 0;
    uint256 hash;
    bool block_hash_provided = false;

    // Validate parameters.
    try
    {
        // Have to do it this way, because the rpc param 0 must be left out of the special parameter handling in client.cpp.
        if (params[0].isNum())
        {
            nHeight = params[0].get_int();
        }
        else
        {
            nHeight = boost::lexical_cast<int>(params[0].get_str());
        }
    }
    catch (const boost::bad_lexical_cast& e)
    {
        std::string strHash = params[0].get_str();
        hash = uint256S(strHash);
        block_hash_provided = true;
    }
    catch (...)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Either a valid block number or block hash must be provided.");
    }

    if (!block_hash_provided)
    {
        if (nHeight < 0 || nHeight > nBestHeight)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Starting block number out of range");
        }
    }
    else
    {
        if (mapBlockIndex.count(hash) == 0)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Starting block for batch not found.");
        }
    }

    int batch_size = params[1].get_int();
    if (batch_size < 1 || batch_size > 1000)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Batch size must be between 1 and 1000, inclusive.");
    }

    bool transaction_details = false;
    if (params.size() > 2) transaction_details = params[2].get_bool();

    LOCK(cs_main);

    g_timer.GetTimes("Finished validating parameters", __func__);

    CBlockIndex* pblockindex_head = nullptr;
    CBlockIndex* pblockindex = nullptr;

    // Find the starting block's index entry point by either rewinding from the head (if the block number was
    // provided), or directly from the mapBlockIndex, if the hash was provided.

    // Select the block index for the head of the chain.
    pblockindex_head = mapBlockIndex[hashBestChain];

    if (!block_hash_provided)
    {
        pblockindex = pblockindex_head;

        // Rewind to the block corresponding to the specified height.
        while (pblockindex->nHeight > nHeight)
        {
            pblockindex = pblockindex->pprev;
        }

    }
    else
    {
        pblockindex = mapBlockIndex[hash];
    }

    g_timer.GetTimes("Finished finding starting block", __func__);

    int i = 0;
    while (i < batch_size)
    {
        CBlock block;
        if (!ReadBlockFromDisk(block, pblockindex, Params().GetConsensus()))
        {
            throw runtime_error("Error reading block from specified batch.");
        }

        blocks.push_back(blockToJSON(block, pblockindex, transaction_details));
        ++i;

        if (pblockindex == pblockindex_head) break;

        pblockindex = pblockindex->pnext;
    }

    result.pushKV("block_count", i);
    result.pushKV("blocks", blocks);

    g_timer.GetTimes("Finished populating result for block batch", __func__);

    return result;
}

UniValue backupprivatekeys(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "backupprivatekeys\n"
                "\n"
                "Backup wallet private keys to file (Wallet must be fully unlocked!)\n");

    string sErrors;
    string sTarget;
    UniValue res(UniValue::VOBJ);

    bool bBackupPrivateKeys = GRC::BackupPrivateKeys(*pwalletMain, sTarget, sErrors);

    if (!bBackupPrivateKeys)
        res.pushKV("error", sErrors);

    else
        res.pushKV("location", sTarget);

    res.pushKV("result", bBackupPrivateKeys);

    return res;
}

UniValue rainbymagnitude(const UniValue& params, bool fHelp)
    {
    if (fHelp || (params.size() < 2 || params.size() > 4))
        throw runtime_error(
                "rainbymagnitude project_id amount ( trial_run output_details )\n"
                "\n"
                "project_id     -> Required: Limits rain to a specific project. Use \"*\" for\n"
                "                  network-wide. Call \"listprojects\" for the IDs of eligible\n"
                "                  projects."
                "amount         -> Required: Amount to rain (1000 GRC minimum).\n"
                "trial_run      -> Optional: Boolean to specify a trial run instead of an actual\n"
                "                  transaction (default: false).\n"
                "output_details -> Optional: Boolean to output recipient details (default: false\n"
                "                  if not trial run, true if trial run).\n"
                "\n"
                "rain coins by magnitude on network");

    UniValue res(UniValue::VOBJ);
    UniValue details(UniValue::VARR);

    std::string sProject = params[0].get_str();

    LogPrint(BCLog::LogFlags::VERBOSE, "rainbymagnitude: sProject = %s", sProject.c_str());

    CAmount amount = AmountFromValue(params[1]);

    if (amount < 1000 * COIN)
    {
        throw runtime_error("Minimum amount to rain is 1000 GRC.");
    }

    std::string sMessage;

    bool trial_run = false;

    if (params.size() > 2)
    {
        trial_run = params[2].get_bool();
    }

    bool output_details = trial_run;

    if (params.size() > 3)
    {
        output_details = params[3].get_bool();
    }

    // Make sure statistics are up to date. This will do nothing if a convergence has already been cached and is clean.
    bool bStatsAvail = ScraperGetSuperblockContract(false, false).WellFormed();

    if (!bStatsAvail)
    {
        throw JSONRPCError(RPC_MISC_ERROR, "Wallet has not formed a convergence from statistics information.");
    }

    ScraperStats mScraperConvergedStats;
    {
        LOCK(cs_ConvergedScraperStatsCache);

        // Make a local copy of the cached stats in the convergence and release the lock.
        mScraperConvergedStats = ConvergedScraperStatsCache.mScraperConvergedStats;
    }

    LogPrint(BCLog::LogFlags::VERBOSE, "rainbymagnitude: mScraperConvergedStats size = %u", mScraperConvergedStats.size());

    CAmount total_amount_1st_pass = 0;
    CAmount total_amount_2nd_pass = 0;

    double dTotalMagnitude = 0;

    statsobjecttype rainbymagmode = (sProject == "*" ? statsobjecttype::byCPID : statsobjecttype::byCPIDbyProject);

    const int64_t now = GetAdjustedTime(); // Time to calculate beacon expiration from

    //------- CPID -------------- beacon address -- Mag --- payment - suppressed
    std::map<GRC::Cpid, std::tuple<CBitcoinAddress, double, CAmount, bool>> mCPIDRain;

    for (const auto& entry : mScraperConvergedStats)
    {
        // Only consider entries along the specified dimension
        if (entry.first.objecttype == rainbymagmode)
        {
            GRC::Cpid CPIDKey;

            if (rainbymagmode == statsobjecttype::byCPIDbyProject)
            {
                std::vector<std::string> vObjectStatsKey = split(entry.first.objectID, ",");

                // Only process elements that match the specified project if in project level rain.
                if (vObjectStatsKey[0] != sProject) continue;

                CPIDKey = GRC::Cpid::Parse(vObjectStatsKey[1]);
            }
            else
            {
                CPIDKey = GRC::Cpid::Parse(entry.first.objectID);
            }

            double dCPIDMag = GRC::Magnitude::RoundFrom(entry.second.statsvalue.dMag).Floating();

            // Zero mag CPIDs do not get paid.
            if (!dCPIDMag) continue;

            CBitcoinAddress address;

            // If the beacon is active get the address and insert an entry into the map for payment,
            // otherwise skip.
            if (const GRC::BeaconOption beacon = GRC::GetBeaconRegistry().TryActive(CPIDKey, now))
            {
                address = beacon->GetAddress();
            }
            else
            {
                continue;
            }

            // The last two elements of the tuple will be filled out when doing the passes for payment.
            mCPIDRain[CPIDKey] = std::make_tuple(address, dCPIDMag, CAmount {0}, false);

            // Increment the accumulated mag. This will be equal to the total mag of the valid CPIDs entered
            // into the RAIN map, and will be used to normalize the payments.
            dTotalMagnitude += dCPIDMag;
        }
    }

    if (mCPIDRain.empty() || !dTotalMagnitude)
    {
        throw JSONRPCError(RPC_MISC_ERROR, "No CPIDs to pay and/or total CPID magnitude is zero. This could be caused by an "
                                           "incorrect project specified.");
    }

    std::vector<std::pair<CScript, CAmount> > vecSend;
    unsigned int subcent_suppression_count = 0;

    for (auto& iter : mCPIDRain)
    {
        double& dCPIDMag = std::get<1>(iter.second);

        CAmount payment = roundint64((double) amount * dCPIDMag / dTotalMagnitude);

        std::get<2>(iter.second) = payment;

        // Do not allow payments less than one cent to prevent dust in the network.
        if (payment < CENT)
        {
            std::get<3>(iter.second) = true;

            ++subcent_suppression_count;
            continue;
        }

        total_amount_1st_pass += payment;
    }

    // Because we are suppressing payments of less than one cent to CPIDs, we need to renormalize the payments to ensure
    // the full amount is disbursed to the surviving CPID payees. This is going to be a small amount, but is worth doing.
    for (const auto& iter : mCPIDRain)
    {
        // Make it easier to read.
        const GRC::Cpid& cpid = iter.first;
        const CBitcoinAddress& address = std::get<0>(iter.second);
        const double& magnitude = std::get<1>(iter.second);

        // This is not a const reference on purpose because it has to be renormalized.
        CAmount payment = std::get<2>(iter.second);
        const bool& suppressed = std::get<3>(iter.second);

        // Note the cast to double ensures that the calculations are reasonable over a wide dynamic range, without risk of
        // overflow on a large iter.second and amount without using bignum math. The slight loss of precision here is not
        // important. As above, with the renormalization here, we will round to the nearest Halford rather than truncating.
        payment = roundint64((double) payment * (double) amount / (double) total_amount_1st_pass);

        CScript scriptPubKey;
        scriptPubKey.SetDestination(address.Get());

        LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: cpid = %s. address = %s, magnitude = %.2f, "
                                           "payment = %s, dust_suppressed = %u",
                 __func__, cpid.ToString(), address.ToString(), magnitude, ValueFromAmount(payment).getValStr(), suppressed);

        if (output_details)
        {
            UniValue detail_entry(UniValue::VOBJ);

            detail_entry.pushKV("cpid", cpid.ToString());
            detail_entry.pushKV("address", address.ToString());
            detail_entry.pushKV("magnitude", magnitude);
            detail_entry.pushKV("amount", ValueFromAmount(payment));
            detail_entry.pushKV("suppressed", suppressed);

            details.push_back(detail_entry);
        }

        // If dust suppression flag is false, add to payment vector for sending.
        if (!suppressed)
        {
            vecSend.push_back(std::make_pair(scriptPubKey, payment));
            total_amount_2nd_pass += payment;
        }
    }

    if (total_amount_2nd_pass <= 0)
    {
        throw JSONRPCError(RPC_MISC_ERROR, "No payments above 0.01 GRC qualify. Please recheck your specified amount.");
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CWalletTx wtx;
    wtx.mapValue["comment"] = "Rain By Magnitude";

    // Custom messages are no longer supported. The static "rain by magnitude" message will be replaced by an actual
    // rain contract at the next mandatory.
    wtx.vContracts.emplace_back(GRC::MakeContract<GRC::TxMessage>(
        GRC::ContractAction::ADD,
        "Rain By Magnitude"));

    EnsureWalletIsUnlocked();

    // Check funds
    CAmount balance = pwalletMain->GetBalance();

    if (total_amount_2nd_pass > balance)
    {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Wallet has insufficient funds for specified rain.");
    }

    // Send
    CReserveKey keyChange(pwalletMain);

    int64_t nFeeRequired = 0;
    bool fCreated = pwalletMain->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired);

    LogPrintf("Rain By Magnitude Transaction Created.");

    if (!fCreated)
    {
        if (total_amount_2nd_pass + nFeeRequired > balance)
        {
            throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Wallet has insufficient funds for specified rain.");
        }

        throw JSONRPCError(RPC_WALLET_ERROR, "Transaction creation failed");
    }

    if (!trial_run)
    {
        // Rain the recipients
        if (!pwalletMain->CommitTransaction(wtx, keyChange))
        {
            error("%s: Rain by magnitude transaction commit failed.", __func__);

            throw JSONRPCError(RPC_WALLET_ERROR, "Rain by magnitude transaction commit failed.");
        }

        res.pushKV("status", "transaction sent");
        res.pushKV("txid", wtx.GetHash().GetHex());
    }
    else
    {
        res.pushKV("status", "trial run - nothing sent");
    }

    res.pushKV("amount", ValueFromAmount(total_amount_2nd_pass));
    res.pushKV("fee", ValueFromAmount(nFeeRequired));
    res.pushKV("recipients", (uint64_t) vecSend.size());
    res.pushKV("suppressed_subcent_recipients", (uint64_t) subcent_suppression_count);

    if (output_details)
    {
        res.pushKV("recipient_details", details);
    }

    return res;
}

UniValue advertisebeacon(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "advertisebeacon ( force )\n"
                "\n"
                "[force] --> If true, generate new beacon keys and send a new "
                "beacon even when an active or pending beacon exists for your "
                "CPID. This is useful if you lose a wallet with your original "
                "beacon keys but not necessary otherwise.\n"
                "\n"
                "Advertise a beacon (Requires wallet to be fully unlocked)\n");

    EnsureWalletIsUnlocked();

    const bool force = params.size() >= 1 ? params[0].get_bool() : false;

    LOCK2(cs_main, pwalletMain->cs_wallet);

    GRC::AdvertiseBeaconResult result = GRC::Researcher::Get()->AdvertiseBeacon(force);

    if (auto public_key_option = result.TryPublicKey()) {
        const GRC::Beacon beacon(std::move(*public_key_option));

        UniValue res(UniValue::VOBJ);

        res.pushKV("result", "SUCCESS");
        res.pushKV("cpid", GRC::Researcher::Get()->Id().ToString());
        res.pushKV("public_key", HexStr(beacon.m_public_key));
        res.pushKV("verification_code", beacon.GetVerificationCode());

        return res;
    }

    switch (result.Error()) {
        case GRC::BeaconError::NONE:
            break; // suppress warning
        case GRC::BeaconError::INSUFFICIENT_FUNDS:
            throw JSONRPCError(
                RPC_WALLET_INSUFFICIENT_FUNDS,
                "Available balance too low to send a beacon transaction");
        case GRC::BeaconError::MISSING_KEY:
            throw JSONRPCError(
                RPC_INVALID_ADDRESS_OR_KEY,
                "Beacon private key missing or invalid");
        case GRC::BeaconError::NO_CPID:
            throw JSONRPCError(
                RPC_INVALID_REQUEST,
                "No CPID detected. Cannot send a beacon in investor mode");
        case GRC::BeaconError::NOT_NEEDED:
            throw JSONRPCError(
                RPC_INVALID_REQUEST,
                "An active beacon already exists for this CPID");
        case GRC::BeaconError::PENDING:
            throw JSONRPCError(
                RPC_INVALID_REQUEST,
                "A beacon advertisement is already pending for this CPID");
        case GRC::BeaconError::TX_FAILED:
            throw JSONRPCError(
                RPC_WALLET_ERROR,
                "Unable to send beacon transaction. See debug.log");
        case GRC::BeaconError::WALLET_LOCKED:
            throw JSONRPCError(
                RPC_WALLET_UNLOCK_NEEDED,
                "Wallet locked. Unlock it fully to send a beacon transaction");
        case GRC::BeaconError::ALEADY_IN_MEMPOOL:
            throw JSONRPCError(
                RPC_INVALID_REQUEST,
                "Beacon transaction for this CPID is already in the mempool");
    }

    throw JSONRPCError(RPC_INTERNAL_ERROR, "Unexpected error occurred");
}

UniValue revokebeacon(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "revokebeacon <cpid>\n"
                "\n"
                "<cpid> CPID associated with the beacon to revoke.\n"
                "\n"
                "Revoke a beacon (Requires wallet to be fully unlocked)\n");

    EnsureWalletIsUnlocked();

    const GRC::CpidOption cpid = GRC::MiningId::Parse(params[0].get_str()).TryCpid();

    if (!cpid) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid CPID.");
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    const GRC::AdvertiseBeaconResult result = GRC::Researcher::Get()->RevokeBeacon(*cpid);

    if (auto public_key_option = result.TryPublicKey()) {
        UniValue res(UniValue::VOBJ);

        res.pushKV("result", "SUCCESS");
        res.pushKV("cpid", cpid->ToString());
        res.pushKV("public_key", HexStr(*public_key_option));

        return res;
    }

    switch (result.Error()) {
        case GRC::BeaconError::NONE:
            break; // suppress warning
        case GRC::BeaconError::INSUFFICIENT_FUNDS:
            throw JSONRPCError(
                RPC_WALLET_INSUFFICIENT_FUNDS,
                "Available balance too low to send a beacon transaction");
        case GRC::BeaconError::MISSING_KEY:
            throw JSONRPCError(
                RPC_INVALID_ADDRESS_OR_KEY,
                "Beacon private key missing or invalid for CPID");
        case GRC::BeaconError::NO_CPID:
            throw JSONRPCError(RPC_INVALID_REQUEST, "No active beacon for CPID");
        case GRC::BeaconError::NOT_NEEDED:
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Unexpected error occurred");
        case GRC::BeaconError::PENDING:
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Unexpected error occurred");
        case GRC::BeaconError::TX_FAILED:
            throw JSONRPCError(
                RPC_WALLET_ERROR,
                "Unable to send beacon transaction. See debug.log");
        case GRC::BeaconError::WALLET_LOCKED:
            throw JSONRPCError(
                RPC_WALLET_UNLOCK_NEEDED,
                "Wallet locked. Unlock it fully to send a beacon transaction");
        case GRC::BeaconError::ALEADY_IN_MEMPOOL:
            throw JSONRPCError(
                RPC_INVALID_REQUEST,
                "Beacon transaction for this CPID is already in the mempool");
    }

    throw JSONRPCError(RPC_INTERNAL_ERROR, "Unexpected error occurred");
}

UniValue beaconreport(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "beaconreport <active only>\n"
                "\n"
                "<active only> Boolean specifying whether only active beacons should be \n"
                "              returned. Defaults to false which also includes expired beacons."
                "\n"
                "Displays list of valid beacons in the network\n");

    bool active_only = false;

    if (params.size() == 1)
    {
        active_only = params[0].getBool();
    }

    UniValue results(UniValue::VARR);

    std::vector<std::pair<GRC::Cpid, GRC::Beacon_ptr>> active_beacon_ptrs;

    int64_t now = GetAdjustedTime();

    // Minimize the lock on cs_main.
    {
        LOCK(cs_main);

        const auto& beacon_map = GRC::GetBeaconRegistry().Beacons();

        active_beacon_ptrs.reserve(beacon_map.size());
        active_beacon_ptrs.assign(beacon_map.begin(), beacon_map.end());
    }

    for (const auto& beacon_pair : active_beacon_ptrs)
    {
        UniValue entry(UniValue::VOBJ);

        if (active_only && beacon_pair.second->Expired(now)) continue;

        entry.pushKV("cpid", beacon_pair.first.ToString());
        entry.pushKV("address", beacon_pair.second->GetAddress().ToString());
        entry.pushKV("timestamp", beacon_pair.second->m_timestamp);
        entry.pushKV("hash", beacon_pair.second->m_hash.GetHex());
        entry.pushKV("prev_beacon_hash", beacon_pair.second->m_previous_hash.GetHex());
        entry.pushKV("status", beacon_pair.second->m_status.Raw());
        entry.pushKV("status_text", beacon_pair.second->StatusToString());

        results.push_back(entry);
    }

    return results;
}

UniValue beaconconvergence(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
                "beaconconvergence\n"
                "\n"
                "Displays verified and pending beacons from the scraper or subscriber viewpoint.\n"
                "\n"
                "There are three output sections:\n"
                "\n"
                "verified_beacons_from_scraper_global:\n"
                "\n"
                "Comes directly from the scraper global map for verified beacons. This is\n"
                "for scraper monitoring of an individual scraper and will be empty if not\n"
                "run on an actual scraper node."
                "\n"
                "verified_beacons_from_latest_convergence:\n"
                "\n"
                "From the latest convergence formed from all of the scrapers. This list\n"
                "is what will be activated in the next superblock.\n"
                "\n"
                "pending_beacons_from_GetConsensusBeaconList:\n"
                "\n"
                "This is a list of pending beacons. Note that it is subject to a one\n"
                "hour ladder, so it will lag the information from the\n"
                "pendingbeaconreport rpc call.\n");

    UniValue results(UniValue::VOBJ);

    UniValue verified_from_global(UniValue::VARR);
    ScraperPendingBeaconMap verified_beacons_from_global = GetVerifiedBeaconsForReport(true);

    for (const auto& verified_beacon_pair : verified_beacons_from_global)
    {
        UniValue entry(UniValue::VOBJ);

        entry.pushKV("cpid", verified_beacon_pair.second.cpid);
        entry.pushKV("verification_code", verified_beacon_pair.first);
        entry.pushKV("timestamp", verified_beacon_pair.second.timestamp);

        verified_from_global.push_back(entry);
    }

    results.pushKV("verified_beacons_from_scraper_global", verified_from_global);

    UniValue verified_from_convergence(UniValue::VARR);
    ScraperPendingBeaconMap verified_beacons_from_convergence = GetVerifiedBeaconsForReport(false);

    for (const auto& verified_beacon_pair : verified_beacons_from_convergence)
    {
        UniValue entry(UniValue::VOBJ);

        entry.pushKV("cpid", verified_beacon_pair.second.cpid);
        entry.pushKV("verification_code", verified_beacon_pair.first);
        entry.pushKV("timestamp", verified_beacon_pair.second.timestamp);

        verified_from_convergence.push_back(entry);
    }

    results.pushKV("verified_beacons_from_latest_convergence", verified_from_convergence);

    UniValue pending(UniValue::VARR);
    ScraperPendingBeaconMap pending_beacons = GetPendingBeaconsForReport();

    for (const auto& beacon_pair : pending_beacons)
    {
        UniValue entry(UniValue::VOBJ);

        entry.pushKV("cpid", beacon_pair.second.cpid);
        entry.pushKV("verification_code", beacon_pair.first);
        entry.pushKV("timestamp", beacon_pair.second.timestamp);

        pending.push_back(entry);
    }

    results.pushKV("pending_beacons_from_GetConsensusBeaconList", pending);

    return results;
}

UniValue pendingbeaconreport(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
                "pendingbeaconreport\n"
                "\n"
                "Displays pending beacons directly from the beacon registry.\n");

    UniValue results(UniValue::VARR);

    std::vector<std::pair<CKeyID, GRC::Beacon_ptr>> pending_beacons;

    // Minimize the lock on cs_main.
    {
        LOCK(cs_main);

        const auto& pending_beacon_map = GRC::GetBeaconRegistry().PendingBeacons();

        pending_beacons.reserve(pending_beacon_map.size());
        pending_beacons.assign(pending_beacon_map.begin(), pending_beacon_map.end());
    }

    for (const auto& pending_beacon_pair : pending_beacons)
    {
        UniValue entry(UniValue::VOBJ);

        CBitcoinAddress address;
        const CKeyID& key_id = pending_beacon_pair.first;

        address.Set(key_id);

        entry.pushKV("cpid", pending_beacon_pair.second->m_cpid.ToString());
        entry.pushKV("key_id", pending_beacon_pair.first.ToString());
        entry.pushKV("address", address.ToString());
        entry.pushKV("timestamp", pending_beacon_pair.second->m_timestamp);

        results.push_back(entry);
    }

    return results;
}

UniValue beaconstatus(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "beaconstatus [cpid]\n"
                "\n"
                "[cpid] -> Optional parameter of cpid\n"
                "\n"
                "Displays status of your beacon or specified beacon on the network\n");

    const GRC::MiningId mining_id = params.size() > 0
        ? GRC::MiningId::Parse(params[0].get_str())
        : GRC::Researcher::Get()->Id();

    if (!mining_id.Valid()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid CPID.");
    }

    const GRC::CpidOption cpid = mining_id.TryCpid();

    if (!cpid) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "No beacon for investor.");
    }

    const int64_t now = GetAdjustedTime();
    const bool is_mine = GRC::Researcher::Get()->Id() == *cpid;

    UniValue res(UniValue::VOBJ);
    UniValue active(UniValue::VARR);
    UniValue pending(UniValue::VARR);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    const GRC::BeaconRegistry& beacons = GRC::GetBeaconRegistry();

    if (const GRC::BeaconOption beacon = beacons.Try(*cpid)) {
        UniValue entry(UniValue::VOBJ);
        entry.pushKV("cpid", cpid->ToString());
        entry.pushKV("active", !beacon->Expired(now));
        entry.pushKV("pending", false);
        entry.pushKV("expired", beacon->Expired(now));
        entry.pushKV("renewable", beacon->Renewable(now));
        entry.pushKV("timestamp", TimestampToHRDate(beacon->m_timestamp));
        entry.pushKV("address", beacon->GetAddress().ToString());
        entry.pushKV("public_key", HexStr(beacon->m_public_key));
        entry.pushKV("private_key_available", beacon->WalletHasPrivateKey(pwalletMain));
        entry.pushKV("magnitude", GRC::Quorum::GetMagnitude(*cpid).Floating());
        entry.pushKV("verification_code", beacon->GetVerificationCode());
        entry.pushKV("is_mine", is_mine);

        active.push_back(entry);
    }

    for (const auto& beacon_ptr : beacons.FindPending(*cpid)) {
        UniValue entry(UniValue::VOBJ);
        entry.pushKV("cpid", cpid->ToString());
        entry.pushKV("active", false);
        entry.pushKV("pending", true);
        entry.pushKV("expired", beacon_ptr->Expired(now));
        entry.pushKV("renewable", false);
        entry.pushKV("timestamp", TimestampToHRDate(beacon_ptr->m_timestamp));
        entry.pushKV("address", beacon_ptr->GetAddress().ToString());
        entry.pushKV("public_key", HexStr(beacon_ptr->m_public_key));
        entry.pushKV("private_key_available", beacon_ptr->WalletHasPrivateKey(pwalletMain));
        entry.pushKV("magnitude", 0);
        entry.pushKV("verification_code", beacon_ptr->GetVerificationCode());
        entry.pushKV("is_mine", is_mine);

        pending.push_back(entry);
    }

    res.pushKV("active", active);
    res.pushKV("pending", pending);

    return res;
}

UniValue beaconaudit(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "beaconaudit [errors only] [cpid]\n"
            "\n"
            "[errors only] -> Boolean to provide errors only. Defaults to true.\n"
            "[cpid] -> Optional parameter of cpid. Defaults to current cpid. * means all active CPIDs.\n"
            "\n"
            "Conducts consistency audit for beacon contracts and beacon chain for given CPID.\n"
            "This is currently limited to looking at multiple renewals for the same CPID in\n"
            "the same block and reporting inconsistencies between the normal contract order\n"
            "and the historical beacon entries (beacon chainlet) for the CPID.\n");

    bool errors_only = true;
    bool global = false;

    GRC::MiningId mining_id;

    if (params.size() > 0) {
        errors_only = params[0].get_bool();
    }

    if (params.size() > 1) {
        if (params[1].get_str() == "*") {
            global = true;
        } else {
            mining_id = GRC::MiningId::Parse(params[1].get_str());
        }
    } else {
        mining_id = GRC::Researcher::Get()->Id();
    }

    if (!global && !mining_id.Valid()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid CPID.");
    }

    const GRC::CpidOption cpid = mining_id.TryCpid();

    if (!global && !cpid) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "No beacon for investor.");
    }

    // Only allow auditing when at or above block V11 threshold.
    if (!IsV11Enabled(pindexBest->nHeight)) {
        throw JSONRPCError(RPC_INVALID_REQUEST, "This function cannot be called when the wallet height is below the block V11"
                                                "threshold.");
    }

    // Find Fern starting block.
    CBlockIndex* block_index = GRC::BlockFinder::FindByHeight(Params().GetConsensus().BlockV11Height);

    std::set<GRC::Cpid> cpids;

    typedef std::tuple<GRC::Contract, CTransaction, unsigned int, CBlockIndex*> BeaconContext;

    std::multimap<GRC::Cpid, BeaconContext> beacon_contracts;

    // This form of block index traversal starts at the first V11 block and continues to pIndexBest (inclusive).
    while (block_index) {
        CBlock block;

        if (!ReadBlockFromDisk(block, block_index, Params().GetConsensus())) {
            throw JSONRPCError(RPC_DATABASE_ERROR, "Unable to read block from disk. Your blockchain files are corrupted.");
        }

        std::set<GRC::Cpid> cpids_in_block;
        std::multimap<GRC::Cpid, BeaconContext> beacon_contracts_in_block;

        for (unsigned int i = 0; i < block.vtx.size(); ++i) {

            for (const auto& tx_contract: block.vtx[i].GetContracts()) {
                if (tx_contract.m_type != GRC::ContractType::BEACON) continue;

                GRC::BeaconPayload beacon_payload = tx_contract.CopyPayloadAs<GRC::BeaconPayload>();

                // If not global (all cpids) and payload cpid does not match input parameter (specified cpid), continue.
                if (!global && beacon_payload.m_cpid != *cpid) continue;

                cpids_in_block.insert(beacon_payload.m_cpid);

                BeaconContext beacon_context {tx_contract, block.vtx[i], i, block_index};

                beacon_contracts_in_block.insert({ beacon_payload.m_cpid, beacon_context });
            }
        }

        for (const auto& cpid_in_block : cpids_in_block) {
            if (beacon_contracts_in_block.count(cpid_in_block) > 1) {
                cpids.insert(cpid_in_block);

                auto beacons_to_insert = beacon_contracts_in_block.equal_range(cpid_in_block);

                for (auto iter = beacons_to_insert.first; iter != beacons_to_insert.second; ++iter) {
                    beacon_contracts.insert({ iter->first, iter->second });
                }
            }
        }

        // If we are at pIndexBest (i.e. no pnext), then break.
        if (block_index->pnext) {
            block_index = block_index->pnext;
        } else {
            break;
        }
    }

    LogPrintf("INFO: %s: number of cpids = %u, number of beacon contracts = %u",
              __func__,
              cpids.size(),
              beacon_contracts.size());

    UniValue res(UniValue::VOBJ);
    UniValue beacons_to_output(UniValue::VARR);

    GRC::BeaconRegistry& beacon_registry = GRC::GetBeaconRegistry();

    for (const auto& cpid_to_output : cpids) {
        UniValue beacon_contracts_output(UniValue::VARR);
        auto beacon_contracts_to_output = beacon_contracts.equal_range(cpid_to_output);

        uint256 prev_block_hash, prev_renewal_hash, prev_renewal_hash_report;

        for (auto beacon_contract_to_output = beacon_contracts_to_output.first;
             beacon_contract_to_output != beacon_contracts_to_output.second;
             ++beacon_contract_to_output) {
            bool prev_hash_mismatch_error = false;
            bool no_historical_entry_error = false;

            size_t i = std::distance(beacon_contracts_to_output.first, beacon_contract_to_output);

            UniValue beacon_contract_output(UniValue::VOBJ);

            uint256 beacon_hash = get<1>(beacon_contract_to_output->second).GetHash();

            GRC::Contract::Action action = get<0>(beacon_contract_to_output->second).m_action;

            const GRC::BeaconPayload& beacon_payload = get<0>(beacon_contract_to_output->second).CopyPayloadAs<GRC::BeaconPayload>();

            GRC::BeaconOption historical_beacon_entry =
                beacon_registry.FindHistorical(get<1>(beacon_contract_to_output->second).GetHash());

            if (historical_beacon_entry) {
                if (action == GRC::ContractAction::ADD && historical_beacon_entry->m_status == GRC::BeaconStatusForStorage::RENEWAL) {
                    if (i && prev_block_hash == get<3>(beacon_contract_to_output->second)->GetBlockHash()
                        && prev_renewal_hash != historical_beacon_entry->m_previous_hash) {
                        prev_hash_mismatch_error = true;
                    }

                    prev_renewal_hash_report = prev_renewal_hash;
                    prev_renewal_hash = beacon_hash;
                    prev_block_hash = get<3>(beacon_contract_to_output->second)->GetBlockHash();
                }
            } else {
                no_historical_entry_error = true;
            }

            if (!errors_only || prev_hash_mismatch_error || no_historical_entry_error) {
                beacon_contract_output.pushKV("height", get<3>(beacon_contract_to_output->second)->nHeight);
                beacon_contract_output.pushKV("vtx_index", (uint64_t) get<2>(beacon_contract_to_output->second));
                beacon_contract_output.pushKV("txid", beacon_hash.ToString());
                beacon_contract_output.pushKV("tx_time", (int64_t) get<1>(beacon_contract_to_output->second).nTime);
                beacon_contract_output.pushKV("tx_time_string", FormatISO8601DateTime(get<1>(beacon_contract_to_output->second).nTime));
                beacon_contract_output.pushKV("action", action.ToString());

                beacon_contract_output.pushKV("same_block_renewal_prev_hash_mismatch", prev_hash_mismatch_error);

                if (prev_hash_mismatch_error) {
                    beacon_contract_output.pushKV("previous_renewal_hash_via_contract_traversal",
                                                  prev_renewal_hash_report.ToString());
                    beacon_contract_output.pushKV("previous_renewal_hash_by_historical_beacon_entry",
                                                  historical_beacon_entry->m_previous_hash.ToString());
                }

                if (!no_historical_entry_error) {
                    beacon_contract_output.pushKV("status", historical_beacon_entry->StatusToString());
                } else {
                    beacon_contract_output.pushKV("status", "no historical entry");
                }

                beacon_contracts_output.push_back(beacon_contract_output);
            }
        }

        UniValue beacon(UniValue::VOBJ);

        if (!beacon_contracts_output.empty()) {
            beacon.pushKV("cpid", cpid_to_output.ToString());
            beacon.pushKV("contracts", beacon_contracts_output);
            beacons_to_output.push_back(beacon);
        }
    }

    res.pushKV("cpids_with_more_than_one_beacon_contract_in_block", beacons_to_output);

    return res;
}

UniValue explainmagnitude(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "explainmagnitude ( cpid )\n"
                "\n"
                "[cpid] -> Optional CPID to explain magnitude for\n"
                "\n"
                "Itemize your CPID magnitudes by project.\n");

    const GRC::MiningId mining_id = params.size() > 0
        ? GRC::MiningId::Parse(params[0].get_str())
        : GRC::Researcher::Get()->Id();

    if (!mining_id.Valid()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid CPID.");
    }

    const GRC::CpidOption cpid = mining_id.TryCpid();

    if (!cpid) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "No data for investor.");
    }

    UniValue res(UniValue::VARR);
    double total_rac = 0;
    double total_magnitude = 0;

    for (const auto& project : GRC::Quorum::ExplainMagnitude(*cpid)) {
        total_rac += project.m_rac;
        total_magnitude += project.m_magnitude;

        UniValue entry(UniValue::VOBJ);

        entry.pushKV("project", project.m_name);
        entry.pushKV("rac", project.m_rac);
        entry.pushKV("magnitude", project.m_magnitude);

        res.push_back(entry);
    }

    UniValue total(UniValue::VOBJ);

    total.pushKV("project", "total");
    total.pushKV("rac", total_rac);
    total.pushKV("magnitude", total_magnitude);

    res.push_back(total);

    return res;
}

UniValue lifetime(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "lifetime [cpid]\n"
                "\n"
                "Displays research rewards for the lifetime of a CPID.\n");

    const GRC::MiningId mining_id = params.size() > 0
        ? GRC::MiningId::Parse(params[0].get_str())
        : GRC::Researcher::Get()->Id();

    if (!mining_id.Valid()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid CPID.");
    }

    const GRC::CpidOption cpid = mining_id.TryCpid();

    if (!cpid) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "No data for investor.");
    }

    UniValue results(UniValue::VOBJ);

    LOCK(cs_main);

    for (const CBlockIndex* pindex = pindexGenesisBlock;
        pindex;
        pindex = pindex->pnext)
    {
        if (pindex->ResearchSubsidy() > 0 && pindex->GetMiningId() == *cpid) {
            results.pushKV(
                ToString(pindex->nHeight),
                ValueFromAmount(pindex->ResearchSubsidy()));
        }
    }

    return results;
}

UniValue magnitude(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "magnitude <cpid>\n"
                "\n"
                "<cpid> -> cpid to look up\n"
                "\n"
                "Displays information for the magnitude of all cpids or specified in the network\n");

    const GRC::MiningId mining_id = params.size() > 0
        ? GRC::MiningId::Parse(params[0].get_str())
        : GRC::Researcher::Get()->Id();

    if (!mining_id.Valid()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid CPID.");
    }

    if (const GRC::CpidOption cpid = mining_id.TryCpid()) {
        LOCK(cs_main);

        return MagnitudeReport(*cpid);
    }

    throw JSONRPCError(RPC_INVALID_PARAMETER, "No data for investor.");
}

UniValue resetcpids(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "resetcpids\n"
                "\n"
                "Reloads cpids\n");

    UniValue res(UniValue::VOBJ);

    LOCK(cs_main);

    std::string error_msg;

    if (!gArgs.ReadConfigFiles(error_msg, true))
    {
        throw JSONRPCError(RPC_MISC_ERROR, error_msg);
    }

    GRC::Researcher::Reload();

    res.pushKV("Reset", 1);

    return res;
}

UniValue superblockage(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "superblockage\n"
                "\n"
                "Display information regarding superblock age\n");

    UniValue res(UniValue::VOBJ);

    const GRC::SuperblockPtr superblock = GRC::Quorum::CurrentSuperblock();

    res.pushKV("Superblock Age", superblock.Age(GetAdjustedTime()));
    res.pushKV("Superblock Timestamp", TimestampToHRDate(superblock.m_timestamp));
    res.pushKV("Superblock Block Number", superblock.m_height);
    res.pushKV("Pending Superblock Height", GRC::Quorum::PendingSuperblock().m_height);

    return res;
}

UniValue superblocks(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error(
                "superblocks [lookback [displaycontract [cpid]]]\n"
                "\n"
                "[lookback] -> Optional: # of SB's to show (default 14)\n"
                "[displaycontract] -> Optional true/false: display SB contract (default false)\n"
                "[cpid] -> Optional: Shows magnitude for a cpid for recent superblocks\n"
                "\n"
                "Display data on recent superblocks\n");

    UniValue res(UniValue::VARR);

    int lookback = 14;
    bool displaycontract = false;
    std::string cpid = "";

    if (params.size() > 0)
    {
        lookback = params[0].get_int();
        LogPrint(BCLog::LogFlags::VERBOSE, "INFO: superblocks: lookback %i", lookback);
    }

    if (params.size() > 1)
    {
        displaycontract = params[1].get_bool();
        LogPrint(BCLog::LogFlags::VERBOSE, "INFO: superblocks: display contract %i", displaycontract);
    }

    if (params.size() > 2)
    {
        cpid = params[2].get_str();
        LogPrint(BCLog::LogFlags::VERBOSE, "INFO: superblocks: CPID %s", cpid);
    }

    LOCK(cs_main);

    res = SuperblockReport(lookback, displaycontract, cpid);

    return res;
}

//!
//! \brief Send a transaction that contains an administrative contract.
//!
//! Before invoking this command, import the master key used to sign and verify
//! transactions that contain administrative contracts. The label is optional:
//!
//!     importprivkey <private_key_hex> master
//!
//! Send some coins to the master key address if necessary:
//!
//!     sendtoaddress <address> <amount>
//!
//! To whitelist a project:
//!
//!     addkey add project projectname url
//!
//! To de-whitelist a project:
//!
//!     addkey delete project projectname
//!
//!     or
//!
//!     addkey delete project projectname 1
//!
//! Key examples:
//!
//!     v1: addkey add project milkyway@home http://milkyway.cs.rpi.edu/milkyway/@
//!     v2: addkey add project milkyway@home http://milkyway.cs.rpi.edu/milkyway/@ true
//!     v1 or v2: addkey delete project milkyway@home
//!     v1 or v2: addkey delete project milkyway@home 1 (last parameter is a dummy)
//!     v2: addkey delete project milkyway@home 1 false (last two parameters are dummies)
//!
//! GRC will only memorize the *last* value it finds for a key in the highest
//! block.
//!
UniValue addkey(const UniValue& params, bool fHelp)
{
    bool project_v2_enabled = false;
    bool block_v13_enabled = false;
    uint32_t contract_version = 0;

    {
        LOCK(cs_main);

        project_v2_enabled = IsProjectV2Enabled(nBestHeight);

        block_v13_enabled = IsV13Enabled(nBestHeight);
        contract_version = block_v13_enabled ? 3 : 2;
    }

    GRC::ContractAction action = GRC::ContractAction::UNKNOWN;
    GRC::Contract::Type type = GRC::ContractType::UNKNOWN;
    size_t required_param_count = 4;
    size_t param_count_max = 4;

    if (params.size()) {
        if (params[0].get_str() == "add") {
            action = GRC::ContractAction::ADD;
        } else if (params[0].get_str() == "delete") {
            action = GRC::ContractAction::REMOVE;
        }
    }

    if (params.size() >= 2) {
        type = GRC::Contract::Type::Parse(params[1].get_str());
    }

    // We only need to specify five parameters if v2 project contracts are enabled AND the action is add AND
    // the key type is project.
    if (project_v2_enabled
            && params.size()
            && action == GRC::ContractAction::ADD
            && type == GRC::ContractType::PROJECT) {
        required_param_count = 5;
        param_count_max = 5;
    }

    if ((type == GRC::ContractType::PROJECT || type == GRC::ContractType::SCRAPER)
            && action == GRC::ContractAction::REMOVE) {
        required_param_count = 3;

        // This is for compatibility with scripts for project and scraper administration that may put something in the
        // fourth parameter because it was originally required even though ignored. The same principal applies
        // to v2, where the last two parameters for a remove can be supplied, but they will be ignored.
        if (project_v2_enabled) {
            param_count_max = 5;
        }
    }

    // For add a mandatory sidestake, the 4th parameter is the allocation and the description (5th parameter) is optional.
    if (type == GRC::ContractType::SIDESTAKE) {
        if (action == GRC::ContractAction::ADD) {
            required_param_count = 4;
            param_count_max = 5;
        } else {
            required_param_count = 3;
            param_count_max = 3;
        }
    }

    if (fHelp || params.size() < required_param_count || params.size() > param_count_max) {
        std::string error_string;

        if (project_v2_enabled) {
            error_string = "addkey <action> <keytype> <keyname> <keyvalue> <gdpr_protection_bool>\n"
                           "\n"
                           "<action> ---> Specify add or delete of key\n"
                           "<keytype> --> Specify keytype ex: project\n"
                           "<keyname> --> Specify keyname ex: milky\n"
                           "<keyvalue> -> Specify keyvalue ex: 1\n"
                           "\n"
                           "For project keytype only\n"
                           "<gdpr_protection_bool> -> true if GDPR stats export protection is enforced for project\n"
                           "\n"
                           "Add a key to the network";
        } else {
            error_string = "addkey <action> <keytype> <keyname> <keyvalue>\n"
                           "\n"
                           "<action> ---> Specify add or delete of key\n"
                           "<keytype> --> Specify keytype ex: project\n"
                           "<keyname> --> Specify keyname ex: milky\n"
                           "<keyvalue> -> Specify keyvalue ex: 1\n"
                           "\n"
                           "Add a key to the network";
        }

        throw runtime_error(error_string);
    }

    if (pwalletMain->IsLocked()) {
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED,
                           "Error: Please enter the wallet passphrase with walletpassphrase first.");
    }

    if (!(type == GRC::ContractType::PROJECT
          || type == GRC::ContractType::SCRAPER
          || type == GRC::ContractType::PROTOCOL
          || type == GRC::ContractType::SIDESTAKE)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid contract type for addkey.");
    }

    if (action == GRC::ContractAction::UNKNOWN) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Action must be 'add' or 'delete'.");
    }

    GRC::Contract contract;

    switch (type.Value()) {
    case GRC::ContractType::PROJECT:
    {
        if (action == GRC::ContractAction::ADD) {
            bool gdpr_export_control = false;

            if (block_v13_enabled) {
                // We must do our own conversion to boolean here, because the 5th parameter can either be
                // a boolean for project or a string for sidestake, which means the client.cpp entry cannot contain a
                // unicode type specifier for the 5th parameter.
                if (ToLower(params[4].get_str()) == "true") {
                    gdpr_export_control = true;
                } else if (ToLower(params[4].get_str()) != "false") {
                    // Neither true or false - throw an exception.
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "GDPR export parameter invalid. Must be true or false.");
                }

                contract = GRC::MakeContract<GRC::Project>(
                            contract_version,
                            action,
                            uint32_t{3},          // Contract payload version number, 3
                            params[2].get_str(),  // Name
                            params[3].get_str(),  // URL
                            gdpr_export_control); // GDPR stats export protection enforced boolean

            } else if (project_v2_enabled) {
                // We must do our own conversion to boolean here, because the 5th parameter can either be
                // a boolean for project or a string for sidestake, which means the client.cpp entry cannot contain a
                // unicode type specifier for the 5th parameter.
                if (ToLower(params[4].get_str()) == "true") {
                    gdpr_export_control = true;
                } else if (ToLower(params[4].get_str()) != "false") {
                    // Neither true or false - throw an exception.
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "GDPR export parameter invalid. Must be true or false.");
                }

                contract = GRC::MakeContract<GRC::Project>(
                            contract_version,
                            action,
                            uint32_t{2},          // Contract payload version number, 2
                            params[2].get_str(),  // Name
                            params[3].get_str(),  // URL
                            gdpr_export_control); // GDPR stats export protection enforced boolean

            } else {
                contract = GRC::MakeContract<GRC::Project>(
                            contract_version,
                            action,
                            params[2].get_str(),  // Name
                            params[3].get_str()); // URL
            }
        } else if (action == GRC::ContractAction::REMOVE) {
            if (block_v13_enabled) {
                contract = GRC::MakeContract<GRC::Project>(
                            contract_version,
                            action,
                            uint32_t{3},          // Contract payload version number, 3
                            params[2].get_str(),  // Name
                            std::string{},        // URL ignored
                            false);               // GDPR status irrelevant

            } else if (project_v2_enabled) {
                contract = GRC::MakeContract<GRC::Project>(
                            contract_version,
                            action,
                            uint32_t{2},          // Contract payload version number, 2
                            params[2].get_str(),  // Name
                            std::string{},        // URL ignored
                            false);               // GDPR status irrelevant

            } else {
                contract = GRC::MakeContract<GRC::Project>(
                            contract_version,
                            action,
                            params[2].get_str(),  // Name
                            std::string{});       // URL ignored
           }
        }
        break;
    }
    case GRC::ContractType::SCRAPER:
    {
        std::string status_string = ToLower(params[3].get_str());
        GRC::ScraperEntryStatus status = GRC::ScraperEntryStatus::UNKNOWN;

        CBitcoinAddress scraper_address;
        if (!scraper_address.SetString(params[2].get_str())) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Address specified for the scraper is invalid.");
        }

        if (block_v13_enabled) { // Contract version will be 3.
            CKeyID key_id;

            scraper_address.GetKeyID(key_id);

            if (action == GRC::ContractAction::ADD) {
                if (status_string == "false") {
                    status = GRC::ScraperEntryStatus::NOT_AUTHORIZED;
                } else if (status_string == "true") {
                    status = GRC::ScraperEntryStatus::AUTHORIZED;
                } else if (status_string == "explorer") {
                    status = GRC::ScraperEntryStatus::EXPLORER;
                } else {
                    JSONRPCError(RPC_INVALID_PARAMETER, "Status specified for the scraper is invalid.");
                }
            } else if (action == GRC::ContractAction::REMOVE) {
                status = GRC::ScraperEntryStatus::DELETED;
            }

            contract = GRC::MakeContract<GRC::ScraperEntryPayload>(
                        contract_version,
                        action,
                        uint32_t {2}, // Contract payload version number
                        key_id,
                        status);

        } else { // Block v13 not enabled. (Contract version will be 2.)
            if (action == GRC::ContractAction::ADD && !(status_string == "false" || status_string == "true")) {
                JSONRPCError(RPC_INVALID_PARAMETER, "Status specified for the scraper is invalid.");
            } else if (action == GRC::ContractAction::REMOVE) {
                status_string = "false";
            }

            // This form of ScraperEntryPayload generation matches the payload constructor that uses the Parse
            // function to convert legacy arguments into a native scraper entry.
            contract = GRC::MakeContract<GRC::ScraperEntryPayload>(
                        contract_version,
                        action,
                        scraper_address.ToString(),
                        status_string);
        }
        break;
    }
    case GRC::ContractType::PROTOCOL:
        // There will be no legacy payload contracts past version 2. This will need to be changed before the
        // block v13 mandatory (which also means contract v3).
        contract = GRC::MakeLegacyContract(
                    type.Value(),
                    action,
                    params[2].get_str(),   // key
                    params[3].get_str());  // value
        break;
    case GRC::ContractType::SIDESTAKE:
    {
        if (block_v13_enabled) {
            CBitcoinAddress sidestake_address;
            if (!sidestake_address.SetString(params[2].get_str())) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Address specified for the sidestake is invalid.");
            }

            std::string description;
            if (params.size() > 4) {
                description = params[4].get_str();
            }

            // We have to do our own conversion here because the 4th parameter type specifier cannot be set other
            // than string in the client.cpp file.
            double allocation = 0.0;
            if (params.size() > 3 && !ParseDouble(params[3].get_str(), &allocation)) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid allocation specified.");
            }

            allocation /= 100.0;

            if (allocation > 1.0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Allocation specified is greater than 100.0%.");
            }

            contract = GRC::MakeContract<GRC::SideStakePayload>(
                contract_version,                                            // Contract version number (3+)
                action,                                                      // Contract action
                uint32_t {1},                                                // Contract payload version number
                sidestake_address.Get(),                                     // Sidestake destination
                allocation,                                                  // Sidestake allocation
                description,                                                 // Sidestake description
                GRC::MandatorySideStake::MandatorySideStakeStatus::MANDATORY // sidestake status
                );
        } else {
             throw JSONRPCError(RPC_INVALID_PARAMETER, "Sidestake contracts are not valid for block version less than v13.");
        }

        break;
    }
    case GRC::ContractType::BEACON:
        [[fallthrough]];
    case GRC::ContractType::CLAIM:
        [[fallthrough]];
    case GRC::ContractType::MESSAGE:
        [[fallthrough]];
    case GRC::ContractType::MRC:
        [[fallthrough]];
    case GRC::ContractType::POLL:
        [[fallthrough]];
    case GRC::ContractType::VOTE:
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid contract type for addkey.");
    case GRC::ContractType::UNKNOWN:
        [[fallthrough]];
    case GRC::ContractType::OUT_OF_BOUND:
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid contract type.");
    }

    if (!contract.RequiresMasterKey()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Not an admin contract type.");
    }

    std::pair<CWalletTx, std::string> result = GRC::SendContract(contract);
    std::string error = result.second;

    if (!error.empty()) {
        throw JSONRPCError(RPC_WALLET_ERROR, std::move(error));
    }

    UniValue res(UniValue::VOBJ);

    res.pushKV("contract", ContractToJson(contract));
    res.pushKV("txid", result.first.GetHash().ToString());

    return res;
}

UniValue currentcontractaverage(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "currentcontractaverage\n"
                "\n"
                "Displays information on your current contract average with regards to superblock contract\n");

    UniValue res(UniValue::VOBJ);

    const GRC::Superblock superblock = GRC::Quorum::CreateSuperblock();

    res.pushKV("contract", SuperblockToJson(superblock));
    res.pushKV("beacon_count", (uint64_t)superblock.m_cpids.TotalCount());
    res.pushKV("avg_mag", superblock.m_cpids.AverageMagnitude());
    res.pushKV("beacon_participant_count", (uint64_t)superblock.m_cpids.size());
    res.pushKV("superblock_valid", superblock.WellFormed());
    res.pushKV("quorum_hash", superblock.GetHash().ToString());
    res.pushKV("size", (uint64_t)GetSerializeSize(superblock, SER_NETWORK, PROTOCOL_VERSION));

    return res;
}

UniValue debug(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "debug <bool>\n"
                "\n"
                "<bool> -> Specify true or false\n"
                "\n"
                "Enable or disable VERBOSE logging category (aka old debug) on the fly\n"
                "This is deprecated by the \"logging verbose\" command.\n");

    UniValue res(UniValue::VOBJ);

    if(params[0].get_bool())
    {
        LogInstance().EnableCategory(BCLog::LogFlags::VERBOSE);
    }
    else
    {
        LogInstance().DisableCategory(BCLog::LogFlags::VERBOSE);
    }

    res.pushKV("Logging category VERBOSE (aka old debug) ", LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE) ? "Enabled." : "Disabled.");

    return res;
}

UniValue listprojects(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "listprojects\n"
                "\n"
                "Displays information about whitelisted projects.\n");

    UniValue res(UniValue::VOBJ);

    for (const auto& project : GRC::GetWhitelist().Snapshot().Sorted()) {
        UniValue entry(UniValue::VOBJ);

        entry.pushKV("version", (int)project.m_version);
        entry.pushKV("display_name", project.DisplayName());
        entry.pushKV("url", project.m_url);
        entry.pushKV("base_url", project.BaseUrl());
        entry.pushKV("display_url", project.DisplayUrl());
        entry.pushKV("stats_url", project.StatsUrl());

        if (project.HasGDPRControls()) {
            entry.pushKV("gdpr_controls", *project.HasGDPRControls());
        }

        entry.pushKV("time", DateTimeStrFormat(project.m_timestamp));

        res.pushKV(project.m_name, entry);
    }

    return res;
}

UniValue listscrapers(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "listscrapers\n"
                "\n"
                "Displays information about scrapers recognized by the network.\n");

    UniValue res(UniValue::VOBJ);
    UniValue scraper_entries(UniValue::VARR);

    for (const auto& scraper : GRC::GetScraperRegistry().Scrapers()) {
        UniValue entry(UniValue::VOBJ);

        CBitcoinAddress address(scraper.first);

        entry.pushKV("scraper_address", address.ToString());
        entry.pushKV("current_scraper_entry_tx_hash", scraper.second->m_hash.ToString());
        if (scraper.second->m_previous_hash.IsNull()) {
            entry.pushKV("previous_scraper_entry_tx_hash", "null");
        } else {
            entry.pushKV("previous_scraper_entry_tx_hash", scraper.second->m_previous_hash.ToString());
        }

        entry.pushKV("scraper_entry_timestamp", scraper.second->m_timestamp);
        entry.pushKV("scraper_entry_time", DateTimeStrFormat(scraper.second->m_timestamp));
        entry.pushKV("scraper_entry_status", scraper.second->StatusToString());

        scraper_entries.push_back(entry);
    }

    res.pushKV("current_scraper_entries", scraper_entries);

    return res;
}

UniValue listprotocolentries(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "listprotocolentries\n"
                "\n"
                "Displays the protocol entries on the network.\n");

    UniValue res(UniValue::VOBJ);
    UniValue scraper_entries(UniValue::VARR);

    for (const auto& protocol : GRC::GetProtocolRegistry().ProtocolEntries()) {
        UniValue entry(UniValue::VOBJ);

        entry.pushKV("protocol_entry_key", protocol.first);
        entry.pushKV("protocol_entry_value", protocol.second->m_value);
        entry.pushKV("current_protocol_entry_tx_hash", protocol.second->m_hash.ToString());
        if (protocol.second->m_previous_hash.IsNull()) {
            entry.pushKV("previous_protocol_entry_tx_hash", "null");
        } else {
            entry.pushKV("previous_protocol_entry_tx_hash", protocol.second->m_previous_hash.ToString());
        }

        entry.pushKV("protocol_entry_timestamp", protocol.second->m_timestamp);
        entry.pushKV("protocol_entry_time", DateTimeStrFormat(protocol.second->m_timestamp));
        entry.pushKV("protocol_entry_status", protocol.second->StatusToString());

        scraper_entries.push_back(entry);
    }

    res.pushKV("current_protocol_entries", scraper_entries);

    return res;
}

UniValue listmandatorysidestakes(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "listprotocolentries\n"
            "\n"
            "Displays the mandatory sidestakes on the network.\n");

    UniValue res(UniValue::VOBJ);
    UniValue scraper_entries(UniValue::VARR);

    for (const auto& sidestake : GRC::GetSideStakeRegistry().ActiveSideStakeEntries(GRC::SideStake::FilterFlag::MANDATORY, false)) {
        UniValue entry(UniValue::VOBJ);

        entry.pushKV("mandatory_sidestake_entry_address", CBitcoinAddress(sidestake->GetDestination()).ToString());
        entry.pushKV("mandatory_sidestake_entry_allocation", sidestake->GetAllocation().ToPercent());
        entry.pushKV("mandatory_sidestake_entry_tx_hash", sidestake->GetHash().ToString());
        if (sidestake->GetPreviousHash().IsNull()) {
            entry.pushKV("previous_mandatory_sidestake_entry_tx_hash", "null");
        } else {
            entry.pushKV("previous_mandatory_sidestake_entry_tx_hash", sidestake->GetPreviousHash().ToString());
        }

        entry.pushKV("mandatory_sidestake_entry_timestamp", sidestake->GetTimeStamp());
        entry.pushKV("mandatory_sidestake_entry_time", DateTimeStrFormat(sidestake->GetTimeStamp()));
        entry.pushKV("mandatory_sidestake_entry_status", sidestake->StatusToString());

        scraper_entries.push_back(entry);
    }

    res.pushKV("current_mandatory_sidestake_entries", scraper_entries);

    return res;
}

UniValue network(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "network\n"
                "\n"
                "Display information about the network health\n");

    UniValue res(UniValue::VOBJ);

    LOCK(cs_main);

    const int64_t now = pindexBest->nTime;
    const int64_t two_weeks_ago = now - (60 * 60 * 24 * 14);
    const double money_supply = pindexBest->nMoneySupply;
    const GRC::SuperblockPtr superblock = GRC::Quorum::CurrentSuperblock();

    int64_t two_week_research_subsidy = 0;

    for (const CBlockIndex* pindex = pindexBest;
        pindex && pindex->nTime > two_weeks_ago;
        pindex = pindex->pprev)
    {
        two_week_research_subsidy += pindex->ResearchSubsidy();
    }

    res.pushKV("total_magnitude", superblock->m_cpids.TotalMagnitude());
    res.pushKV("average_magnitude", superblock->m_cpids.AverageMagnitude());
    res.pushKV("magnitude_unit", GRC::Tally::GetMagnitudeUnit(pindexBest));
    res.pushKV("research_paid_two_weeks", ValueFromAmount(two_week_research_subsidy));
    res.pushKV("research_paid_daily_average", ValueFromAmount(two_week_research_subsidy / 14));
    res.pushKV("research_paid_daily_limit", ValueFromAmount(GRC::Tally::MaxEmission(now)));
    res.pushKV("total_money_supply", ValueFromAmount(money_supply));

    return res;
}

UniValue parselegacysb(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1)
        throw runtime_error(
                "parselegacysb\n"
                "\n"
                "Convert a legacy superblock contract to JSON.\n");

    UniValue json(UniValue::VOBJ);

    GRC::Superblock superblock = GRC::Superblock::UnpackLegacy(params[0].get_str());

    json.pushKV("contract", SuperblockToJson(superblock));
    json.pushKV("legacy_hash", superblock.GetHash().ToString());

    return json;
}

UniValue projects(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "projects\n"
                "\n"
                "Show the status of locally attached BOINC projects.\n");

    UniValue res(UniValue::VARR);
    const GRC::ResearcherPtr researcher = GRC::Researcher::Get();
    const GRC::WhitelistSnapshot whitelist = GRC::GetWhitelist().Snapshot();

    for (const auto& project_pair : researcher->Projects()) {
        const GRC::MiningProject& project = project_pair.second;

        UniValue entry(UniValue::VOBJ);

        entry.pushKV("name", project.m_name);
        entry.pushKV("url", project.m_url);

        entry.pushKV("cpid", project.m_cpid.ToString());
        entry.pushKV("team", project.m_team);
        entry.pushKV("eligible", project.Eligible());
        entry.pushKV("whitelisted", project.Whitelisted(whitelist));

        if (!project.Eligible()) {
            entry.pushKV("error", project.ErrorMessage());
        }

        res.push_back(entry);
    }

    return res;
}

UniValue readdata(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "readdata <key>\n"
                "\n"
                "<key> -> generic key\n"
                "\n"
                "Reads generic data from disk from a specified key\n");

    UniValue res(UniValue::VOBJ);

    std::string sKey = params[0].get_str();
    std::string sValue = "?";

    //CTxDB txdb("cr");
    CTxDB txdb;

    if (!txdb.ReadGenericData(sKey, sValue))
    {
        res.pushKV("Error", sValue);

        sValue = "Failed to read from disk.";
    }

    res.pushKV("Key", sKey);
    res.pushKV("Result", sValue);

    return res;
}

UniValue sendblock(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "sendblock <blockhash>\n"
                "\n"
                "<blockhash> Blockhash of block to send to network\n"
                "\n"
                "Sends a block to the network\n");

    UniValue res(UniValue::VOBJ);

    uint256 hash = uint256S(params[0].get_str());
    bool fResult = AskForOutstandingBlocks(hash);

    res.pushKV("Requesting", hash.ToString());
    res.pushKV("Result", fResult);

    return res;
}

UniValue superblockaverage(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "superblockaverage\n"
                "\n"
                "Displays average information for current superblock\n");

    UniValue res(UniValue::VOBJ);

    LOCK(cs_main);

    const GRC::SuperblockPtr superblock = GRC::Quorum::CurrentSuperblock();
    const int64_t now = GetAdjustedTime();

    res.pushKV("beacon_count", (uint64_t)superblock->m_cpids.TotalCount());
    res.pushKV("beacon_participant_count", (uint64_t)superblock->m_cpids.size());
    res.pushKV("average_magnitude", superblock->m_cpids.AverageMagnitude());
    res.pushKV("superblock_valid", superblock->WellFormed());
    res.pushKV("Superblock Age", superblock.Age(now));
    res.pushKV("Dire Need of Superblock", GRC::Quorum::SuperblockNeeded(now));

    return res;
}

UniValue versionreport(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
                "versionreport <lookback:int> <full:bool>\n"
                "\n"
                "<lookback> --> Number of blocks to tally from the chain head "
                    "(default: " + ToString(BLOCKS_PER_DAY) + ").\n"
                "<full> ------> Classify by commit suffix (default: false).\n"
                "\n"
                "Display the software versions of nodes that recently staked.\n");

    const int64_t lookback = params.size() > 0
        ? std::max(params[0].get_int(), 1)
        : BLOCKS_PER_DAY;

    const bool full_version = params.size() > 1
        ? params[1].get_bool()
        : false;

    LOCK(cs_main);

    return GetJSONVersionReport(lookback, full_version);
}

UniValue writedata(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
                "writedata <key> <value>\n"
                "\n"
                "<key> ---> Key where value will be written\n"
                "<value> -> Value to be written to specified key\n"
                "\n"
                "Writes a value to specified key\n");

    UniValue res(UniValue::VOBJ);

    std::string sKey = params[0].get_str();
    std::string sValue = params[1].get_str();
    //CTxDB txdb("rw");
    CTxDB txdb;
    txdb.TxnBegin();
    std::string result = "Success.";

    if (!txdb.WriteGenericData(sKey, sValue))
        result = "Unable to write.";

    if (!txdb.TxnCommit())
        result = "Unable to Commit.";

    res.pushKV("Result", result);

    return res;
}

// Network RPC commands

UniValue askforoutstandingblocks(const UniValue& params, bool fHelp)
        {
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "askforoutstandingblocks\n"
                "\n"
                "Requests network for outstanding blocks\n");

    UniValue res(UniValue::VOBJ);

    bool fResult = AskForOutstandingBlocks(uint256());

    res.pushKV("Sent.", fResult);

    return res;
}

UniValue getblockchaininfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getblockchaininfo\n"
                "\n"
                "Displays data on current blockchain\n");

    LOCK(cs_main);

    UniValue res(UniValue::VOBJ), diff(UniValue::VOBJ);

    res.pushKV("blocks", nBestHeight);
    res.pushKV("in_sync", !OutOfSyncByAge());
    res.pushKV("moneysupply", ValueFromAmount(pindexBest->nMoneySupply));
    diff.pushKV("current", GRC::GetCurrentDifficulty());
    diff.pushKV("target", GRC::GetTargetDifficulty());
    res.pushKV("difficulty", diff);
    res.pushKV("testnet", fTestNet);
    res.pushKV("errors", GetWarnings("statusbar"));

    return res;
}


UniValue currenttime(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "currenttime\n"
                "\n"
                "Displays UTC Unix time as well as date and time in UTC\n");

    UniValue res(UniValue::VOBJ);

    res.pushKV("Unix", GetAdjustedTime());
    res.pushKV("UTC", TimestampToHRDate(GetAdjustedTime()));

    return res;
}

UniValue networktime(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "networktime\n"
                "\n"
                "Displays current network time\n");

    UniValue res(UniValue::VOBJ);

    res.pushKV("Network Time", GetAdjustedTime());

    return res;
}

UniValue SuperblockReport(int lookback, bool displaycontract, std::string cpid)
{
    UniValue results(UniValue::VARR);

    CBlockIndex* pblockindex = pindexBest;
    if (!pblockindex) return results;

    const GRC::CpidOption cpid_parsed = GRC::MiningId::Parse(cpid).TryCpid();

    for (int i = 0; i < lookback; )
    {
        if (pblockindex->IsInMainChain() && pblockindex->IsSuperblock())
        {
            const GRC::ClaimOption claim = GetClaimByIndex(pblockindex);

            if (claim && claim->ContainsSuperblock())
            {
                const GRC::Superblock& superblock = *claim->m_superblock;

                UniValue c(UniValue::VOBJ);
                c.pushKV("height", ToString(pblockindex->nHeight));
                c.pushKV("block", pblockindex->GetBlockHash().GetHex());
                c.pushKV("date", TimestampToHRDate(pblockindex->nTime));
                c.pushKV("wallet_version", claim->m_client_version);

                c.pushKV("total_cpids", (int)superblock.m_cpids.TotalCount());
                c.pushKV("active_beacons", (int)superblock.m_cpids.size());
                c.pushKV("inactive_beacons", (int)superblock.m_cpids.Zeros());

                c.pushKV("total_magnitude", superblock.m_cpids.TotalMagnitude());
                c.pushKV("average_magnitude", superblock.m_cpids.AverageMagnitude());

                c.pushKV("total_projects", (int)superblock.m_projects.size());

                if (cpid_parsed)
                {
                    c.pushKV("magnitude", superblock.m_cpids.MagnitudeOf(*cpid_parsed).Floating());
                }

                if (displaycontract)
                    c.pushKV("contract_contents", SuperblockToJson(superblock));

                results.push_back(c);

                ++i;
            }
        }

        if (!pblockindex->pprev) break;
        pblockindex = pblockindex->pprev;

    }

    return results;
}

UniValue MagnitudeReport(const GRC::Cpid cpid)
{
    UniValue json(UniValue::VOBJ);

    const int64_t now = OutOfSyncByAge() ? pindexBest->nTime : GetAdjustedTime();
    const GRC::ResearchAccount& account = GRC::Tally::GetAccount(cpid);
    const GRC::AccrualComputer calc = GRC::Tally::GetComputer(cpid, now, pindexBest);

    json.pushKV("CPID", cpid.ToString());
    json.pushKV("Magnitude (Last Superblock)", GRC::Quorum::GetMagnitude(cpid).Floating());
    json.pushKV("Current Magnitude Unit", calc->MagnitudeUnit());

    json.pushKV("First Payment Time", TimestampToHRDate(account.FirstRewardTime()));
    json.pushKV("First Block Paid", account.FirstRewardBlockHash().ToString());
    json.pushKV("First Height Paid", (int)account.FirstRewardHeight());

    json.pushKV("Last Payment Time", TimestampToHRDate(account.LastRewardTime()));
    json.pushKV("Last Block Paid", account.LastRewardBlockHash().ToString());
    json.pushKV("Last Height Paid", (int)account.LastRewardHeight());

    json.pushKV("Accrual Days", calc->AccrualDays());
    json.pushKV("Owed", ValueFromAmount(calc->Accrual()));

    if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE)) {
        json.pushKV("Owed (raw)", ValueFromAmount(calc->RawAccrual()));
        json.pushKV("Owed (last superblock)", ValueFromAmount(account.m_accrual));
    }

    json.pushKV("Expected Earnings (14 days)", ValueFromAmount(calc->ExpectedDaily() * 14));
    json.pushKV("Expected Earnings (Daily)", ValueFromAmount(calc->ExpectedDaily()));

    json.pushKV("Lifetime Research Paid", ValueFromAmount(account.m_total_research_subsidy));
    json.pushKV("Lifetime Magnitude Sum", (int)account.m_total_magnitude);
    json.pushKV("Lifetime Magnitude Average", account.AverageLifetimeMagnitude());
    json.pushKV("Lifetime Payments", (int)account.m_accuracy);

    json.pushKV("Lifetime Payments Per Day", ValueFromAmount(calc->PaymentPerDay()));
    json.pushKV("Lifetime Payments Per Day Limit", ValueFromAmount(calc->PaymentPerDayLimit()));

    return json;
}

UniValue GetJSONVersionReport(const int64_t lookback, const bool full_version)
{
    const int64_t min_height = std::max<int64_t>(nBestHeight - lookback, 0);

    std::map<std::string, uint64_t> version_tally;

    for (const CBlockIndex* pindex = pindexBest;
        pindex && pindex->nHeight > min_height;
        pindex = pindex->pprev)
    {
        CBlock block;
        ReadBlockFromDisk(block, pindex, Params().GetConsensus());

        std::string version = block.PullClaim().m_client_version;

        if (version.empty()) {
            version = "unknown";
        } else if (!full_version) {
            // Ignore the source control version commit ID after the hyphen:
            const size_t separator_position = version.find('-');

            if (separator_position != std::string::npos) {
                version.erase(separator_position);
            }
        }

        ++version_tally[version];
    }

    UniValue json(UniValue::VARR);

    for (const auto& version_pair : version_tally) {
        UniValue entry(UniValue::VOBJ);

        entry.pushKV("version", version_pair.first);
        entry.pushKV("count", version_pair.second);
        entry.pushKV("percent", ((double)version_pair.second / lookback) * 100);

        json.push_back(entry);
    }

    return json;
}

// ppcoin: get information of sync-checkpoint
UniValue getcheckpoint(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getcheckpoint\n"
                "Show info of synchronized checkpoint.\n");

    UniValue result(UniValue::VOBJ);

    LOCK(cs_main);

    const CBlockIndex* pindexCheckpoint = Checkpoints::GetLastCheckpoint(mapBlockIndex);
    if (pindexCheckpoint != nullptr) {
        result.pushKV("synccheckpoint", pindexCheckpoint->GetBlockHash().ToString().c_str());
        result.pushKV("height", pindexCheckpoint->nHeight);
        result.pushKV("timestamp", DateTimeStrFormat(pindexCheckpoint->GetBlockTime()).c_str());
    }

    return result;
}

//Brod
UniValue rpc_reorganize(const UniValue& params, bool fHelp)
{
    UniValue results(UniValue::VOBJ);
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "reorganize <hash>\n"
                "Roll back the block chain to specified block hash.\n"
                "The block hash must already be present in block index");

    uint256 NewHash;
    NewHash.SetHex(params[0].get_str());

    bool fResult = ForceReorganizeToHash(NewHash);
    results.pushKV("RollbackChain",fResult);
    return results;
}

UniValue getburnreport(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getburnreport\n"
                "Scan for and aggregate network-wide amounts for provably-destroyed outputs.\n");

    CBlock block;
    CAmount total_amount = 0;
    CAmount voluntary_amount = 0;
    std::map<GRC::ContractType, CAmount> contract_amounts;

    LOCK(cs_main);

    // For now, we only consider transaction outputs with scripts that a node
    // will immediately refuse to evaluate:
    //
    //  - The script begins with the OP_RETURN op code
    //  - The script exceeds the maximum size
    //
    // We can try additional heuristics in the future, but many of these will
    // be very difficult or expensive to recognize.
    //
    for (const CBlockIndex* pindex = pindexGenesisBlock; pindex; pindex = pindex->pnext) {
        if (!ReadBlockFromDisk(block, pindex, Params().GetConsensus())) {
            continue;
        }

        for (size_t i = pindex->IsProofOfStake() ? 2 : 1; i < block.vtx.size(); ++i) {
            const CTransaction& tx = block.vtx[i];

            for (const auto& output : tx.vout) {
                if (output.scriptPubKey.IsUnspendable()) {
                    total_amount += output.nValue;

                    if (!tx.GetContracts().empty()) {
                        contract_amounts[tx.vContracts[0].m_type.Value()] += output.nValue;
                    } else {
                        voluntary_amount += output.nValue;
                    }
                }
            }
        }
    }

    UniValue json(UniValue::VOBJ);

    json.pushKV("total", ValueFromAmount(total_amount));
    json.pushKV("voluntary", ValueFromAmount(voluntary_amount));

    UniValue contracts(UniValue::VOBJ);

    for (const auto& amount_pair : contract_amounts) {
        contracts.pushKV(
            GRC::Contract::Type(amount_pair.first).ToString(),
            ValueFromAmount(amount_pair.second));
    }

    json.pushKV("contracts", contracts);

    return json;
}

UniValue createmrcrequest(const UniValue& params, const bool fHelp) {
    if (fHelp || params.size() > 3) {
        throw runtime_error("createmrcrequest [dry_run [force [fee]]]\n"
                            "\n"
                            "[dry_run] - If true, calculate the reward and fee but do not "
                            "send the contract. Defaults to false.\n"
                            "\n"
                            "[force] - If true, create the request even if it results "
                            "in a reward loss or ban from the network. Defaults to false. "
                            "Only works on testnet.\n"
                            "\n"
                            "[fee] - If passed, use the fee provided instead of the "
                            "calculated fee. Must not be lower than the calculated fee.\n"
                            "\n"
                            "Creates an MRC request. Requires an unlocked wallet.");
    }

    bool dry_run{false};
    bool force{false};
    CAmount provided_fee{0};

    if (params.size() > 0) {
        dry_run = params[0].get_bool();
    }

    if (params.size() > 1) {
        force = params[1].get_bool() && fTestNet;
    }

    if (params.size() > 2 && !ParseMoney(params[2].get_str(), provided_fee)) {
        throw runtime_error("Invalid fee.");
    }

    LOCK(cs_main);

    CBlockIndex* pindex = mapBlockIndex[hashBestChain];

    EnsureWalletIsUnlocked();

    UniValue resp(UniValue::VOBJ);

    GRC::MRC mrc;
    CAmount reward{0};
    CAmount fee{0};

    if (!IsV12Enabled(pindex->nHeight)) {
        throw runtime_error("MRC requests require version 12 blocks to be active.");
    }

    // If the fee is provided, set the fee for the CreateMRC to this value to override the calculated fee.
    if (provided_fee != 0) {
        fee = provided_fee;
    }

    // If the fee is not overridden by the provided fee above (i.e. zero), it will be filled in
    // at the calculated mrc value by CreateMRC. CreateMRC also rechecks the bounds
    // of the provided fee.
    try {
        GRC::CreateMRC(pindex, mrc, reward, fee, pwalletMain);
    } catch (GRC::MRC_error& e) {
        throw runtime_error(e.what());
    }

    if (!dry_run && !force && reward == fee) {
        throw runtime_error("MRC request is in zero payout interval.");
    }

    int pos{0};
    bool found{false};
    CAmount tail_fee{std::numeric_limits<CAmount>::max()};
    CAmount pay_limit_fee{std::numeric_limits<CAmount>::max()};
    CAmount head_fee{0};
    int queue_length{0};
    int limit = static_cast<int>(GetMRCOutputLimit(pindex->nVersion, false));

    // This sorts the MRCs in descending order of MRC fees to allow determination of the payout limit fee.

    // ---------- mrc fee --- mrc ------ descending order
    std::multimap<CAmount, GRC::MRC, std::greater<CAmount>> mrc_multimap;

    for (const auto& [_, tx] : mempool.mapTx) {
        if (!tx.GetContracts().empty()) {
            // By protocol the MRC contract MUST be the only one in the transaction.
            const GRC::Contract& contract = tx.GetContracts()[0];

            if (contract.m_type == GRC::ContractType::MRC) {
                GRC::MRC mempool_mrc = contract.CopyPayloadAs<GRC::MRC>();

                mrc_multimap.insert(std::make_pair(mempool_mrc.m_fee, mempool_mrc));
            } // match to mrc contract type
        } // contract present in transaction?
    }

    for (const auto& [_, mempool_mrc] : mrc_multimap) {
        found |= mrc.m_mining_id == mempool_mrc.m_mining_id;

        if (!found && mempool_mrc.m_fee >= mrc.m_fee) ++pos;
        head_fee = std::max(head_fee, mempool_mrc.m_fee);
        tail_fee = std::min(tail_fee, mempool_mrc.m_fee);

        ++queue_length;
    }

    // The tail fee converges from the max numeric limit of CAmount; however, when the above loop is done
    // it cannot end up with a number higher than the head fee. This can happen if there are no MRC transactions
    // in the loop.
    tail_fee = std::min(head_fee, tail_fee);

    // Here we select the minimum of the mrc_multimap.size() - 1 in the case where the multimap does not reach the
    // m_mrc_output_limit - 1, or the m_mrc_output_limit - 1 if the multimap indicates the queue is (over)full,
    // i.e. the number of MRC's in the queue exceeds the m_mrc_output_limit for paying in a block.
    int pay_limit_fee_pos = std::min<int>(mrc_multimap.size(), limit) - 1;

    if (pay_limit_fee_pos >= 0) {
        std::multimap<CAmount, GRC::MRC, std::greater<CAmount>>::iterator iter = mrc_multimap.begin();

        std::advance(iter, pay_limit_fee_pos);

        pay_limit_fee = iter->first;
    }

    pay_limit_fee = std::min(head_fee, pay_limit_fee);

    if (!dry_run && !force) {
        if (found) {
            throw runtime_error("Outstanding MRC request already present in the mempool for CPID.");
        }

        if (pos >= limit) {
            throw runtime_error("MRC request queue is full.");
        }
    }

    resp.pushKV("outstanding_request", found);
    // Sadly, humans start indexing by 1.
    resp.pushKV("limit", limit);
    resp.pushKV("mrcs_in_queue", queue_length);
    resp.pushKV("head_fee", ValueFromAmount(head_fee));

    if (queue_length >= limit) {
        resp.pushKV("pay_limit_position_fee", ValueFromAmount(pay_limit_fee));
    } else {
        resp.pushKV("pay_limit_position_fee", "N/A");
    }

    resp.pushKV("tail_fee", ValueFromAmount(tail_fee));
    resp.pushKV("pos", pos + 1);

    if (!dry_run) {
        LOCK(pwalletMain->cs_wallet);

        CWalletTx wtx;
        std::string error;

        uint32_t contract_version = IsV13Enabled(nBestHeight) ? 3 : 2;

        std::tie(wtx, error) = GRC::SendContract(GRC::MakeContract<GRC::MRC>(contract_version,
                                                                             GRC::ContractAction::ADD,
                                                                             mrc));
        if (!error.empty()) {
            throw runtime_error(error);
        }
        resp.pushKV("txid", wtx.GetHash().ToString());
    }

    resp.pushKV("mrc", MRCToJson(mrc));

    return resp;
}
