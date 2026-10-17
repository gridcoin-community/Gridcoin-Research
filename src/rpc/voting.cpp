#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <tuple>

#include "init.h"
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
#include "util/string.h"
#include "util/time.h"

using namespace GRC;

namespace {
//! The error for a title that more than one poll has: their ids, so the caller can name one. The title is not echoed.
std::string ManyPollsError(const std::vector<const PollReference*>& polls)
{
    return strprintf("%u polls have that title. Use the poll id: %s", polls.size(),
                     Join(polls, std::string(", "), [](const PollReference* ref) { return ref->Txid().ToString(); }));
}

const PollReference* TryPollByTitleOrId(const std::string& title_or_id)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main, PollRegistry::cs_poll_registry)
{
    PollRegistry& registry = GetPollRegistry();

    if (title_or_id.size() == sizeof(uint256) * 2 && IsHex(title_or_id)) {
        const uint256 txid = uint256S(title_or_id);

        // This will return a ref to the poll in the registry if found, or, try and load it and return the ref if
        // the load is successful.
        if (const PollReference* ref = registry.TryByTxidWithAddHistoricalPollAndVotes(txid)) {
            return ref;
        }
    }

    const std::vector<const PollReference*> polls = registry.FindByTitle(GRC::PollTitleKey(title_or_id));

    if (polls.size() > 1) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, ManyPollsError(polls));
    }

    return polls.empty() ? nullptr : polls.front();
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

