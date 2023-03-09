// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "chainparams.h"
#include "main.h"
#include "gridcoin/appcache.h"
#include "gridcoin/claim.h"
#include "gridcoin/mrc.h"
#include "gridcoin/protocol.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/contract/handler.h"
#include "gridcoin/contract/registry.h"
#include "gridcoin/beacon.h"
#include "gridcoin/project.h"
#include "gridcoin/researcher.h"
#include "gridcoin/scraper/scraper_registry.h"
#include "gridcoin/support/block_finder.h"
#include "gridcoin/support/xml.h"
#include "gridcoin/tx_message.h"
#include "gridcoin/voting/payloads.h"
#include "gridcoin/voting/registry.h"
#include "node/blockstorage.h"
#include "util.h"
#include "wallet/wallet.h"

using namespace GRC;

// -----------------------------------------------------------------------------
// Contract Context (see handler.h)
// -----------------------------------------------------------------------------

void ContractContext::Log(const std::string& prefix) const
{
    LogPrint(BCLog::LogFlags::CONTRACT,
             "<Contract::Log>: %s: block %i, txid %s, v%u, %s, %s, %s, %s",
             prefix,
             m_pindex->nHeight,
             m_tx.GetHash().ToString(),
             m_contract.m_version,
             m_contract.m_type.ToString(),
             m_contract.m_action.ToString(),
             m_contract.SharePayload()->LegacyKeyString(),
             m_contract.SharePayload()->LegacyValueString());
}

namespace {
//!
//! \brief An empty, invalid contract payload.
//!
//! Useful for situations where we need to satisfy the interface but cannot
//! provide a valid contract payload.
//!
class EmptyPayload : public IContractPayload
{
public:
    GRC::ContractType ContractType() const override
    {
        return GRC::ContractType::UNKNOWN;
    }

    bool WellFormed(const ContractAction action) const override
    {
        return false;
    }

    std::string LegacyKeyString() const override
    {
        return "";
    }

    std::string LegacyValueString() const override
    {
        return "";
    }

    CAmount RequiredBurnAmount() const override
    {
        return MAX_MONEY;
    }

    ADD_CONTRACT_PAYLOAD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(
        Stream& s,
        Operation ser_action,
        const ContractAction contract_action)
    {
        return;
    }
}; // EmptyPayload


//!
//! \brief Handles unknown contract message types by logging a message.
//!
class UnknownContractHandler : public IContractHandler
{
public:
    void Reset() override
    {
        // Nothing to do.
    }

    bool Validate(const Contract& contract, const CTransaction& tx, int& DoS) const override
    {
        return true; // No contextual validation needed yet
    }

    bool BlockValidate(const ContractContext& ctx, int& DoS) const override
    {
        return true; // No contextual validation needed yet
    }

    //!
    //! \brief Handle a contract addition.
    //!
    //! \param ctx References the contract and associated context.
    //!
    void Add(const ContractContext& ctx) override
    {
        ctx.Log("WARNING: Add unknown contract type ignored");
    }

    //!
    //! \brief Handle a contract deletion.
    //!
    //! \param ctx References the contract and associated context.
    //!
    void Delete(const ContractContext& ctx) override
    {
        ctx.Log("WARNING: Delete unknown contract type ignored");
    }

    //!
    //! \brief Handle a contract reversal.
    //!
    //! \param ctx References the contract and associated context.
    //!
    void Revert(const ContractContext& ctx) override
    {
        ctx.Log("WARNING: Revert unknown contract type ignored");
    }
};

//!
//! \brief Processes contracts from transaction messages by routing them to the
//! appropriate contract handler implementations.
//!
class Dispatcher
{
public:
    //!
    //! \brief Reset the cached state of any contract handler to prepare for
    //! historical contract replay. Note that all handlers are now native. The
    //! appcache is formally retired. The only handler left that requires
    //! historical reversion during reorgs because of multiple entries with
    //! the same key, but no registry backing store, is the Projects (whitelist)
    //! Registry. So this sole handler has to be reset and contracts replayed.
    //! The contract replay will skip contracts for other handler types where
    //! the backing store exists (beacons, scraper entries, and protocol entries),
    //! or the objects are independent and unique by key and admit to simple
    //! reversion, such as polls/votes.
    //!
    void ResetHandlers()
    {
        GetWhitelist().Reset();
    }

