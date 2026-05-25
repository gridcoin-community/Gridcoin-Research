#include <stdexcept>

#include "init.h"
#include "main.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/contract/message.h"
#include "gridcoin/voting/builders.h"
#include "gridcoin/voting/payloads.h"
#include "gridcoin/voting/poll.h"
#include "gridcoin/voting/registry.h"
#include "gridcoin/voting/result.h"
#include "primitives/transaction.h"
#include "protocol.h"
#include "rpc/util.h"
#include "server.h"
#include "uint256.h"
#include "util/strencodings.h"
#include "util/time.h"

using namespace GRC;

namespace {
const PollReference* TryPollByTitleOrId(const std::string& title_or_id) EXCLUSIVE_LOCKS_REQUIRED(PollRegistry::cs_poll_registry)
{
    PollRegistry& registry = GetPollRegistry();

    if (title_or_id.size() == sizeof(uint256) * 2 && IsHex(title_or_id)) {
        const uint256 txid = uint256S(title_or_id);

        // This will return a ref to the poll in the registry if found, or, try and load it and return the ref if
        // the load is successful.
        if (const PollReference* ref = WITH_LOCK(cs_main, return registry.TryByTxidWithAddHistoricalPollAndVotes(txid))) {
            return ref;
        }
    }

    const std::string title = boost::to_lower_copy(title_or_id);

    if (const PollReference* ref = registry.TryByTitle(title)) {
        return ref;
    }

    return nullptr;
}

UniValue PollChoicesToJson(const Poll::ChoiceList& choices)
{
    UniValue json(UniValue::VARR);

    for (size_t i = 0; i < choices.size(); ++i) {
        UniValue choice(UniValue::VOBJ);
        choice.pushKV("id", (int)i);
        choice.pushKV("label", choices.At(i)->m_label);

        json.push_back(choice);
    }

    return json;
}

UniValue PollAdditionalFieldsToJson(const Poll::AdditionalFieldList fields)
{
    UniValue json(UniValue::VARR);

    for (size_t i = 0; i < fields.size(); ++i) {
        UniValue field(UniValue::VOBJ);

        field.pushKV("name", fields.At(i)->m_name);
        field.pushKV("value", fields.At(i)->m_value);
        field.pushKV("required", fields.At(i)->m_required);

        json.push_back(field);
    }

    return json;
}

UniValue PollToJson(const Poll& poll, const uint256 txid)
{
    UniValue json(UniValue::VOBJ);

    json.pushKV("title", poll.m_title);
    json.pushKV("id", txid.ToString());
    json.pushKV("question", poll.m_question);
    json.pushKV("url", poll.m_url);
    json.pushKV("additional_fields", PollAdditionalFieldsToJson(poll.AdditionalFields()));
    json.pushKV("poll_type", poll.PollTypeToString());
    json.pushKV("poll_type_id", (int)poll.m_type.Raw());
    json.pushKV("weight_type", poll.WeightTypeToString());
    json.pushKV("weight_type_id", (int)poll.m_weight_type.Raw());
    json.pushKV("response_type", poll.ResponseTypeToString());
    json.pushKV("response_type_id", (int)poll.m_response_type.Raw());
    json.pushKV("duration_days", (int)poll.m_duration_days);
    json.pushKV("expiration", TimestampToHRDate(poll.Expiration()));
    json.pushKV("timestamp", TimestampToHRDate(poll.m_timestamp));
    json.pushKV("choices", PollChoicesToJson(poll.Choices()));

    return json;
}

UniValue PollToJson(const Poll& poll, const PollReference& poll_ref)
{
    UniValue json = PollToJson(poll, poll_ref.Txid());
    json.pushKV("votes", (uint64_t)poll_ref.Votes().size());

    return json;
}

UniValue PollResultToJson(const PollResult& result, const PollReference& poll_ref)
{
    UniValue json(UniValue::VOBJ);

    json.pushKV("poll_id", poll_ref.Txid().ToString());
    json.pushKV("poll_title", poll_ref.Title());
    json.pushKV("poll_expired", poll_ref.Expired(GetAdjustedTime()));

    {
        LOCK(cs_main);

        if (auto start_height = poll_ref.GetStartingHeight()) {
            json.pushKV("starting_block_height", *start_height);
        }

        if (auto end_height = poll_ref.GetEndingHeight()) {
            json.pushKV("ending_block_height", *end_height);
        }
    }

    json.pushKV("votes", (uint64_t)poll_ref.Votes().size());
    json.pushKV("invalid_votes", (uint64_t)result.m_invalid_votes);
    json.pushKV("total_weight", ValueFromAmount(result.m_total_weight));

    if (result.m_active_vote_weight) {
        json.pushKV("active_vote_weight", ValueFromAmount(*result.m_active_vote_weight));
    }

    if (result.m_vote_percent_avw) {
        json.pushKV("vote_percent_avw", *result.m_vote_percent_avw);
    }

    if (result.m_poll_results_validated) {
        json.pushKV("poll_results_validated", *result.m_poll_results_validated);
    }

    if (!result.m_votes.empty()) {
        json.pushKV("top_choice_id", (uint64_t)result.Winner());
        json.pushKV("top_choice", result.WinnerLabel());
    } else {
        json.pushKV("top_choice_id", NullUniValue);
        json.pushKV("top_choice", NullUniValue);
    }

    UniValue responses(UniValue::VARR);

    for (size_t i = 0; i < result.m_responses.size(); ++i) {
        const PollResult::ResponseDetail& detail = result.m_responses[i];
        UniValue response(UniValue::VOBJ);

        response.pushKV("choice", result.m_poll.Choices().At(i)->m_label);
        response.pushKV("id", (int)i);
        response.pushKV("weight", ValueFromAmount(detail.m_weight));
        response.pushKV("votes", detail.m_votes);

        responses.push_back(response);
    }

    json.pushKV("responses", responses);

    return json;
}

UniValue PollResultToJson(const PollReference& poll_ref)
{
    g_timer.InitTimer("buildPollTable", LogInstance().WillLogCategory(BCLog::LogFlags::VOTE));

    GetPollRegistry().registry_traversal_in_progress = true;

    try {
         if (const PollResultOption result = PollResult::BuildFor(poll_ref)) {
            GetPollRegistry().registry_traversal_in_progress = false;

            return PollResultToJson(*result, poll_ref);
        }
    } catch (InvalidDuetoReorgFork& e) {
        GetPollRegistry().registry_traversal_in_progress = false;

        throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to load poll from disk due to reorg in progress during inquiry.");
    }

    GetPollRegistry().registry_traversal_in_progress = false;

    throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to load poll from disk");
}

UniValue VoteDetailsToJson(const PollResult& result)
{
    UniValue json(UniValue::VARR);

    for (const auto& detail : result.m_votes) {
        UniValue vote(UniValue::VOBJ);

        vote.pushKV("amount", ValueFromAmount(detail.m_amount));
        vote.pushKV("cpid", detail.m_mining_id.ToString());
        vote.pushKV("magnitude", detail.m_magnitude.Floating());

        UniValue answers(UniValue::VARR);
        PollResult::Weight total_weight = 0;

        for (const auto& answer_pair : detail.m_responses) {
            UniValue answer(UniValue::VOBJ);
            answer.pushKV("id", (int)answer_pair.first);
            answer.pushKV("weight", ValueFromAmount(answer_pair.second));

            total_weight += answer_pair.second;
            answers.push_back(answer);
        }

        vote.pushKV("total_weight", ValueFromAmount(total_weight));
        vote.pushKV("answers", answers);

        json.push_back(vote);
    }

    return json;
}

UniValue VoteDetailsToJson(const PollReference& poll_ref)
{
    GetPollRegistry().registry_traversal_in_progress = true;

    try {
        if (const PollResultOption result = PollResult::BuildFor(poll_ref)) {
            GetPollRegistry().registry_traversal_in_progress = false;

            return VoteDetailsToJson(*result);
        }
    } catch (InvalidDuetoReorgFork& e) {
        GetPollRegistry().registry_traversal_in_progress = false;

        throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to load poll from disk due to reorg in progress during inquiry.");
    }

    GetPollRegistry().registry_traversal_in_progress = false;

    throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to load poll from disk.");
}

UniValue AddressClaimToJson(const AddressClaim& claim)
{
    UniValue json(UniValue::VOBJ);

    json.pushKV("public_key", HexStr(claim.m_public_key));
    json.pushKV("signature", HexStr(claim.m_signature));

    UniValue outpoints(UniValue::VARR);

    for (const auto& txo : claim.m_outpoints) {
        UniValue outpoint(UniValue::VOBJ);
        outpoint.pushKV("txid", txo.hash.ToString());
        outpoint.pushKV("offset", (uint64_t)txo.n);

        outpoints.push_back(outpoint);
    }

    json.pushKV("outpoints", outpoints);

    return json;
}

UniValue BalanceClaimToJson(const BalanceClaim& claim)
{
    UniValue json(UniValue::VARR);

    for (const auto& address_claim : claim.m_address_claims) {
        json.push_back(AddressClaimToJson(address_claim));
    }

    return json;
}

UniValue MagnitudeClaimToJson(const MagnitudeClaim& claim)
{
    UniValue json(UniValue::VOBJ);

    json.pushKV("mining_id", claim.m_mining_id.ToString());
    json.pushKV("beacon_txid", claim.m_beacon_txid.ToString());
    json.pushKV("signature", HexStr(claim.m_signature));

    return json;
}

UniValue PollClaimToJson(const PollEligibilityClaim& claim)
{
    UniValue json(UniValue::VOBJ);

    json.pushKV("version", (uint64_t)claim.m_version);
    json.pushKV("balance_claim", BalanceClaimToJson(claim.m_balance_claim));

    return json;
}

UniValue VoteClaimToJson(const VoteWeightClaim& claim)
{
    UniValue json(UniValue::VOBJ);

    json.pushKV("version", (uint64_t)claim.m_version);
    json.pushKV("magnitude_claim", MagnitudeClaimToJson(claim.m_magnitude_claim));
    json.pushKV("balance_claim", BalanceClaimToJson(claim.m_balance_claim));

    return json;
}

UniValue SubmitVote(const Poll& poll, VoteBuilder builder)
{
    std::pair<CWalletTx, std::string> result_pair;

    {
        LOCK2(cs_main, pwalletMain->cs_wallet);
        // Note that a lock on cs_poll_registry does NOT need to be taken here.
        // This lock will be taken by the contract handler.

        uint32_t contract_version = IsV13Enabled(nBestHeight) ? 3: 2;

        result_pair = SendContract(builder.BuildContractTx(pwalletMain, contract_version));
    }

    if (!result_pair.second.empty()) {
        throw JSONRPCError(RPC_WALLET_ERROR, result_pair.second);
    }

    const CWalletTx& result_tx = result_pair.first;
    const ContractPayload payload = result_tx.vContracts[0].SharePayload();

    UniValue result(UniValue::VOBJ);
    result.pushKV("poll", poll.m_title);
    result.pushKV("vote_txid", result_tx.GetHash().ToString());

    UniValue responses(UniValue::VARR);

    const auto& vote = payload.As<Vote>();

    for (const auto& offset : vote.m_responses) {
        if (const Poll::Choice* choice = poll.Choices().At(offset)) {
            responses.push_back(choice->m_label);
        }
    }

    result.pushKV("responses", responses);

    return result;
}
} // Anonymous namespace