UniValue PollResultToJson(const PollResult& result, const PollSnapshot& snapshot)
{
    UniValue json(UniValue::VOBJ);

    json.pushKV("poll_id", snapshot.m_txid.ToString());
    json.pushKV("poll_title", snapshot.m_title);

    // Judged on the tip the result was tallied on, by its blocks, as the result's own finished flag.
    json.pushKV("poll_expired", snapshot.m_finished);

    if (snapshot.m_pindex_start) {
        json.pushKV("starting_block_height", snapshot.m_pindex_start->nHeight);
    }

    if (snapshot.m_pindex_end) {
        json.pushKV("ending_block_height", snapshot.m_pindex_end->nHeight);
    }

    json.pushKV("votes", (uint64_t)result.m_vote_count);
    json.pushKV("invalid_votes", (uint64_t)result.m_invalid_votes);
    json.pushKV("superseded_votes", (uint64_t)result.m_superseded_votes);
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

//!
//! \brief Find the poll \p title_or_id names and take its snapshot on the current tip, under cs_main and the registry
//! lock. Nothing of the registry is read after they are released.
//!
//! \throws JSONRPCError RPC_IN_WARMUP before there is a tip.
//!
std::optional<PollSnapshot> SnapshotPollByTitleOrId(const std::string& title_or_id)
{
    // cs_main before cs_poll_registry, the order block connection takes them: a txid argument may load a historical
    // poll, which needs cs_main.
    LOCK2(cs_main, PollRegistry::cs_poll_registry);

    if (!pindexBest) {
        throw JSONRPCError(RPC_IN_WARMUP, "Chain tip not available yet");
    }

    if (const PollReference* ref = TryPollByTitleOrId(title_or_id)) {
        return GetPollRegistry().Snapshot(*ref, pindexBest);
    }

    return std::nullopt;
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

//! Read a poll choice id from an RPC parameter. A vote holds each choice as
//! one byte, so an id outside 0..255 is a parameter error.
uint8_t ChoiceIdFromParam(const UniValue& param)
{
    const int choice_id = param.get_int();

    if (choice_id < 0 || choice_id > std::numeric_limits<uint8_t>::max()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("choice id %d is out of range", choice_id));
    }

    return static_cast<uint8_t>(choice_id);
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

// addpoll's help text and validation data depend on the active poll-payload
// version (v2 vs v3), which is a pure function of whether IsPollV3Enabled has
// activated at nBestHeight. The cache keys an AddPollBuild by payload_version
// so each version is constructed at most once per process lifetime; reorgs
// across the V3 boundary don't invalidate cached entries because the cached
// values (valid_poll_types, types_ss_str, the rendered help) depend only on
// payload_version, not on chain state. Dispatcher's arity pre-check is opted
// out via MarkVariadic() so the `addpoll <type>` 1-arg wizard form reaches
// the body. The body reads addpoll_build() directly to get the same
// destructured values used to construct the help, so no second cs_main
// acquisition and no recomputation of valid_poll_types or types_ss_str is
// needed.
struct AddPollBuild {
    RPCHelpMan help;
    uint32_t payload_version{0};
    std::vector<PollType> valid_poll_types;
    std::string types_ss_str;
};

namespace {
AddPollBuild make_addpoll_build(uint32_t payload_version)
{
    std::vector<PollType> valid_poll_types = GRC::PollPayload::GetValidPollTypes(payload_version);

    std::stringstream types_ss;
    for (const auto& type : valid_poll_types) {
        if (types_ss.str() != std::string{}) {
            types_ss << ", ";
        }
        types_ss << ToLower(Poll::PollTypeToString(type, false));
    }
    std::string types_ss_str = types_ss.str();

    return AddPollBuild{
        /*help=*/RPCHelpMan{
            "addpoll",
            "Add a poll to the network.\n"
            "Requires 100K GRC balance. Costs 50 GRC.\n"
            "Provide an empty string for <answers> when choosing \"yes/no/abstain\" for <responsetype>.\n"
            "Certain poll types require additional fields. Call `addpoll <type>` with no other "
            "parameters to see the required fields for a specific type.",
            std::vector<RPCArg>{
                {"type", RPCArg::Type::STR, RPCArg::Optional::NO,
                 strprintf("Type of poll. Valid types for the active protocol version: %s.",
                           types_ss_str)},
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
            }
        }.MarkVariadic(),
        /*payload_version=*/payload_version,
        /*valid_poll_types=*/std::move(valid_poll_types),
        /*types_ss_str=*/std::move(types_ss_str),
    };
}
} // namespace

const AddPollBuild& addpoll_build()
{
    static std::map<uint32_t, AddPollBuild> cache;
    static std::mutex cache_mutex;

    uint32_t payload_version = 0;
    {
        LOCK(cs_main);
        payload_version = IsPollV3Enabled(nBestHeight) ? 3 : 2;
    }

    std::lock_guard<std::mutex> guard(cache_mutex);
    auto it = cache.find(payload_version);
    if (it == cache.end()) {
        it = cache.emplace(payload_version, make_addpoll_build(payload_version)).first;
    }
    return it->second;
}

const RPCHelpMan& addpoll_helpman()
{
    return addpoll_build().help;
}

// Test/inspection accessor: render addpoll's help for an explicit payload
// version. Routes through the same make_addpoll_build() that addpoll_build()
// (and therefore both addpoll_helpman() and the addpoll body) consume, but
// without touching cs_main/nBestHeight — letting tests exercise the v2/v3
// fork flip deterministically. See declaration in rpc/server.h.
std::string addpoll_help_for_version(uint32_t payload_version)
{
    return make_addpoll_build(payload_version).help.ToString();
}

UniValue addpoll(const UniValue& params)
{
    // The accessor returns a cached AddPollBuild whose payload_version /
    // valid_poll_types / types_ss_str were derived from a single cs_main
    // acquisition at construction time. The body destructures those rather
    // than re-acquiring the lock or recomputing — eliminates the prior
    // accessor-vs-body TOCTOU window across a V3-activation boundary.
    const AddPollBuild& build = addpoll_build();
    const RPCHelpMan& help = build.help;
    const uint32_t payload_version = build.payload_version;
    const std::vector<PollType>& valid_poll_types = build.valid_poll_types;
    const std::string& types_ss_str = build.types_ss_str;

    // The dispatcher's arity pre-check skips addpoll because its helpman is
    // marked variadic (see make_addpoll_build) — the wizard interaction
    // `addpoll <type>` with one arg has to reach this body. Enforce arity
    // here for non-wizard calls: allow 1 arg (wizard path below) or whatever
    // the helpman declares (8 or 9 args).
    if (params.size() != 1 && !help.IsValidNumArgs(params.size())) {
        throw std::runtime_error(help.ToString());
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
            strprintf("Invalid poll type specified. Valid types are %s.", types_ss_str));
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

static const RPCHelpMan listpolls_help{
    "listpolls",
    "Lists poll details for all currently active polls (or for all polls if showfinished is true).",
    {
        {"showfinished", RPCArg::Type::BOOL, RPCArg::Optional::OMITTED,
            "If true, show finished polls as well. Default: false."},
    },
    RPCResult{RPCResult::Type::ARR, "", "",
        {
            {RPCResult::Type::ELISION, "", "Poll detail object; see 'getpollresults' / 'addpoll' for shape."},
        }},
    RPCExamples{
        HelpExampleCli("listpolls", "") +
        HelpExampleCli("listpolls", "true") +
        HelpExampleRpc("listpolls", "true")},
};
const RPCHelpMan& listpolls_helpman() { return listpolls_help; }

UniValue listpolls(const UniValue& params)
{
    UniValue json(UniValue::VARR);

    const bool active = params.size() > 0 ? !params[0].get_bool() : true;

    // Which polls are open is judged by blocks on one tip, as the poll table and getpollresults judge it,
    // so the RPCs agree. cs_main is taken before cs_poll_registry and both are held while the polls are judged,
    // so no reorg lands in between (the snapshot's off-main-chain throw cannot fire here). The polls are read from
    // disk after the locks are released.
    std::vector<std::tuple<uint256, uint64_t, int64_t>> wanted; // Poll txid, vote count and start.
    {
        LOCK2(cs_main, PollRegistry::cs_poll_registry);

        if (!pindexBest) {
            return json;
        }

        PollRegistry& registry = GetPollRegistry();

        for (const auto& iter : registry.Polls()) {
            const PollSnapshot snapshot = registry.Snapshot(iter->Ref(), pindexBest);

            if (!active || !snapshot.m_finished) {
                wanted.emplace_back(snapshot.m_txid, snapshot.m_votes.size(), snapshot.m_start_time);
            }
        }
    }

    CTxDB txdb("r");

    for (const auto& [txid, votes, start_time] : wanted) {
        if (PollOption poll = ReadPollFromDisk(txdb, txid)) {
            // The poll's start, which from v15 is its block's time.
            poll->m_timestamp = start_time;

            UniValue entry = PollToJson(*poll, txid);
            entry.pushKV("votes", votes);
            json.push_back(entry);
        }
    }

    return json;
}

static const RPCHelpMan getpollresults_help{
    "getpollresults",
    "Display the results for the specified poll.\n"
    "\n"
    "Note that in the small chance that a blockchain reorg occurs during the tally for the poll,\n"
    "this call will return an error. Retrying should succeed.",
    {
        {"poll_title_or_id", RPCArg::Type::STR, RPCArg::Optional::NO, "Title or ID of the poll."},
    },
    RPCResult{RPCResult::Type::OBJ, "", "",
        {{RPCResult::Type::ELISION, "", "Poll result object; see source (PollResultToJson) for the exact shape."}}},
    RPCExamples{
        HelpExampleCli("getpollresults", "\"Example Poll\"") +
        HelpExampleRpc("getpollresults", "\"Example Poll\"")},
};
const RPCHelpMan& getpollresults_helpman() { return getpollresults_help; }

UniValue getpollresults(const UniValue& params)
{
    const std::string title_or_id = params[0].get_str();

    // The lookup and the snapshot are taken under the locks; the tally runs without them and throws on a reorg.

    // Mark a traversal in progress for the whole call (RAII, cleared on every return/throw below); the count-based
    // scope keeps the reorg detector armed even if this overlaps a GUI table build.
    PollRegistry::TraversalScope traversal(GetPollRegistry());

    try {
        if (const std::optional<PollSnapshot> snapshot = SnapshotPollByTitleOrId(title_or_id)) {
            if (const PollResultOption result = PollResult::BuildFor(*snapshot)) {
                return PollResultToJson(*result, *snapshot);
            }

            throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to load poll from disk");
        }
    } catch (InvalidDuetoReorgFork& e) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to load poll from disk due to reorg in progress during inquiry.");
    }

    throw JSONRPCError(RPC_MISC_ERROR, "No matching poll found");
}

static const RPCHelpMan getvotingclaim_help{
    "getvotingclaim",
    "Display the claim for the specified poll or vote.",
    {
        {"poll_or_vote_id", RPCArg::Type::STR_HEX, RPCArg::Optional::NO,
            "Transaction hash of the poll or vote."},
    },
    RPCResult{RPCResult::Type::OBJ, "", "",
        {{RPCResult::Type::ELISION, "",
            "Claim object; shape depends on whether the transaction is a poll (PollClaimToJson) or vote (VoteClaimToJson)."}}},
    RPCExamples{
        HelpExampleCli("getvotingclaim", "\"<txid>\"") +
        HelpExampleRpc("getvotingclaim", "\"<txid>\"")},
};
const RPCHelpMan& getvotingclaim_helpman() { return getvotingclaim_help; }

UniValue getvotingclaim(const UniValue& params)
{
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

static const RPCHelpMan vote_help{
    "vote",
    "DEPRECATED. Use votebyid instead. This RPC may be removed in the future.\n"
    "\n"
    "Cast a vote for a poll, identified by title.",
    {
        {"title", RPCArg::Type::STR, RPCArg::Optional::NO, "Title of the poll to vote for."},
        {"answers", RPCArg::Type::STR, RPCArg::Optional::NO,
            "Labels of the choices to vote for, separated by semicolons (;)."},
    },
    RPCResult{RPCResult::Type::OBJ, "", "",
        {{RPCResult::Type::ELISION, "",
            "Vote submission detail; see source (SubmitVote) - includes poll, vote_txid, and responses."}}},
    RPCExamples{
        HelpExampleCli("vote", "\"Example Poll\" \"yes\"") +
        HelpExampleRpc("vote", "\"Example Poll\", \"yes\"")},
};
const RPCHelpMan& vote_helpman() { return vote_help; }

UniValue vote(const UniValue& params)
{
    if (OutOfSyncByAge()) {
        throw JSONRPCError(RPC_MISC_ERROR, "The wallet must be in sync to vote.");
    }

    EnsureWalletIsUnlocked();

    const std::string title = GRC::PollTitleKey(params[0].get_str());

    uint256 poll_txid;
    PollOption poll;

    {
        LOCK(GRC::PollRegistry::cs_poll_registry);

        const std::vector<const PollReference*> polls = GetPollRegistry().FindByTitle(title);

        if (polls.size() > 1) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, ManyPollsError(polls));
        }

        if (polls.empty()) {
            throw JSONRPCError(RPC_MISC_ERROR, "No poll exists with that title");
        }

        poll = polls.front()->TryReadFromDisk();
        poll_txid = polls.front()->Txid();
    }

    if (!poll) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to load poll from disk");
    }

    VoteBuilder builder = VoteBuilder::ForPoll(*poll, poll_txid)
        .SetResponses(split(params[1].get_str(), ";"));

    return SubmitVote(*poll, std::move(builder));
}

