// Copyright (c) 2017-2022 The Bitcoin Core developers
// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_RPC_UTIL_H
#define GRIDCOIN_RPC_UTIL_H

#include <rpc/protocol.h>
#include <univalue.h>
#include <util/check.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

/**
 * Ported from Bitcoin Core v26.0 (src/rpc/util.{h,cpp}) for Gridcoin issue #2922.
 *
 * This is a *reduced* port. It contains the help-text rendering and
 * argument-count validation subset of the upstream RPCHelpMan machinery. The
 * dispatch-path features (RPCHelpMan::HandleRequest, m_fun, Arg<T>/MaybeArg<T>,
 * GetArgMap, RPCArg::MatchesType, RPCResult::MatchesType) are intentionally
 * omitted — those all depend on upstream's JSONRPCRequest struct, which Gridcoin
 * does not yet have. A future PR can grow this API into the full upstream shape
 * without breaking the API surface defined here: everything declared below is a
 * strict subset of the v26.0 public surface.
 *
 * Usage in a command body (current (params, fHelp) signature):
 *
 *   UniValue getbestblockhash(const UniValue& params, bool fHelp)
 *   {
 *       static const RPCHelpMan help{
 *           "getbestblockhash", "Returns the hash of the best block in the longest block chain.",
 *           {},
 *           RPCResult{RPCResult::Type::STR_HEX, "", "the block hash, hex-encoded"},
 *           RPCExamples{HelpExampleCli("getbestblockhash", "") + HelpExampleRpc("getbestblockhash", "")}
 *       };
 *       if (fHelp || !help.IsValidNumArgs(params.size()))
 *           throw std::runtime_error(help.ToString());
 *       // ... command logic ...
 *   }
 *
 * RPCHelpMan may also be constructed per-call (not only as static const) for
 * commands whose help text or argument list depends on runtime state (e.g.
 * addkey varying by protocol version, addpoll varying by enabled poll types).
 * Construction is cheap; all fields are held by value and moved in.
 */

struct Sections;

/**
 * String used to describe UNIX epoch time in documentation, factored out to a
 * constant for consistency.
 */
extern const std::string UNIX_EPOCH_TIME;

/**
 * Example addresses for the RPCExamples help documentation. They are
 * intentionally invalid placeholder strings — Tier 1 conversion PRs should
 * replace these with real Gridcoin addresses before the first command that
 * references EXAMPLE_ADDRESS lands.
 *
 * TODO(#2922): replace with real Gridcoin example addresses.
 */
extern const std::string EXAMPLE_ADDRESS[2];

using RPCArgList = std::vector<std::pair<std::string, UniValue>>;

std::string HelpExampleCli(const std::string& methodname, const std::string& args);
std::string HelpExampleCliNamed(const std::string& methodname, const RPCArgList& args);
std::string HelpExampleRpc(const std::string& methodname, const std::string& args);
std::string HelpExampleRpcNamed(const std::string& methodname, const RPCArgList& args);

/**
 * Serializing JSON objects depends on the outer type. Only arrays and
 * dictionaries can be nested in json. The top-level outer type is "NONE".
 */
enum class OuterType {
    ARR,
    OBJ,
    NONE, // Only set on first recursion
};

struct RPCArgOptions {
    bool skip_type_check{false};
    std::string oneline_description{};   //!< Should be empty unless it is supposed to override the auto-generated summary line
    std::vector<std::string> type_str{}; //!< Should be empty unless it is supposed to override the auto-generated type strings. Vector length is either 0 or 2, m_opts.type_str.at(0) will override the type of the value in a key-value pair, m_opts.type_str.at(1) will override the type in the argument description.
    bool hidden{false};                  //!< For testing only
    bool also_positional{false};         //!< If set allows a named-parameter field in an OBJ_NAMED_PARAM options object
                                         //!< to have the same name as a top-level parameter. By default the RPC
                                         //!< framework disallows this, because if an RPC request passes the value by
                                         //!< name, it is assigned to top-level parameter position, not to the options
                                         //!< position, defeating the purpose of using OBJ_NAMED_PARAMS instead OBJ for
                                         //!< that option. But sometimes it makes sense to allow less-commonly used
                                         //!< options to be passed by name only, and more commonly used options to be
                                         //!< passed by name or position, so the RPC framework allows this as long as
                                         //!< methods set the also_positional flag and read values from both positions.
};