UniValue addpoll(const UniValue& params, bool fHelp)
{
    // The set of valid poll types depends on whether PollV3 is active, so the help
    // text must be constructed per-call from the runtime-resolved list.
    uint32_t payload_version = 0;
    std::vector<PollType> valid_poll_types;
    {
        LOCK(cs_main);

        payload_version = IsPollV3Enabled(nBestHeight) ? 3 : 2;
        valid_poll_types = GRC::PollPayload::GetValidPollTypes(payload_version);
    }

    std::stringstream types_ss;
    for (const auto& type : valid_poll_types) {
        if (types_ss.str() != std::string{}) {
            types_ss << ", ";
        }
        types_ss << ToLower(Poll::PollTypeToString(type, false));
    }

    const RPCHelpMan help{
        "addpoll",
        "Add a poll to the network.\n"
        "Requires 100K GRC balance. Costs 50 GRC.\n"
        "Provide an empty string for <answers> when choosing \"yes/no/abstain\" for <responsetype>.\n"
        "Certain poll types require additional fields. Call `addpoll <type>` with no other "
        "parameters to see the required fields for a specific type.",
        {
            {"type", RPCArg::Type::STR, RPCArg::Optional::NO,
             strprintf("Type of poll. Valid types for the active protocol version: %s.",
                       types_ss.str())},
            {"title", RPCArg::Type::STR, RPCArg::Optional::NO, "Title for the poll."},
            {"days", RPCArg::Type::NUM, RPCArg::Optional::NO, "Number of days the poll will run."},
            {"question", RPCArg::Type::STR, RPCArg::Optional::NO, "Prompt that voters shall answer."},
            {"answers", RPCArg::Type::STR, RPCArg::Optional::NO,
             "Semicolon-separated answer list (whitespace is not trimmed). "
             "Pass an empty string for yes/no/abstain response type."},
            {"weighttype", RPCArg::Type::NUM, RPCArg::Optional::NO,
             "Weighing method for the poll: 1 = Balance, 2 = Magnitude + Balance."},
            {"responsetype", RPCArg::Type::NUM, RPCArg::Optional::NO,
             "1 = yes/no/abstain, 2 = single-choice, 3 = multiple-choice."},
            {"url", RPCArg::Type::STR, RPCArg::Optional::NO, "Discussion web page URL for the poll."},
            {"required_fields", RPCArg::Type::STR, RPCArg::Optional::OMITTED,
             "Semicolon-separated name=value pairs for poll types that require additional fields "
             "(e.g. project requires project_url). Call `addpoll <type>` with no other parameters "
             "to discover the required fields for a given poll type."},
            {"fee_outpoint", RPCArg::Type::STR, RPCArg::Optional::OMITTED,
             "Optional explicit UTXO to use for the 50 GRC poll-creation fee, in "
             "\"<txid>:<vout>\" form. When omitted, the wallet picks the smallest set of "
             "largest UTXOs that covers the fee. Use this for coin-control over the fee-paying "
             "input on a wallet where the default largest-first selection picks something you'd "
             "rather keep, or when you want a specific UTXO to be the one consumed."},
        },
        RPCResult{RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "title", "Poll title."},
                {RPCResult::Type::STR_HEX, "id", "Poll transaction id (hex)."},
                {RPCResult::Type::STR, "question", "Poll question."},
                {RPCResult::Type::STR, "url", "Discussion URL."},
                {RPCResult::Type::ARR, "additional_fields", "Additional name/value pairs.",
                    {
                        {RPCResult::Type::OBJ, "", "",
                            {
                                {RPCResult::Type::STR, "name", "Field name."},
                                {RPCResult::Type::STR, "value", "Field value."},
                                {RPCResult::Type::BOOL, "required", "Whether the field is required for this poll type."},
                            }},
                    }},
                {RPCResult::Type::STR, "poll_type", "Poll type (string form)."},
                {RPCResult::Type::NUM, "poll_type_id", "Poll type (numeric enum)."},
                {RPCResult::Type::STR, "weight_type", "Weight type (string form)."},
                {RPCResult::Type::NUM, "weight_type_id", "Weight type (numeric enum)."},
                {RPCResult::Type::STR, "response_type", "Response type (string form)."},
                {RPCResult::Type::NUM, "response_type_id", "Response type (numeric enum)."},
                {RPCResult::Type::NUM, "duration_days", "Poll duration in days."},
                {RPCResult::Type::STR, "expiration", "Human-readable expiration timestamp."},
                {RPCResult::Type::STR, "timestamp", "Human-readable creation timestamp."},
                {RPCResult::Type::ARR, "choices", "Answer choices.",
                    {
                        {RPCResult::Type::OBJ, "", "",
                            {
                                {RPCResult::Type::NUM, "id", "Choice index."},
                                {RPCResult::Type::STR, "label", "Choice label."},
                            }},
                    }},
            }},
        RPCExamples{
            HelpExampleCli("addpoll",
                "survey \"Example poll\" 7 \"What do you think?\" \"yes;no;maybe\" 1 2 "
                "\"https://example.org/discussion\"")
            + HelpExampleCli("addpoll",
                "project \"Add XYZ\" 14 \"Add project XYZ?\" \"\" 1 1 "
                "\"https://example.org\" \"project_url=https://xyz.example\"")
            + HelpExampleRpc("addpoll",
                "\"survey\", \"Example poll\", 7, \"What do you think?\", \"yes;no;maybe\", 1, 2, "
                "\"https://example.org/discussion\"")
        },
    };

    // Preserve the interactive wizard: `addpoll <type>` (one arg) shows the required-fields
    // hint for that type. Fall through past the help gate and intercept after type resolution.
    if (params.size() != 1) {
        if (fHelp || !help.IsValidNumArgs(params.size())) {
            throw std::runtime_error(help.ToString());
        }
    }

    std::string type_string = ToLower(params[0].get_str());
    PollType poll_type = PollType::UNKNOWN;
    bool valid_type_parameter = false;
    for (const auto& type : valid_poll_types) {
        if (ToLower(Poll::PollTypeToString(type, false)) == type_string) {
            poll_type = type;
            valid_type_parameter = true;
            break;
        }
    }

    if (!valid_type_parameter) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
            strprintf("Invalid poll type specified. Valid types are %s.", types_ss.str()));
    }

    const std::vector<std::string>& required_fields = Poll::POLL_TYPE_RULES[(int) poll_type].m_required_fields;

    // Wizard hint: `addpoll <type>` — reframe the per-type required-fields list as a
    // validation error instead of help text. Fires before the sync check so an unsynced
    // user discovering the interface still gets the hint.
    if (params.size() == 1) {
        std::stringstream required_fields_ss;
        for (const auto& f : required_fields) {
            if (required_fields_ss.str() != std::string{}) {
                required_fields_ss << ", ";
            }
            required_fields_ss << f;
        }
        throw JSONRPCError(RPC_INVALID_PARAMETER,
            strprintf("For addpoll %s, the required fields are the following: %s.",
                      type_string,
                      required_fields.empty() ? "none" : required_fields_ss.str()));
    }

    if (!required_fields.empty() && params.size() < 9) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
            strprintf("Poll type %s requires the additional <required_fields> argument.", type_string));
    }

    if (OutOfSyncByAge()) {
        throw JSONRPCError(RPC_MISC_ERROR, "Cannot add a poll with a wallet that is not in sync.");
    }

    EnsureWalletIsUnlocked();

    PollBuilder builder;
    {
        // PollBuilder::SetPayloadVersion reads nBestHeight to validate the
        // version against the current block-version gate. Hold cs_main only
        // for that call; keep the remaining (non-chain-touching) chained
        // setters outside to bound contention.
        LOCK(cs_main);
        builder = PollBuilder().SetPayloadVersion(payload_version);
    }

    builder = std::move(builder)
        .SetType(poll_type)
        .SetTitle(params[1].get_str())
        .SetDuration(params[2].get_int())
        .SetQuestion(params[3].get_str())
        .SetWeightType(params[5].get_int() + 1)
        .SetResponseType(params[6].get_int())
        .SetUrl(params[7].get_str());

    if (!params[4].isNull() && !params[4].get_str().empty()) {
        builder = builder.SetChoices(split(params[4].get_str(), ";"));
    }

    if (params.size() >= 9 && !params[8].isNull() && !params[8].get_str().empty()) {
        std::vector<std::string> name_value_pairs = split(params[8].get_str(), ";");
        Poll::AdditionalFieldList fields;

        for (const auto& name_value_pair : name_value_pairs) {
            std::vector v_field = split(name_value_pair, "=");
            bool required = true;

            if (v_field.size() != 2) {
                throw std::runtime_error("Required fields parameter for poll is malformed.");
            }

            std::string field_name = TrimString(v_field[0]);
            std::string field_value = TrimString(v_field[1]);

            if (std::find(required_fields.begin(), required_fields.end(), field_name) == required_fields.end()) {
                required = false;
            }

            Poll::AdditionalField field(field_name, field_value, required);

            fields.Add(field);
        }

        // TODO: Extend Wellformed to do a duplicate check on the field name? This is done in the builder anyway. This
        // makes sure that at least the required fields have been provided and that they are well formed.
        if (!fields.WellFormed(poll_type)) {
            throw std::runtime_error("Required field list is malformed.");
        }

        builder = builder.AddAdditionalFields(fields);
    }

    // Optional fee_outpoint param: "<txid>:<vout>". When provided, the wallet
    // uses ONLY that outpoint for the fee-paying inputs (coin-control). When
    // omitted, the wallet picks largest-first to bound the resulting tx size.
    if (params.size() >= 10 && !params[9].isNull() && !params[9].get_str().empty()) {
        const std::string fee_outpoint_str = params[9].get_str();
        const auto colon_pos = fee_outpoint_str.find(':');
        if (colon_pos == std::string::npos) {
            throw JSONRPCError(RPC_INVALID_PARAMETER,
                "fee_outpoint must be in \"<txid>:<vout>\" form.");
        }

        const std::string txid_str = fee_outpoint_str.substr(0, colon_pos);
        const std::string vout_str = fee_outpoint_str.substr(colon_pos + 1);

        if (txid_str.size() != 64 || !IsHex(txid_str)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER,
                "fee_outpoint txid must be a 64-character hex string.");
        }

        uint32_t vout;
        if (!ParseUInt32(vout_str, &vout)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER,
                "fee_outpoint vout must be a non-negative integer.");
        }

        builder = builder.SetFeeOutpoint(COutPoint(uint256S(txid_str), vout));
    }

    std::pair<CWalletTx, std::string> result_pair;

    {
        LOCK2(cs_main, pwalletMain->cs_wallet);
        // Note that a lock on cs_poll_registry does NOT need to be taken here.
        // This lock will be taken by the contract handler.

        uint32_t contract_version = IsV13Enabled(nBestHeight) ? 3 : 2;

        result_pair = SendContract(builder.BuildContractTx(pwalletMain, contract_version));
    }

    if (!result_pair.second.empty()) {
        throw JSONRPCError(RPC_WALLET_ERROR, result_pair.second);
    }

    const CWalletTx& result_tx = result_pair.first;
    const auto payload = result_tx.vContracts[0].SharePayloadAs<PollPayload>();

    return PollToJson(payload->m_poll, result_tx.GetHash());
}