// Variadic: the declared signature is the typical two-arg shape, but
// multi-choice polls accept additional positional choice_id values. The
// body validates the actual arity; MarkVariadic() opts the command out
// of the dispatcher's IsValidNumArgs pre-check so the trailing choices
// are accepted.
static const RPCHelpMan votebyid_help = RPCHelpMan{
    "votebyid",
    "Cast a vote for a poll.",
    {
        {"poll_id", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "ID (transaction hash) of the poll to vote for."},
        {"choice_id", RPCArg::Type::NUM, RPCArg::Optional::NO,
            "Numeric ID of a choice to vote for. Pass additional positional choice IDs for multi-choice polls."},
    },
    RPCResult{RPCResult::Type::OBJ, "", "",
        {{RPCResult::Type::ELISION, "",
            "Vote submission detail; see source (SubmitVote) - includes poll, vote_txid, and responses fields."}}},
    RPCExamples{
        HelpExampleCli("votebyid", "\"<poll_txid>\" 0") +
        HelpExampleCli("votebyid", "\"<poll_txid>\" 0 1") +
        HelpExampleRpc("votebyid", "\"<poll_txid>\", 0, 1")},
}.MarkVariadic();
const RPCHelpMan& votebyid_helpman() { return votebyid_help; }

UniValue votebyid(const UniValue& params)
{
    // Variadic positional: at least one choice_id is required (legacy minimum was 2 args total).
    // RPCHelpMan does not model unbounded variadic, so retain a body-level lower-bound check
    // after the dispatcher has handled the help-rendering and arity-upper-bound paths.
    if (params.size() < 2)
        throw std::runtime_error(votebyid_helpman().ToString());

    if (OutOfSyncByAge()) {
        throw JSONRPCError(RPC_MISC_ERROR, "The wallet must be in sync to vote.");
    }

    EnsureWalletIsUnlocked();

    const uint256 poll_id = uint256S(params[0].get_str());
    PollOption poll;

    {
        LOCK(GRC::PollRegistry::cs_poll_registry);

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
        builder = builder.AddResponse(ChoiceIdFromParam(params[i]));
    }

    return SubmitVote(*poll, std::move(builder));
}