    //!
    //! \brief Validate the provided contract and forward it to the appropriate
    //! contract handler.
    //!
    //! \param ctx References the contract and associated context.
    //!
    void Apply(const ContractContext& ctx)
    {
        if (ctx->m_action == ContractAction::ADD) {
            ctx.Log("INFO: Add contract");
            GetHandler(ctx->m_type.Value()).Add(ctx);
            return;
        }

        if (ctx->m_action == ContractAction::REMOVE) {
            ctx.Log("INFO: Delete contract");
            GetHandler(ctx->m_type.Value()).Delete(ctx);
            return;
        }

        ctx.Log("WARNING: Unknown contract action ignored");
    }

    //!
    //! \brief Perform contextual validation for the provided contract.
    //!
    //! \param contract Contract to validate.
    //! \param tx       Transaction that contains the contract.
    //! \param DoS      Misbehavior score out.
    //!
    //! \return \c false If the contract fails validation.
    //!
    bool Validate(const Contract& contract, const CTransaction& tx, int& DoS)
    {
        return GetHandler(contract.m_type.Value()).Validate(contract, tx, DoS);
    }

    //!
    //! \brief Perform contextual validation for the provided contract including block context. This is used
    //! in ConnectBlock.
    //!
    //! \param ctx ContractContext to validate.
    //! \param DoS Misbehavior score out.
    //!
    //! \return \c false If the contract fails validation.
    //!
    bool BlockValidate(const ContractContext& ctx, int& DoS)
    {
        if (!GetHandler(ctx.m_contract.m_type.Value()).BlockValidate(ctx, DoS)) {
            error("%s: Contract of type %s failed validation.",
                  __func__,
                  ctx.m_contract.m_type.ToString());

            return false;
        }

        return true;
    }

    //!
    //! \brief Revert a previously-applied contract from a transaction message
    //! by passing it to the appropriate contract handler.
    //!
    //! \param ctx References the contract and associated context.
    //!
    void Revert(const ContractContext& ctx)
    {
        ctx.Log("INFO: Revert contract");

        // The default implementation of IContractHandler reverses an action
        // (addition or deletion) declared in the contract argument, but the
        // type-specific handlers may override this behavior as needed:
        GetHandler(ctx->m_type.Value()).Revert(ctx);
    }

private:
    MRCContractHandler m_mrc_contract_handler;  //<! Simple wrapper to do context validation on MRC transactions.
    UnknownContractHandler m_unknown_handler;   //<! Logs unknown types.

    //!
    //! \brief Select an appropriate contract handler based on the message type.
    //!
    //! \param type Contract type. Determines how to handle the message.
    //!
    //! \return Reference to an object capable of handling the contract type.
    //!
    IContractHandler& GetHandler(const ContractType type)
    {
        // TODO: build contract handlers for the remaining contract types:
        // TODO: refactor to dynamic registration for easier testing:
        switch (type) {
            case ContractType::BEACON:     return GetBeaconRegistry();
            case ContractType::POLL:       return GetPollRegistry();
            case ContractType::PROJECT:    return GetWhitelist();
            case ContractType::PROTOCOL:   return GetProtocolRegistry();
            case ContractType::SCRAPER:    return GetScraperRegistry();
            case ContractType::VOTE:       return GetPollRegistry();
            case ContractType::MRC:        return m_mrc_contract_handler;
            default:                       return m_unknown_handler;
        }
    }
}; // class Dispatcher

//!
//! \brief Global contract dispatcher instance.
//!
Dispatcher g_dispatcher;

//!
//! \brief Validate a legacy contract message.
//!
//! This function performs a sanity check for historical contract messages. It
//! verifies the contract signature for administrative contracts only. Version
//! 2 contracts undergo much more robust validation. Testnet contains some bad
//! administrative contracts that this routine filters out.
//!
//! \param contract The version 1 contract to validate.
//! \param tx       The transaction that contains the contract.
//!
//! \return \c true if the contract passes validation.
//!
bool CheckLegacyContract(const Contract& contract, const CTransaction& tx, int block_height)
{
    if (!contract.WellFormed()) {
        return false;
    }

    if (!contract.RequiresMasterKey()) {
        return true;
    }

    const std::string base64_sig = ExtractXML(tx.hashBoinc, "<MS>", "</MS>");

    if (base64_sig.empty()) {
        return false;
    }

    bool invalid;
    const std::vector<uint8_t> sig = DecodeBase64(base64_sig.c_str(), &invalid);

    if (invalid) {
        return false;
    }

    const std::string type_string = contract.m_type.ToString();

    // We use static_cast here instead of dynamic_cast to avoid the lookup. The
    // value of m_payload is guaranteed to be a LegacyPayload for v1 contracts:
    //
    const ContractPayload payload = contract.m_body.AssumeLegacy();
    const auto& body = static_cast<const LegacyPayload&>(*payload);

    const uint256 body_hash = Hash(type_string, body.m_key, body.m_value);

    return CPubKey(Params().MasterKey(block_height)).Verify(body_hash, sig);
}


} // anonymous namespace