UniValue listpolls(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw std::runtime_error(
                "listpolls ( showfinished )\n"
                "\n"
                "[showfinished] -> If true, show finished polls as well.\n"
                "\n"
                "Lists poll details\n");

    UniValue json(UniValue::VARR);

    const bool active = params.size() > 0 ? !params[0].get_bool() : true;

    LOCK(GetPollRegistry().cs_poll_registry);

    for (const auto& iter : GetPollRegistry().Polls().OnlyActive(active)) {
        if (const PollOption poll = iter->TryPollFromDisk()) {
            json.push_back(PollToJson(*poll, iter.Ref()));
        }
    }

    return json;
}

UniValue getpollresults(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
                "getpollresults <poll_title_or_id>\n"
                "\n"
                "<poll_title_or_id> --> Title or ID of the poll.\n"
                "\n"
                "Display the results for the specified poll.\n"
                "\n"
                "Note that in the small chance that a blockchain reorg occurs during\n"
                "the tally for the poll, this call will return an error. Retrying\n"
                "should succeed.");

    const std::string title_or_id = params[0].get_str();

    // We only need to lock the registry to retrieve the reference. If there is a reorg during the PollResultToJson, it will
    // throw.

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"
#endif
   if (const PollReference* ref = WITH_LOCK(GetPollRegistry().cs_poll_registry, return TryPollByTitleOrId(title_or_id))) {
        return PollResultToJson(*ref);
    }
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

    throw JSONRPCError(RPC_MISC_ERROR, "No matching poll found");
}