static const RPCHelpMan removepoll_help = RPCHelpMan{
    "removepoll",
    "Remove a poll registered since block version 11, by its id, from block version 15. Requires the master key. "
    "The poll keeps its title.",
    {
        {"poll_id", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "ID (transaction hash) of the poll to remove."},
    },
    RPCResult{RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::STR_HEX, "poll_id", "The removed poll's id."},
            {RPCResult::Type::STR_HEX, "txid", "The removal's transaction id."},
        }},
    RPCExamples{
        HelpExampleCli("removepoll", "\"<poll_txid>\"") +
        HelpExampleRpc("removepoll", "\"<poll_txid>\"")},
};
const RPCHelpMan& removepoll_helpman() { return removepoll_help; }

UniValue removepoll(const UniValue& params)
{
    EnsureWalletIsUnlocked();

    const uint256 poll_txid = uint256S(params[0].get_str());
    GRC::Contract contract;

    {
        LOCK(cs_main);

        // A REMOVE names its poll by id from block version 15; below it the network accepts only the older
        // form, which names a title.
        if (!IsV15Enabled(nBestHeight + 1)) {
            throw JSONRPCError(RPC_MISC_ERROR, "Polls can be removed by id from block version 15.");
        }

        LOCK(GRC::PollRegistry::cs_poll_registry);

        // Only an ACTIVE poll the registry stores (registered since V11); a pre-V11 poll is never removed.
        if (!GetPollRegistry().TryByTxid(poll_txid) || !GetPollRegistry().IsStored(poll_txid)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "No poll registered since block version 11 has that id.");
        }

        contract = GRC::MakeContract<GRC::PollPayload>(
            uint32_t {3}, GRC::ContractAction::REMOVE, GRC::PollPayload::REMOVE_BY_TXID_VERSION, poll_txid);
    }

    std::pair<CWalletTx, std::string> result;
    {
        LOCK(cs_main);
        result = GRC::SendContract(contract);
    }

    if (!result.second.empty()) {
        throw JSONRPCError(RPC_WALLET_ERROR, result.second);
    }

    UniValue res(UniValue::VOBJ);
    res.pushKV("poll_id", poll_txid.ToString());
    res.pushKV("txid", result.first.GetHash().ToString());

    return res;
}