// -----------------------------------------------------------------------------
// Global Functions
// -----------------------------------------------------------------------------

Contract GRC::MakeLegacyContract(
    const ContractType type,
    const ContractAction action,
    std::string key,
    std::string value)
{
    // There will be no new LegacyPayload contracts past version 2.
    Contract contract = MakeContract<LegacyPayload>(
        uint32_t {2},
        action,
        std::move(key),
        std::move(value));

    contract.m_type = type;

    return contract;
}

void GRC::ReplayContracts(CBlockIndex* pindex_end, CBlockIndex* pindex_start)
{
    CBlockIndex*& pindex = pindex_start;

    // If there is no pindex_start (i.e. default value of nullptr), then set standard lookback. A Non-standard lookback
    // where there is a specific pindex_start argument supplied, is only used in the GRC InitializeContracts call for
    // when the beacon and scraper database in LevelDB has not already been populated.
    if (!pindex)
    {
        pindex = GRC::BlockFinder::FindByMinTime(pindexBest->nTime - Params().GetConsensus().StandardContractReplayLookback);
    }

    if (pindex->nHeight < (fTestNet ? 1 : 164618)) {
        return;
    }

    LogPrint(BCLog::LogFlags::CONTRACT,	"Replaying contracts from block %" PRId64 "...", pindex->nHeight);

    // This no longer includes beacons, scraper entries, or polls/votes, but DOES include projects and protocol entries.
    g_dispatcher.ResetHandlers();

    RegistryBookmarks db_heights;

    for (const auto& contract_type : CONTRACT_TYPES) {
        std::optional<int> db_height = db_heights.GetRegistryBlockHeight(contract_type);

        if (!db_height) continue;

        LogPrint(BCLog::LogFlags::CONTRACT, "INFO: %s: %s entry database at height %i",
                 __func__,
                 Contract::Type::ToString(contract_type),
                 *db_height);
    }

    // This provides a convenient reference for the beacon registry, which has special processing below due to activations
    // and the IsContract flag corrections. The scraper entries require no such special processing and are handled
    // by the ApplyContracts call.
    BeaconRegistry& beacons = GetBeaconRegistry();

    if (beacons.NeedsIsContractCorrection())
    {
        LogPrintf("INFO %s: The NeedsIsContractCorrection flag is set. All blocks within the scan range "
                  "will be checked to ensure the contains contract flag is set correctly and corrections made. "
                  "This may take a little longer than the standard replay.",
                  __func__);
    }

    CBlock block;

    // These are memorized consecutively in order from oldest to newest.
    for (; pindex; pindex = pindex->pnext) {

        // If the NeedsIsContractCorrection flag is set which means all blocks within the scan range
        // have to be checked, OR the block index entry is already marked to contain contract(s),
        // then apply the contracts found in the block.
        if (beacons.NeedsIsContractCorrection() || pindex->IsContract()) {
            if (!ReadBlockFromDisk(block, pindex, Params().GetConsensus())) {
                continue;
            }

            // The ApplyContracts below handles all of the contract types. The rest of this is special
            // processing required for beacons.
            bool found_contract;
            ApplyContracts(block, pindex, db_heights, found_contract);

            // If a contract was found and the NeedsIsContractCorrection flag is set, then
            // record that a contract was found in the block index. This corrects the block index
            // record.
            if (found_contract && beacons.NeedsIsContractCorrection() && !pindex->IsContract())
            {
                LogPrintf("WARNING %s: There were found contract(s) in block %i but IsContract() is false. "
                          "Correcting IsContract flag to true in the block index.",
                          __func__,
                          pindex->nHeight);

                pindex->MarkAsContract();

                CTxDB txdb("rw");

                CDiskBlockIndex disk_block_index(pindex);
                if (!txdb.WriteBlockIndex(disk_block_index))
                {
                    error("%s: Block index correction of IsContract flag for block %i failed.",
                          __func__,
                          pindex->nHeight);
                }
            }
        }

        if (pindex->IsSuperblock() && pindex->nVersion >= 11) {
            if (block.hashPrevBlock != pindex->pprev->GetBlockHash()
                && !ReadBlockFromDisk(block, pindex, Params().GetConsensus()))
            {
                continue;
            }

            // Only apply activations that have not already been stored/loaded into
            // the beacon DB. This is at the block level, so we have to be careful here.
            // If the pindex->nHeight is equal to the beacon db height, then the ActivatePending
            // has already been replayed for this block and we do not need to call it again for that block.
            // BECAUSE ActivatePending is called at the block level. We do not need to worry about multiple
            // calls within the same block like below in ApplyContracts.
            std::optional<int> beacon_db_height = db_heights.GetRegistryBlockHeight(ContractType::BEACON);

            if (beacon_db_height) {
                if (pindex->nHeight > *beacon_db_height) {
                    beacons.ActivatePending(
                                block.GetSuperblock()->m_verified_beacons.m_verified,
                                block.GetBlockTime(),
                                block.GetHash(),
                                pindex->nHeight);
                } else {
                    LogPrint(BCLog::LogFlags::BEACON, "INFO: %s: GetBeaconRegistry().ActivatePending() "
                              "skipped for superblock: pindex->height = %i <= beacon_db_height = %i.",
                             __func__, pindex->nHeight, *beacon_db_height);
                }
            }
        }

        if (pindex == pindex_end)
        {
            // Finished the rescan. If the NeedsIsContractCorrection was set to true, then reset
            // to false.
            if (beacons.NeedsIsContractCorrection()) beacons.SetNeedsIsContractCorrection(false);

            break;
        }
    }

    Researcher::Refresh();
}