UniValue getvotingclaim(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
                "getvotingclaim <poll_or_vote_id>\n"
                "\n"
                "<poll_or_vote_id> --> Transaction hash of the poll or vote.\n"
                "\n"
                "Display the claim for the specified poll or vote.\n");

    const uint256 id = uint256S(params[0].get_str());

    CTransaction tx;
    uint256 block_hash;

    {
        LOCK(cs_main);

        if (!GetTransaction(id, tx, block_hash)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                "Could not find a poll or vote transaction with that ID");
        }
    }

    if (tx.nVersion <= 1) {
        throw JSONRPCError(RPC_MISC_ERROR, "Legacy transaction not supported");
    }

    if (tx.GetContracts().empty()) {
        throw JSONRPCError(RPC_MISC_ERROR, "Transaction contains no contract");
    }

    if (tx.GetContracts().front().m_type == ContractType::POLL) {
        auto payload = tx.GetContracts().front().SharePayloadAs<PollPayload>();

        return PollClaimToJson(payload->m_claim);
    }

    if (tx.GetContracts().front().m_type == ContractType::VOTE) {
        auto payload = tx.GetContracts().front().SharePayloadAs<Vote>();

        return VoteClaimToJson(payload->m_claim);
    }

    throw JSONRPCError(RPC_MISC_ERROR, "Transaction contains no voting contract");
}