static const RPCHelpMan votewithclaim_help{
    "votewithclaim",
    "Regtest only. Cast a vote whose balance claim lists exactly the given outputs, for\n"
    "testing the tally. It builds a balance claim only, with no magnitude claim, even on a\n"
    "poll weighted by magnitude.",
    {
        {"poll_id", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "ID (transaction hash) of the poll to vote for."},
        {"choice_ids", RPCArg::Type::ARR, RPCArg::Optional::NO, "Numeric IDs of the choices to vote for (0 to 255).",
            {
                {"choice_id", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, ""},
            }},
        {"outpoints", RPCArg::Type::ARR, RPCArg::Optional::NO, "Wallet outputs to claim.",
            {
                {"", RPCArg::Type::OBJ, RPCArg::Optional::OMITTED, "",
                    {
                        {"txid", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The transaction id"},
                        {"vout", RPCArg::Type::NUM, RPCArg::Optional::NO, "The output number"},
                    }},
            }},
        {"fee_outpoint", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "\"<txid>:<vout>\" to pay the fee from."},
    },
    RPCResult{RPCResult::Type::OBJ, "", "",
        {{RPCResult::Type::ELISION, "", "Vote submission detail; see source (SubmitVote)."}}},
    RPCExamples{
        HelpExampleCli("votewithclaim", "\"<poll_txid>\" \"[0]\" \"[{\\\"txid\\\":\\\"<txid>\\\",\\\"vout\\\":0}]\"")},
};
const RPCHelpMan& votewithclaim_helpman() { return votewithclaim_help; }

UniValue votewithclaim(const UniValue& params)
{
    if (!Params().IsMockableChain()) {
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "votewithclaim is available on regtest only");
    }

    EnsureWalletIsUnlocked();

    const uint256 poll_id = uint256S(params[0].get_str());
    PollOption poll;

    {
        LOCK(GRC::PollRegistry::cs_poll_registry);

        if (const PollReference* ref = GetPollRegistry().TryByTxid(poll_id)) {
            poll = ref->TryReadFromDisk();
        } else {
            throw JSONRPCError(RPC_MISC_ERROR, "No poll exists for that ID");
        }
    }

    if (!poll) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to load poll from disk");
    }

    std::vector<COutPoint> outpoints;

    for (const UniValue& entry : params[2].get_array().getValues()) {
        const uint256 txid = ParseHashO(entry, "txid");
        const int vout = find_value(entry.get_obj(), "vout").get_int();

        if (vout < 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "vout must be non-negative");
        }

        outpoints.emplace_back(txid, vout);
    }

    std::optional<COutPoint> fee_outpoint;

    if (params.size() > 3 && !params[3].isNull()) {
        const std::string& fee = params[3].get_str();
        const auto colon = fee.find(':');
        uint32_t vout = 0;

        if (colon == std::string::npos || !ParseUInt32(fee.substr(colon + 1), &vout)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "fee_outpoint must be \"<txid>:<vout>\"");
        }

        fee_outpoint = COutPoint(ParseHashV(UniValue(fee.substr(0, colon)), "fee_outpoint txid"), vout);
    }

    try {
        VoteBuilder builder = VoteBuilder::ForPoll(*poll, poll_id);

        for (const UniValue& choice : params[1].get_array().getValues()) {
            const int choice_id = choice.get_int();

            if (choice_id < 0 || choice_id > std::numeric_limits<uint8_t>::max()) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("choice id %d is out of range", choice_id));
            }

            builder = builder.AddResponse(static_cast<uint8_t>(choice_id));
        }

        builder = builder.SetClaimOutpoints(std::move(outpoints));

        if (fee_outpoint) {
            builder = builder.SetFeeOutpoint(*fee_outpoint);
        }

        return SubmitVote(*poll, std::move(builder));
    } catch (const VotingError& e) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, e.what());
    }
}