void GRC::ApplyContracts(
    const CBlock& block,
    const CBlockIndex* const pindex, const RegistryBookmarks& db_heights,
    bool& out_found_contract)
{
    out_found_contract = false;

    // Skip coinbase and coinstake transactions:
    for (auto iter = std::next(block.vtx.begin(), 2), end = block.vtx.end();
        iter != end;
        ++iter)
    {
        ApplyContracts(*iter, pindex, db_heights, out_found_contract);
    }
}

void GRC::ApplyContracts(
    const CTransaction& tx,
    const CBlockIndex* const pindex, const RegistryBookmarks& db_heights,
    bool& out_found_contract)
{
    for (const auto& contract : tx.GetContracts()) {
        // Do not (re)apply contracts that have already been stored/loaded into
        // the relevant entry dbs up to the block BEFORE the relevant db height. Because
        // these db heights are at the block level, and are updated on each relevant entry
        // insert, when in a sync from zero situation where the contracts are played as each block
        // is validated, any relevant contract in the block EQUAL to the relevant db height
        // must fail this test and be inserted again, because otherwise the second and succeeding
        // contracts on the same block will not be inserted and those relevant entries will
        // not be recorded properly. For beacons, this was the cause of the failure to sync through
        // 2069264 that started on 20210312. See GitHub issue #2045.

        bool skip_apply_contract = false;

        for (const auto& contract_type : CONTRACT_TYPES) {
            if (contract.m_type == contract_type) {

                std::optional<int> db_height = db_heights.GetRegistryBlockHeight(contract_type);

                if (db_height && pindex->nHeight < *db_height) {
                    skip_apply_contract = true;
                    break;
                }
            }
        }

        if (skip_apply_contract) continue;

        // V2 contracts are checked upon receipt:
        if (contract.m_version == 1 && !CheckLegacyContract(contract, tx, pindex->nHeight)) {
            continue;
        }

        // Support dynamic team requirement or whitelist configuration:
        //
        // TODO: move this into the appropriate contract handler.
        //
        if (contract.m_type == ContractType::PROTOCOL) {
            const auto payload = contract.SharePayloadAs<LegacyPayload>();

            if (payload->m_key == "REQUIRE_TEAM_WHITELIST_MEMBERSHIP"
                || payload->m_key == "TEAM_WHITELIST")
            {
                // Rescan in-memory project CPIDs to resolve a primary CPID
                // that fits the now active team requirement settings:
                Researcher::MarkDirty();
            }
        }

        // Note that for polls and votes, this rescan could overlap contracts already recorded. The handlers for polls/votes
        // check for the existence of contracts already recorded and will prevent a double application.
        g_dispatcher.Apply({ contract, tx, pindex });

        // Don't track transaction message contracts in the block index:
        out_found_contract |= contract.m_type != ContractType::MESSAGE;
    }
}