UniValue vote(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw std::runtime_error(
            "DEPRECATED: vote <title> <answer1;answer2...>\n"
            "\n"
            "<title> ---> Title of the poll to vote for.\n"
            "<answers> -> Labels of the choices to vote for separated by semicolons (;).\n"
            "\n"
            "Cast a vote for a poll.\n"
            "\n"
            "This RPC function is deprecated and may be removed in the future. "
            "Use \"votebyid\" instead.");

    if (OutOfSyncByAge()) {
        throw JSONRPCError(RPC_MISC_ERROR, "The wallet must be in sync to vote.");
    }

    EnsureWalletIsUnlocked();

    const std::string title = boost::to_lower_copy(params[0].get_str());

    uint256 poll_txid;
    PollOption poll;

    {
        LOCK(GetPollRegistry().cs_poll_registry);

        if (const PollReference* ref = GetPollRegistry().TryByTitle(title)) {
            poll = ref->TryReadFromDisk();
            poll_txid = ref->Txid();
        } else {
            throw JSONRPCError(RPC_MISC_ERROR, "No poll exists with that title");
        }
    }

    if (!poll) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to load poll from disk");
    }

    VoteBuilder builder = VoteBuilder::ForPoll(*poll, poll_txid)
        .SetResponses(split(params[1].get_str(), ";"));

    return SubmitVote(*poll, std::move(builder));
}