static const RPCHelpMan votedetails_help{
    "votedetails",
    "Display the vote details for the specified poll.\n"
    "\n"
    "Note that in the small chance that a blockchain reorg occurs during the tally for the vote details,\n"
    "this call will return an error. Retrying should succeed.",
    {
        {"poll_title_or_id", RPCArg::Type::STR, RPCArg::Optional::NO, "Title or ID of the poll."},
    },
    RPCResult{RPCResult::Type::OBJ, "", "",
        {{RPCResult::Type::ELISION, "", "Vote details object; see source (VoteDetailsToJson) for shape."}}},
    RPCExamples{
        HelpExampleCli("votedetails", "\"Example Poll\"") +
        HelpExampleRpc("votedetails", "\"Example Poll\"")},
};
const RPCHelpMan& votedetails_helpman() { return votedetails_help; }

UniValue votedetails(const UniValue& params)
{
    const std::string title_or_id = params[0].get_str();

    // The lookup and the snapshot are taken under the locks; the tally runs without them and throws on a reorg.

    // Mark a traversal in progress for the whole call (RAII, cleared on every return/throw below); the count-based
    // scope keeps the reorg detector armed even if this overlaps a GUI table build.
    PollRegistry::TraversalScope traversal(GetPollRegistry());

    try {
        if (const std::optional<PollSnapshot> snapshot = SnapshotPollByTitleOrId(title_or_id)) {
            if (const PollResultOption result = PollResult::BuildFor(*snapshot)) {
                return VoteDetailsToJson(*result);
            }

            throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to load poll from disk.");
        }
    } catch (InvalidDuetoReorgFork& e) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to load poll from disk due to reorg in progress during inquiry.");
    }

    throw JSONRPCError(RPC_MISC_ERROR, "No matching poll found");
}