bool GRC::ValidateContracts(const CTransaction& tx, int& DoS)
{
    for (const auto& contract : tx.GetContracts()) {
        if (!g_dispatcher.Validate(contract, tx, DoS)) {
            return false;
        }
    }

    return true;
}

bool GRC::BlockValidateContracts(const CBlockIndex* const pindex, const CTransaction& tx, int& DoS)
{
    for (const auto& contract: tx.GetContracts()) {
        if (!g_dispatcher.BlockValidate({ contract, tx, pindex }, DoS)) {
            return false;
        }
    }

    return true;
}

void GRC::RevertContracts(const CTransaction& tx, const CBlockIndex* const pindex)
{
    // Reverse the contracts. Reorganize will load any previous versions:
    for (const auto& contract : tx.GetContracts()) {
        // V2 contracts are checked upon receipt:
        if (contract.m_version == 1 && !CheckLegacyContract(contract, tx, pindex->nHeight)) {
            continue;
        }

        g_dispatcher.Revert({ contract, tx, pindex });
    }
}

// -----------------------------------------------------------------------------
// Class: Contract
// -----------------------------------------------------------------------------

constexpr CAmount Contract::STANDARD_BURN_AMOUNT; // for clang

Contract::Contract()
    : m_version(Contract::CURRENT_VERSION)
    , m_type(ContractType::UNKNOWN)
    , m_action(ContractAction::UNKNOWN)
    , m_body()
{
}

Contract::Contract(
    Contract::Type type,
    Contract::Action action,
    Contract::Body body)
    : m_version(Contract::CURRENT_VERSION)
    , m_type(type)
    , m_action(action)
    , m_body(std::move(body))
{
}

Contract::Contract(
    uint32_t version,
    Contract::Type type,
    Contract::Action action,
    Contract::Body body)
    : m_version(version)
    , m_type(type)
    , m_action(action)
    , m_body(std::move(body))
{
}

bool Contract::Detect(const std::string& message)
{
    return !message.empty()
        && Contains(message, "<MT>")
        // Superblock handled elsewhere:
        && !Contains(message, "<MT>superblock</MT>");
}

Contract Contract::Parse(const std::string& message)
{
    if (message.empty()) {
        return Contract();
    }

    return Contract(
        1, // Legacy XML-like string contracts always parse to a v1 contract.
        Contract::Type::ParseLegacy(ExtractXML(message, "<MT>", "</MT>")),
        Contract::Action::Parse(ExtractXML(message, "<MA>", "</MA>")),
        Contract::Body(ContractPayload::Make<LegacyPayload>(
            ExtractXML(message, "<MK>", "</MK>"),
            ExtractXML(message, "<MV>", "</MV>"))));
}

bool Contract::RequiresMasterKey() const
{
    switch (m_type.Value()) {
        case ContractType::BEACON:
            // Contracts version 2+ allow participants to revoke their own
            // beacons by signing them with the original private key:
            return m_version == 1 && m_action == ContractAction::REMOVE;

        case ContractType::POLL:     return m_action == ContractAction::REMOVE;
        case ContractType::PROJECT:  return true;
        case ContractType::PROTOCOL: return true;
        case ContractType::SCRAPER:  return true;
        case ContractType::VOTE:     return m_action == ContractAction::REMOVE;
        default:                     return false;
    }
}

CAmount Contract::RequiredBurnAmount() const
{
    return m_body.m_payload->RequiredBurnAmount();
}

bool Contract::WellFormed() const
{
    return m_version > 0 && m_version <= Contract::CURRENT_VERSION
        && m_type != ContractType::UNKNOWN
        && m_action != ContractAction::UNKNOWN
        && m_body.WellFormed(m_action.Value());
}

ContractPayload Contract::SharePayload() const
{
    // The scraper entry format is changed to native later than the others and a new contract
    // version three is introduced for that. This will be coincident with block v13.
    if (m_version < 2
            || (m_type == ContractType::SCRAPER && m_version < 3)
            || (m_type == ContractType::PROTOCOL && m_version < 3)) {
        return m_body.ConvertFromLegacy(m_type.Value(), m_version);
    }

    return m_body.m_payload;
}