UniValue votebyid(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 2)
        throw std::runtime_error(
            "votebyid <poll_id> <choice_id_1> ( choice_id_2... )\n"
            "\n"
            "<poll_id> --------> ID of the poll to vote for.\n"
            "<choice_ids...> --> Numeric IDs of the choices to vote for.\n"
            "\n"
            "Cast a vote for a poll.\n");

    if (OutOfSyncByAge()) {
        throw JSONRPCError(RPC_MISC_ERROR, "The wallet must be in sync to vote.");
    }

    EnsureWalletIsUnlocked();

    const uint256 poll_id = uint256S(params[0].get_str());
    PollOption poll;

    {
        LOCK(GetPollRegistry().cs_poll_registry);

        if (const PollReference* ref = GetPollRegistry().TryByTxid(poll_id)) {
            poll = ref->TryReadFromDisk();
        } else {
            throw JSONRPCError(RPC_MISC_ERROR, "No poll exists for that ID");
        }
    }

    if (!poll) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to load poll from disk");
    }

    VoteBuilder builder = VoteBuilder::ForPoll(*poll, poll_id);

    for (size_t i = 1; i < params.size(); ++i) {
        builder = builder.AddResponse(params[i].get_int());
    }

    return SubmitVote(*poll, std::move(builder));
}