struct RPCArg {
    enum class Type {
        OBJ,
        ARR,
        STR,
        NUM,
        BOOL,
        OBJ_NAMED_PARAMS, //!< Special type that behaves almost exactly like
                          //!< OBJ, defining an options object with a list of
                          //!< pre-defined keys. The only difference between OBJ
                          //!< and OBJ_NAMED_PARAMS is that OBJ_NAMED_PARMS
                          //!< also allows the keys to be passed as top-level
                          //!< named parameters, as a more convenient way to pass
                          //!< options to the RPC method without nesting them.
        OBJ_USER_KEYS, //!< Special type where the user must set the keys e.g. to define multiple addresses; as opposed to e.g. an options object where the keys are predefined
        AMOUNT,        //!< Special type representing a floating point amount (can be either NUM or STR)
        STR_HEX,       //!< Special type that is a STR with only hex chars
        RANGE,         //!< Special type that is a NUM or [NUM,NUM]
    };

    enum class Optional {
        /** Required arg */
        NO,
        /**
         * Optional argument for which the default value is omitted from
         * help text for one of two reasons:
         * - It's a named argument and has a default value of `null`.
         * - Its default value is implicitly clear. That is, elements in an
         *    array may not exist by default.
         * When possible, the default value should be specified.
         */
        OMITTED,
    };
    /** Hint for default value */
    using DefaultHint = std::string;
    /** Default constant value */
    using Default = UniValue;
    using Fallback = std::variant<Optional, DefaultHint, Default>;

    const std::string m_names; //!< The name of the arg (can be empty for inner args, can contain multiple aliases separated by | for named request arguments). Aliased names ("foo|bar") are only valid on top-level args; GetName() asserts no '|' is present, and Sections::Push routes inner args through GetName() unconditionally.
    const Type m_type;
    const std::vector<RPCArg> m_inner; //!< Only used for arrays or dicts
    const Fallback m_fallback;
    const std::string m_description;
    const RPCArgOptions m_opts;

    RPCArg(
        std::string name,
        Type type,
        Fallback fallback,
        std::string description,
        RPCArgOptions opts = {})
        : m_names{std::move(name)},
          m_type{std::move(type)},
          m_fallback{std::move(fallback)},
          m_description{std::move(description)},
          m_opts{std::move(opts)}
    {
        CHECK_NONFATAL(type != Type::ARR && type != Type::OBJ && type != Type::OBJ_NAMED_PARAMS && type != Type::OBJ_USER_KEYS);
        CHECK_NONFATAL(m_opts.type_str.empty() || m_opts.type_str.size() == 2);
    }

    RPCArg(
        std::string name,
        Type type,
        Fallback fallback,
        std::string description,
        std::vector<RPCArg> inner,
        RPCArgOptions opts = {})
        : m_names{std::move(name)},
          m_type{std::move(type)},
          m_inner{std::move(inner)},
          m_fallback{std::move(fallback)},
          m_description{std::move(description)},
          m_opts{std::move(opts)}
    {
        CHECK_NONFATAL(type == Type::ARR || type == Type::OBJ || type == Type::OBJ_NAMED_PARAMS || type == Type::OBJ_USER_KEYS);
        CHECK_NONFATAL(m_opts.type_str.empty() || m_opts.type_str.size() == 2);
    }

    bool IsOptional() const;

    /** Return the first of all aliases */
    std::string GetFirstName() const;

    /** Return the name, throws when there are aliases */
    std::string GetName() const;

    /**
     * Return the type string of the argument.
     * Set oneline to allow it to be overridden by a custom oneline type string (m_opts.oneline_description).
     */
    std::string ToString(bool oneline) const;
    /**
     * Return the type string of the argument when it is in an object (dict).
     * Set oneline to get the oneline representation (less whitespace)
     */
    std::string ToStringObj(bool oneline) const;
    /**
     * Return the description string, including the argument type and whether
     * the argument is required.
     */
    std::string ToDescriptionString(bool is_named_arg) const;
};

struct RPCResult {
    enum class Type {
        OBJ,
        ARR,
        STR,
        NUM,
        BOOL,
        NONE,
        ANY,        //!< Special type to disable type checks (for testing only)
        STR_AMOUNT, //!< Special string to represent a floating point amount
        STR_HEX,    //!< Special string with only hex chars
        OBJ_DYN,    //!< Special dictionary with keys that are not literals
        ARR_FIXED,  //!< Special array that has a fixed number of entries
        NUM_TIME,   //!< Special numeric to denote unix epoch time
        ELISION,    //!< Special type to denote elision (...)
    };