void Contract::Log(const std::string& prefix) const
{
    // TODO: temporary... needs better logging
    LogPrint(BCLog::LogFlags::CONTRACT,
        "<Contract::Log>: %s: v%d, %s, %s, %s, %s",
        prefix,
        m_version,
        m_type.ToString(),
        m_action.ToString(),
        m_body.m_payload->LegacyKeyString(),
        m_body.m_payload->LegacyValueString());
}

// -----------------------------------------------------------------------------
// Class: Contract::Type
// -----------------------------------------------------------------------------

Contract::Type::Type(ContractType type) : EnumByte(type)
{
}

Contract::Type Contract::Type::ParseLegacy(std::string input)
{
    // For parsing historical contracts. Do not add new contract types
    // to this function. Add to Contract::Type::Parse instead.

    // Ordered by frequency:
    if (input == "beacon")         return ContractType::BEACON;
    if (input == "vote")           return ContractType::VOTE;
    if (input == "poll")           return ContractType::POLL;
    if (input == "project")        return ContractType::PROJECT;
    if (input == "scraper")        return ContractType::SCRAPER;
    if (input == "protocol")       return ContractType::PROTOCOL;

    return ContractType::UNKNOWN;
}

Contract::Type Contract::Type::Parse(std::string input)
{
    // Ordered by frequency:
    if (input == "claim")          return ContractType::CLAIM;
    if (input == "beacon")         return ContractType::BEACON;
    if (input == "mrc")            return ContractType::MRC;
    if (input == "vote")           return ContractType::VOTE;
    if (input == "poll")           return ContractType::POLL;
    if (input == "project")        return ContractType::PROJECT;
    if (input == "scraper")        return ContractType::SCRAPER;
    if (input == "protocol")       return ContractType::PROTOCOL;
    if (input == "message")        return ContractType::MESSAGE;

    return ContractType::UNKNOWN;
}

std::string Contract::Type::ToString() const
{
    switch (m_value) {
        case ContractType::BEACON:     return "beacon";
        case ContractType::CLAIM:      return "claim";
        case ContractType::MRC:        return "mrc";
        case ContractType::MESSAGE:    return "message";
        case ContractType::POLL:       return "poll";
        case ContractType::PROJECT:    return "project";
        case ContractType::PROTOCOL:   return "protocol";
        case ContractType::SCRAPER:    return "scraper";
        case ContractType::VOTE:       return "vote";
        default:                       return "";
    }
}

std::string Contract::Type::ToString(ContractType contract_type)
{
    switch (contract_type) {
        case ContractType::BEACON:     return "beacon";
        case ContractType::CLAIM:      return "claim";
        case ContractType::MRC:        return "mrc";
        case ContractType::MESSAGE:    return "message";
        case ContractType::POLL:       return "poll";
        case ContractType::PROJECT:    return "project";
        case ContractType::PROTOCOL:   return "protocol";
        case ContractType::SCRAPER:    return "scraper";
        case ContractType::VOTE:       return "vote";
        default:                       return "";
    }
}

// -----------------------------------------------------------------------------
// Class: Contract::Action
// -----------------------------------------------------------------------------

Contract::Action::Action(ContractAction action) : EnumByte(action)
{
}

Contract::Action Contract::Action::Parse(std::string input)
{
    if (input == "A")  return ContractAction::ADD;
    if (input == "D")  return ContractAction::REMOVE;

    return ContractAction::UNKNOWN;
}

std::string Contract::Action::ToString() const
{
    switch (m_value) {
        case ContractAction::ADD:    return "A";
        case ContractAction::REMOVE: return "D";
        default:                     return "";
    }
}

// -----------------------------------------------------------------------------
// Class: Contract::Body
// -----------------------------------------------------------------------------

Contract::Body::Body()
    : m_payload(ContractPayload::Make<EmptyPayload>())
{
}

Contract::Body::Body(ContractPayload payload)
    : m_payload(std::move(payload))
{
}

bool Contract::Body::WellFormed(const ContractAction action) const
{
    return m_payload->WellFormed(action);
}

ContractPayload Contract::Body::AssumeLegacy() const
{
    return m_payload;
}