UniValue votedetails(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
                "votedetails <poll_title_or_id>\n"
                "\n"
                "<poll_title_or_id> --> Title or ID of the poll.\n"
                "\n"
                "Display the vote details for the specified poll.\n"
                "\n"
                "Note that in the small chance that a blockchain reorg occurs during\n"
                "the tally for the vote details, this call will return an error. Retrying\n"
                "should succeed.");

    const std::string title_or_id = params[0].get_str();

    // We only need to lock the registry to retrieve the reference. If there is a reorg during the PollResultToJson, it will
    // throw.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"
#endif
   if (const PollReference* ref = WITH_LOCK(GetPollRegistry().cs_poll_registry, return TryPollByTitleOrId(title_or_id))) {
        return VoteDetailsToJson(*ref);
    }
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

    throw JSONRPCError(RPC_MISC_ERROR, "No matching poll found");
}

UniValue testpollnotification(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1) {
        throw std::runtime_error(
                "testpollnotification <poll txid>\n"
                "\n"
                "<poll txid> --> Transaction id of the poll to test notification.\n"
                "\n"
                "Test the poll notification system.\n");
    }

    const uint256 txid = uint256S(params[0].get_str());

    const PollReference* ref = GetPollRegistry().TryByTxid(txid);

    if (!ref) {
        throw JSONRPCError(RPC_MISC_ERROR, "No poll exists for that ID");
    }

    std::string cmd = ref->Notify(PollReference::PollNotificationType::POLL_NOTIFY_TEST);

    return strprintf("Notification command: %s", cmd);
}