    const Type m_type;
    const std::string m_key_name;         //!< Only used for dicts
    const std::vector<RPCResult> m_inner; //!< Only used for arrays or dicts
    const bool m_optional;
    const bool m_skip_type_check;
    const std::string m_description;
    const std::string m_cond;

    RPCResult(
        std::string cond,
        Type type,
        std::string m_key_name,
        bool optional,
        std::string description,
        std::vector<RPCResult> inner = {})
        : m_type{std::move(type)},
          m_key_name{std::move(m_key_name)},
          m_inner{std::move(inner)},
          m_optional{optional},
          m_skip_type_check{false},
          m_description{std::move(description)},
          m_cond{std::move(cond)}
    {
        CHECK_NONFATAL(!m_cond.empty());
        CheckInnerDoc();
    }

    RPCResult(
        std::string cond,
        Type type,
        std::string m_key_name,
        std::string description,
        std::vector<RPCResult> inner = {})
        : RPCResult{std::move(cond), type, std::move(m_key_name), /*optional=*/false, std::move(description), std::move(inner)} {}

    RPCResult(
        Type type,
        std::string m_key_name,
        bool optional,
        std::string description,
        std::vector<RPCResult> inner = {},
        bool skip_type_check = false)
        : m_type{std::move(type)},
          m_key_name{std::move(m_key_name)},
          m_inner{std::move(inner)},
          m_optional{optional},
          m_skip_type_check{skip_type_check},
          m_description{std::move(description)},
          m_cond{}
    {
        CheckInnerDoc();
    }

    RPCResult(
        Type type,
        std::string m_key_name,
        std::string description,
        std::vector<RPCResult> inner = {},
        bool skip_type_check = false)
        : RPCResult{type, std::move(m_key_name), /*optional=*/false, std::move(description), std::move(inner), skip_type_check} {}

    /** Append the sections of the result. */
    void ToSections(Sections& sections, OuterType outer_type = OuterType::NONE, const int current_indent = 0) const;
    /** Return the type string of the result when it is in an object (dict). */
    std::string ToStringObj() const;
    /** Return the description string, including the result type. */
    std::string ToDescriptionString() const;

private:
    void CheckInnerDoc() const;
};

struct RPCResults {
    const std::vector<RPCResult> m_results;

    RPCResults(RPCResult result)
        : m_results{{result}}
    {
    }

    RPCResults(std::initializer_list<RPCResult> results)
        : m_results{results}
    {
    }

    /**
     * Return the description string.
     */
    std::string ToDescriptionString() const;
};

struct RPCExamples {
    const std::string m_examples;
    explicit RPCExamples(
        std::string examples)
        : m_examples(std::move(examples))
    {
    }
    std::string ToDescriptionString() const;
};

class RPCHelpMan
{
public:
    RPCHelpMan(std::string name, std::string description, std::vector<RPCArg> args, RPCResults results, RPCExamples examples);

    std::string ToString() const;
    /** If the supplied number of args is neither too small nor too high */
    bool IsValidNumArgs(size_t num_args) const;
    //! Return list of arguments and whether they are named-only.
    std::vector<std::pair<std::string, bool>> GetArgNames() const;

    //! Mark this command as accepting any number of trailing args beyond the declared
    //! signature (i.e. the dispatcher should NOT enforce IsValidNumArgs as an upper bound).
    //! Use for commands whose semantics are variadic but whose RPCHelpMan declaration can
    //! only enumerate the typical-shape positional args. The body remains responsible for
    //! validating the actual arity it accepts. Chainable off a temporary so
    //! `static const RPCHelpMan x = RPCHelpMan{...}.MarkVariadic();` works.
    RPCHelpMan& MarkVariadic() { m_variadic = true; return *this; }
    bool IsVariadic() const { return m_variadic; }

    const std::string m_name;

private:
    const std::string m_description;
    const std::vector<RPCArg> m_args;
    const RPCResults m_results;
    const RPCExamples m_examples;
    bool m_variadic{false};
};

#endif // GRIDCOIN_RPC_UTIL_H