ContractPayload Contract::Body::ConvertFromLegacy(const ContractType type, uint32_t version) const
{
    // We use static_cast here instead of dynamic_cast to avoid the lookup. The
    // value of m_payload is guaranteed to be a LegacyPayload for v1 contracts.
    //
    LegacyPayload legacy;

    //if (version < 2) {
        legacy = static_cast<const LegacyPayload&>(*m_payload);
    //}

    switch (type) {
        case ContractType::UNKNOWN:
            return ContractPayload::Make<EmptyPayload>();
        case ContractType::BEACON:
            return ContractPayload::Make<BeaconPayload>(
                BeaconPayload::Parse(legacy.m_key, legacy.m_value));
        case ContractType::CLAIM:
            // Claims can only exist in a coinbase transaction and have no
            // legacy representation as a contract:
            assert(false && "Attempted to convert legacy claim contract.");
        case ContractType::MRC:
            // MRCs have no legacy representation as a contract.
            assert(false && "Attempted to convert non-existent legacy MRC contract.");
        case ContractType::MESSAGE:
            // The contract system does not map legacy transaction messages
            // stored in the CTransaction::hashBoinc field:
            assert(false && "Attempted to convert legacy message contract.");
        case ContractType::POLL:
            return ContractPayload::Make<PollPayload>(
                Poll::Parse(legacy.m_key, legacy.m_value));
        case ContractType::PROJECT:
            return ContractPayload::Make<Project>(legacy.m_key, legacy.m_value, 0);
        case ContractType::PROTOCOL:
            return ContractPayload::Make<ProtocolEntryPayload>(
                ProtocolEntryPayload::Parse(legacy.m_key, legacy.m_value));
        case ContractType::SCRAPER:
            return ContractPayload::Make<ScraperEntryPayload>(
                ScraperEntryPayload::Parse(legacy.m_key, legacy.m_value));
        case ContractType::VOTE:
            return ContractPayload::Make<LegacyVote>(
                LegacyVote::Parse(legacy.m_key, legacy.m_value));
        case ContractType::OUT_OF_BOUND:
            assert(false);
    }

    return ContractPayload::Make<EmptyPayload>();
}

void Contract::Body::ResetType(const ContractType type)
{
    switch (type) {
        case ContractType::UNKNOWN:
            m_payload.Reset(new EmptyPayload());
            break;
        case ContractType::BEACON:
            m_payload.Reset(new BeaconPayload());
            break;
        case ContractType::CLAIM:
            m_payload.Reset(new Claim());
            break;
        case ContractType::MRC:
            m_payload.Reset(new MRC());
            break;
        case ContractType::MESSAGE:
            m_payload.Reset(new TxMessage());
            break;
        case ContractType::POLL:
            // Note that the contract code expects cs_main to already be taken which
            // means that the access to nBestHeight is safe.
            // TODO: This ternary should be removed at the next mandatory after
            // Kermit's Mom.
            m_payload.Reset(new PollPayload(IsPollV3Enabled(nBestHeight) ? 3 : 2));
            break;
        case ContractType::PROJECT:
            m_payload.Reset(new Project());
            break;
        case ContractType::PROTOCOL:
            // Note that the contract code expects cs_main to already be taken which
            // means that the access to nBestHeight is safe.
            // TODO: This ternary should be removed at the next mandatory after
            // Kermit's Mom.
            m_payload.Reset(new ProtocolEntryPayload(IsV13Enabled(nBestHeight) ? 2 : 1));
            break;
        case ContractType::SCRAPER:
            // Note that the contract code expects cs_main to already be taken which
            // means that the access to nBestHeight is safe.
            // TODO: This ternary should be removed at the next mandatory after
            // Kermit's Mom.
            m_payload.Reset(new ScraperEntryPayload(IsV13Enabled(nBestHeight) ? 2 : 1));
            break;
        case ContractType::VOTE:
            m_payload.Reset(new Vote());
            break;
        case ContractType::OUT_OF_BOUND:
            assert(false);
    }
}

// -----------------------------------------------------------------------------
// Abstract Class: IContractHandler
// -----------------------------------------------------------------------------

void IContractHandler::Revert(const ContractContext& ctx)
{
    if (ctx->m_action == ContractAction::ADD) {
        Delete(ctx);
        return;
    }

    if (ctx->m_action == ContractAction::REMOVE) {
        Add(ctx);
        return;
    }

    error("Unknown contract action ignored: %s", ctx->m_action.ToString());
}