static const RPCHelpMan testpollnotification_help{
    "testpollnotification",
    "Test the poll notification system for the specified poll. The reply shows the -pollnotify command, a\n"
    "sensitive setting, as ****, as listsettings does.",
    {
        {"poll_txid", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "Transaction ID of the poll to test notification."},
    },
    RPCResult{RPCResult::Type::NONE, "", ""},
    RPCExamples{
        HelpExampleCli("testpollnotification", "\"<txid>\"") +
        HelpExampleRpc("testpollnotification", "\"<txid>\"")},
};
const RPCHelpMan& testpollnotification_helpman() { return testpollnotification_help; }

UniValue testpollnotification(const UniValue& params)
{
    const uint256 txid = uint256S(params[0].get_str());

    // The lock covers the lookup AND the use. TryByTxid returns a raw pointer
    // into the registry's guarded map, so without it a block disconnect carrying
    // a poll contract (PollRegistry::Delete, on the validation thread) could erase
    // the entry between the lookup and the Notify below, leaving this thread to
    // dereference freed memory. Every other registry lookup in this file already
    // holds it; this one was missed, and could not be caught by the analyzer until
    // the lock requirements moved onto the declarations in registry.h.
    LOCK(GRC::PollRegistry::cs_poll_registry);

    const PollReference* ref = GetPollRegistry().TryByTxid(txid);

    if (!ref) {
        throw JSONRPCError(RPC_MISC_ERROR, "No poll exists for that ID");
    }

    const std::string cmd = ref->Notify(PollReference::PollNotificationType::POLL_NOTIFY_TEST);

    // -pollnotify is a sensitive setting, which may hold a token: like listsettings and the startup
    // log, the reply says whether one is set, not what it is.
    return strprintf("Notification command: %s", cmd.empty() ? "" : "****");
}
