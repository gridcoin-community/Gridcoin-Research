// Copyright (c) 2017-2022 The Bitcoin Core developers
// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <rpc/util.h>

#include <tinyformat.h>
#include <univalue.h>
#include <util/check.h>
#include <util/string.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

// Ported from Bitcoin Core v26.0 (src/rpc/util.cpp). Reduced subset: help-text
// rendering and argument-count validation only. The dispatch-path code
// (RPCHelpMan::HandleRequest, Arg<T>/MaybeArg<T>, GetArgMap, MatchesType) is
// intentionally omitted — see src/rpc/util.h for rationale.

const std::string UNIX_EPOCH_TIME = "UNIX epoch time";
// TODO(#2922): replace with real Gridcoin example addresses before the first
// Tier 1 conversion PR that references EXAMPLE_ADDRESS lands.
const std::string EXAMPLE_ADDRESS[2] = {
    "S1ExampleGridcoinAddressPlaceholder1",
    "S1ExampleGridcoinAddressPlaceholder2",
};

namespace {

/**
 * Quote an argument for shell.
 *
 * @note This is intended for help, not for security-sensitive purposes.
 */
std::string ShellQuote(const std::string& s)
{
    std::string result;
    result.reserve(s.size() * 2);
    for (const char ch : s) {
        if (ch == '\'') {
            result += "'\''";
        } else {
            result += ch;
        }
    }
    return "'" + result + "'";
}

/**
 * Shell-quotes the argument if it needs quoting, else returns it literally, to save typing.
 *
 * @note This is intended for help, not for security-sensitive purposes.
 */
std::string ShellQuoteIfNeeded(const std::string& s)
{
    for (const char ch : s) {
        if (ch == ' ' || ch == '\'' || ch == '"') {
            return ShellQuote(s);
        }
    }

    return s;
}

} // namespace

std::string HelpExampleCli(const std::string& methodname, const std::string& args)
{
    return "> gridcoinresearch " + methodname + " " + args + "\n";
}

std::string HelpExampleCliNamed(const std::string& methodname, const RPCArgList& args)
{
    std::string result = "> gridcoinresearch -named " + methodname;
    for (const auto& argpair : args) {
        const auto& value = argpair.second.isStr()
                                ? argpair.second.get_str()
                                : argpair.second.write();
        result += " " + argpair.first + "=" + ShellQuoteIfNeeded(value);
    }
    result += "\n";
    return result;
}

std::string HelpExampleRpc(const std::string& methodname, const std::string& args)
{
    return "> curl --user myusername --data-binary '{\"jsonrpc\": \"1.0\", \"id\": \"curltest\", "
           "\"method\": \"" +
           methodname + "\", \"params\": [" + args + "]}' -H 'content-type: text/plain;' http://127.0.0.1:9332/\n";
}

std::string HelpExampleRpcNamed(const std::string& methodname, const RPCArgList& args)
{
    UniValue params(UniValue::VOBJ);
    for (const auto& param : args) {
        params.pushKV(param.first, param.second);
    }

    return "> curl --user myusername --data-binary '{\"jsonrpc\": \"1.0\", \"id\": \"curltest\", "
           "\"method\": \"" +
           methodname + "\", \"params\": " + params.write() + "}' -H 'content-type: text/plain;' http://127.0.0.1:9332/\n";
}

/**
 * A pair of strings that can be aligned (through padding) with other Sections
 * later on
 */
struct Section {
    Section(const std::string& left, const std::string& right)
        : m_left{left}, m_right{right} {}
    std::string m_left;
    const std::string m_right;
};

/**
 * Keeps track of RPCArgs by transforming them into sections for the purpose
 * of serializing everything to a single string
 */
struct Sections {
    std::vector<Section> m_sections;
    size_t m_max_pad{0};

    void PushSection(const Section& s)
    {
        m_max_pad = std::max(m_max_pad, s.m_left.size());
        m_sections.push_back(s);
    }

    /**
     * Recursive helper to translate an RPCArg into sections
     */
    void Push(const RPCArg& arg, const size_t current_indent = 5, const OuterType outer_type = OuterType::NONE)
    {
        const auto indent = std::string(current_indent, ' ');
        const auto indent_next = std::string(current_indent + 2, ' ');
        const bool push_name{outer_type == OuterType::OBJ}; // Dictionary keys must have a name
        const bool is_top_level_arg{outer_type == OuterType::NONE}; // True on the first recursion

        switch (arg.m_type) {
        case RPCArg::Type::STR_HEX:
        case RPCArg::Type::STR:
        case RPCArg::Type::NUM:
        case RPCArg::Type::AMOUNT:
        case RPCArg::Type::RANGE:
        case RPCArg::Type::BOOL:
        case RPCArg::Type::OBJ_NAMED_PARAMS: {
            if (is_top_level_arg) return; // Nothing more to do for non-recursive types on first recursion
            auto left = indent;
            if (arg.m_opts.type_str.size() != 0 && push_name) {
                left += "\"" + arg.GetName() + "\": " + arg.m_opts.type_str.at(0);
            } else {
                left += push_name ? arg.ToStringObj(/*oneline=*/false) : arg.ToString(/*oneline=*/false);
            }
            left += ",";
            PushSection({left, arg.ToDescriptionString(/*is_named_arg=*/push_name)});
            break;
        }
        case RPCArg::Type::OBJ:
        case RPCArg::Type::OBJ_USER_KEYS: {
            const auto right = is_top_level_arg ? "" : arg.ToDescriptionString(/*is_named_arg=*/push_name);
            PushSection({indent + (push_name ? "\"" + arg.GetName() + "\": " : "") + "{", right});
            for (const auto& arg_inner : arg.m_inner) {
                Push(arg_inner, current_indent + 2, OuterType::OBJ);
            }
            if (arg.m_type != RPCArg::Type::OBJ) {
                PushSection({indent_next + "...", ""});
            }
            PushSection({indent + "}" + (is_top_level_arg ? "" : ","), ""});
            break;
        }
        case RPCArg::Type::ARR: {
            auto left = indent;
            left += push_name ? "\"" + arg.GetName() + "\": " : "";
            left += "[";
            const auto right = is_top_level_arg ? "" : arg.ToDescriptionString(/*is_named_arg=*/push_name);
            PushSection({left, right});
            for (const auto& arg_inner : arg.m_inner) {
                Push(arg_inner, current_indent + 2, OuterType::ARR);
            }
            PushSection({indent_next + "...", ""});
            PushSection({indent + "]" + (is_top_level_arg ? "" : ","), ""});
            break;
        }
        } // no default case, so the compiler can warn about missing cases
    }

    /**
     * Concatenate all sections with proper padding
     */
    std::string ToString() const
    {
        std::string ret;
        const size_t pad = m_max_pad + 4;
        for (const auto& s : m_sections) {
            // The left part of a section is assumed to be a single line, usually it is the name of the JSON struct or a
            // brace like {, }, [, or ]
            CHECK_NONFATAL(s.m_left.find('\n') == std::string::npos);
            if (s.m_right.empty()) {
                ret += s.m_left;
                ret += "\n";
                continue;
            }

            std::string left = s.m_left;
            left.resize(pad, ' ');
            ret += left;

            // Properly pad after newlines
            std::string right;
            size_t begin = 0;
            size_t new_line_pos = s.m_right.find_first_of('\n');
            while (true) {
                right += s.m_right.substr(begin, new_line_pos - begin);
                if (new_line_pos == std::string::npos) {
                    break; // No new line
                }
                right += "\n" + std::string(pad, ' ');
                begin = s.m_right.find_first_not_of(' ', new_line_pos + 1);
                if (begin == std::string::npos) {
                    break; // Empty line
                }
                new_line_pos = s.m_right.find_first_of('\n', begin + 1);
            }
            ret += right;
            ret += "\n";
        }
        return ret;
    }
};

RPCHelpMan::RPCHelpMan(std::string name, std::string description, std::vector<RPCArg> args, RPCResults results, RPCExamples examples)
    : m_name{std::move(name)},
      m_description{std::move(description)},
      m_args{std::move(args)},
      m_results{std::move(results)},
      m_examples{std::move(examples)}
{
    // Map of parameter names and types just used to check whether the names are
    // unique. Parameter names always need to be unique, with the exception that
    // there can be pairs of POSITIONAL and NAMED parameters with the same name.
    enum ParamType { POSITIONAL = 1, NAMED = 2, NAMED_ONLY = 4 };
    std::map<std::string, int> param_names;

    for (const auto& arg : m_args) {
        std::vector<std::string> names = SplitString(arg.m_names, '|');
        // Should have unique named arguments
        for (const std::string& arg_name : names) {
            auto& param_type = param_names[arg_name];
            CHECK_NONFATAL(!(param_type & POSITIONAL));
            CHECK_NONFATAL(!(param_type & NAMED_ONLY));
            param_type |= POSITIONAL;
        }
        if (arg.m_type == RPCArg::Type::OBJ_NAMED_PARAMS) {
            for (const auto& inner : arg.m_inner) {
                std::vector<std::string> inner_names = SplitString(inner.m_names, '|');
                for (const std::string& inner_name : inner_names) {
                    auto& param_type = param_names[inner_name];
                    CHECK_NONFATAL(!(param_type & POSITIONAL) || inner.m_opts.also_positional);
                    CHECK_NONFATAL(!(param_type & NAMED));
                    CHECK_NONFATAL(!(param_type & NAMED_ONLY));
                    param_type |= inner.m_opts.also_positional ? NAMED : NAMED_ONLY;
                }
            }
        }
        // Default value type should match argument type only when defined
        if (arg.m_fallback.index() == 2) {
            const RPCArg::Type type = arg.m_type;
            switch (std::get<RPCArg::Default>(arg.m_fallback).getType()) {
            case UniValue::VOBJ:
                CHECK_NONFATAL(type == RPCArg::Type::OBJ);
                break;
            case UniValue::VARR:
                CHECK_NONFATAL(type == RPCArg::Type::ARR);
                break;
            case UniValue::VSTR:
                CHECK_NONFATAL(type == RPCArg::Type::STR || type == RPCArg::Type::STR_HEX || type == RPCArg::Type::AMOUNT);
                break;
            case UniValue::VNUM:
                CHECK_NONFATAL(type == RPCArg::Type::NUM || type == RPCArg::Type::AMOUNT || type == RPCArg::Type::RANGE);
                break;
            case UniValue::VBOOL:
                CHECK_NONFATAL(type == RPCArg::Type::BOOL);
                break;
            case UniValue::VNULL:
                // Null values are accepted in all arguments
                break;
            default:
                CHECK_NONFATAL(false);
                break;
            }
        }
    }
}

std::string RPCResults::ToDescriptionString() const
{
    std::string result;
    for (const auto& r : m_results) {
        if (r.m_type == RPCResult::Type::ANY) continue; // for testing only
        if (r.m_cond.empty()) {
            result += "\nResult:\n";
        } else {
            result += "\nResult (" + r.m_cond + "):\n";
        }
        Sections sections;
        r.ToSections(sections);
        result += sections.ToString();
    }
    return result;
}

std::string RPCExamples::ToDescriptionString() const
{
    return m_examples.empty() ? m_examples : "\nExamples:\n" + m_examples;
}

bool RPCHelpMan::IsValidNumArgs(size_t num_args) const
{
    size_t num_required_args = 0;
    for (size_t n = m_args.size(); n > 0; --n) {
        if (!m_args.at(n - 1).IsOptional()) {
            num_required_args = n;
            break;
        }
    }
    return num_required_args <= num_args && num_args <= m_args.size();
}

std::vector<std::pair<std::string, bool>> RPCHelpMan::GetArgNames() const
{
    std::vector<std::pair<std::string, bool>> ret;
    ret.reserve(m_args.size());
    for (const auto& arg : m_args) {
        if (arg.m_type == RPCArg::Type::OBJ_NAMED_PARAMS) {
            for (const auto& inner : arg.m_inner) {
                ret.emplace_back(inner.m_names, /*named_only=*/true);
            }
        }
        ret.emplace_back(arg.m_names, /*named_only=*/false);
    }
    return ret;
}

std::string RPCHelpMan::ToString() const
{
    std::string ret;

    // Oneline summary
    ret += m_name;
    bool was_optional{false};
    for (const auto& arg : m_args) {
        if (arg.m_opts.hidden) break; // Any arg that follows is also hidden
        const bool optional = arg.IsOptional();
        ret += " ";
        if (optional) {
            if (!was_optional) ret += "( ";
            was_optional = true;
        } else {
            if (was_optional) ret += ") ";
            was_optional = false;
        }
        ret += arg.ToString(/*oneline=*/true);
    }
    if (was_optional) ret += " )";

    // Description
    ret += "\n\n" + TrimString(m_description) + "\n";

    // Arguments
    Sections sections;
    Sections named_only_sections;
    for (size_t i{0}; i < m_args.size(); ++i) {
        const auto& arg = m_args.at(i);
        if (arg.m_opts.hidden) break; // Any arg that follows is also hidden

        // Push named argument name and description
        sections.m_sections.emplace_back(strprintf("%d", i + 1) + ". " + arg.GetFirstName(), arg.ToDescriptionString(/*is_named_arg=*/true));
        sections.m_max_pad = std::max(sections.m_max_pad, sections.m_sections.back().m_left.size());

        // Recursively push nested args
        sections.Push(arg);

        // Push named-only argument sections
        if (arg.m_type == RPCArg::Type::OBJ_NAMED_PARAMS) {
            for (const auto& arg_inner : arg.m_inner) {
                named_only_sections.PushSection({arg_inner.GetFirstName(), arg_inner.ToDescriptionString(/*is_named_arg=*/true)});
                named_only_sections.Push(arg_inner);
            }
        }
    }

    if (!sections.m_sections.empty()) ret += "\nArguments:\n";
    ret += sections.ToString();
    if (!named_only_sections.m_sections.empty()) ret += "\nNamed Arguments:\n";
    ret += named_only_sections.ToString();

    // Result
    ret += m_results.ToDescriptionString();

    // Examples
    ret += m_examples.ToDescriptionString();

    return ret;
}

std::string RPCArg::GetFirstName() const
{
    return m_names.substr(0, m_names.find('|'));
}

std::string RPCArg::GetName() const
{
    CHECK_NONFATAL(std::string::npos == m_names.find('|'));
    return m_names;
}

bool RPCArg::IsOptional() const
{
    if (m_fallback.index() != 0) {
        return true;
    } else {
        return RPCArg::Optional::NO != std::get<RPCArg::Optional>(m_fallback);
    }
}

std::string RPCArg::ToDescriptionString(bool is_named_arg) const
{
    std::string ret;
    ret += "(";
    if (m_opts.type_str.size() != 0) {
        ret += m_opts.type_str.at(1);
    } else {
        switch (m_type) {
        case Type::STR_HEX:
        case Type::STR: {
            ret += "string";
            break;
        }
        case Type::NUM: {
            ret += "numeric";
            break;
        }
        case Type::AMOUNT: {
            ret += "numeric or string";
            break;
        }
        case Type::RANGE: {
            ret += "numeric or array";
            break;
        }
        case Type::BOOL: {
            ret += "boolean";
            break;
        }
        case Type::OBJ:
        case Type::OBJ_NAMED_PARAMS:
        case Type::OBJ_USER_KEYS: {
            ret += "json object";
            break;
        }
        case Type::ARR: {
            ret += "json array";
            break;
        }
        } // no default case, so the compiler can warn about missing cases
    }
    if (m_fallback.index() == 1) {
        ret += ", optional, default=" + std::get<RPCArg::DefaultHint>(m_fallback);
    } else if (m_fallback.index() == 2) {
        ret += ", optional, default=" + std::get<RPCArg::Default>(m_fallback).write();
    } else {
        switch (std::get<RPCArg::Optional>(m_fallback)) {
        case RPCArg::Optional::OMITTED: {
            if (is_named_arg) ret += ", optional"; // Default value is "null" in dicts. Otherwise,
            // nothing to do. Element is treated as if not present and has no default value
            break;
        }
        case RPCArg::Optional::NO: {
            ret += ", required";
            break;
        }
        } // no default case, so the compiler can warn about missing cases
    }
    ret += ")";
    if (m_type == Type::OBJ_NAMED_PARAMS) ret += " Options object that can be used to pass named arguments, listed below.";
    ret += m_description.empty() ? "" : " " + m_description;
    return ret;
}

void RPCResult::ToSections(Sections& sections, const OuterType outer_type, const int current_indent) const
{
    // Indentation
    const std::string indent(current_indent, ' ');
    const std::string indent_next(current_indent + 2, ' ');

    // Elements in a JSON structure (dictionary or array) are separated by a comma
    const std::string maybe_separator{outer_type != OuterType::NONE ? "," : ""};

    // The key name if recursed into a dictionary
    const std::string maybe_key{
        outer_type == OuterType::OBJ ? "\"" + this->m_key_name + "\" : " : ""};

    // Format description with type
    const auto Description = [&](const std::string& type) {
        return "(" + type + (this->m_optional ? ", optional" : "") + ")" +
               (this->m_description.empty() ? "" : " " + this->m_description);
    };

    switch (m_type) {
    case Type::ELISION: {
        // If the inner result is empty, use three dots for elision
        sections.PushSection({indent + "..." + maybe_separator, m_description});
        return;
    }
    case Type::ANY: {
        CHECK_NONFATAL(false); // Only for testing
    }
    case Type::NONE: {
        sections.PushSection({indent + "null" + maybe_separator, Description("json null")});
        return;
    }
    case Type::STR: {
        sections.PushSection({indent + maybe_key + "\"str\"" + maybe_separator, Description("string")});
        return;
    }
    case Type::STR_AMOUNT: {
        sections.PushSection({indent + maybe_key + "n" + maybe_separator, Description("numeric")});
        return;
    }
    case Type::STR_HEX: {
        sections.PushSection({indent + maybe_key + "\"hex\"" + maybe_separator, Description("string")});
        return;
    }
    case Type::NUM: {
        sections.PushSection({indent + maybe_key + "n" + maybe_separator, Description("numeric")});
        return;
    }
    case Type::NUM_TIME: {
        sections.PushSection({indent + maybe_key + "xxx" + maybe_separator, Description("numeric")});
        return;
    }
    case Type::BOOL: {
        sections.PushSection({indent + maybe_key + "true|false" + maybe_separator, Description("boolean")});
        return;
    }
    case Type::ARR_FIXED:
    case Type::ARR: {
        sections.PushSection({indent + maybe_key + "[", Description("json array")});
        for (const auto& i : m_inner) {
            i.ToSections(sections, OuterType::ARR, current_indent + 2);
        }
        CHECK_NONFATAL(!m_inner.empty());
        if (m_type == Type::ARR && m_inner.back().m_type != Type::ELISION) {
            sections.PushSection({indent_next + "...", ""});
        } else {
            // Remove final comma, which would be invalid JSON
            sections.m_sections.back().m_left.pop_back();
        }
        sections.PushSection({indent + "]" + maybe_separator, ""});
        return;
    }
    case Type::OBJ_DYN:
    case Type::OBJ: {
        if (m_inner.empty()) {
            sections.PushSection({indent + maybe_key + "{}", Description("empty JSON object")});
            return;
        }
        sections.PushSection({indent + maybe_key + "{", Description("json object")});
        for (const auto& i : m_inner) {
            i.ToSections(sections, OuterType::OBJ, current_indent + 2);
        }
        if (m_type == Type::OBJ_DYN && m_inner.back().m_type != Type::ELISION) {
            // If the dictionary keys are dynamic, use three dots for continuation
            sections.PushSection({indent_next + "...", ""});
        } else {
            // Remove final comma, which would be invalid JSON
            sections.m_sections.back().m_left.pop_back();
        }
        sections.PushSection({indent + "}" + maybe_separator, ""});
        return;
    }
    } // no default case, so the compiler can warn about missing cases
    CHECK_NONFATAL(false);
}

void RPCResult::CheckInnerDoc() const
{
    if (m_type == Type::OBJ) {
        // May or may not be empty
        return;
    }
    // Everything else must either be empty or not
    const bool inner_needed{m_type == Type::ARR || m_type == Type::ARR_FIXED || m_type == Type::OBJ_DYN};
    CHECK_NONFATAL(inner_needed != m_inner.empty());
}

std::string RPCArg::ToStringObj(const bool oneline) const
{
    std::string res;
    res += "\"";
    res += GetFirstName();
    if (oneline) {
        res += "\":";
    } else {
        res += "\": ";
    }
    switch (m_type) {
    case Type::STR:
        return res + "\"str\"";
    case Type::STR_HEX:
        return res + "\"hex\"";
    case Type::NUM:
        return res + "n";
    case Type::RANGE:
        return res + "n or [n,n]";
    case Type::AMOUNT:
        return res + "amount";
    case Type::BOOL:
        return res + "bool";
    case Type::ARR:
        res += "[";
        for (const auto& i : m_inner) {
            res += i.ToString(oneline) + ",";
        }
        return res + "...]";
    case Type::OBJ:
    case Type::OBJ_NAMED_PARAMS:
    case Type::OBJ_USER_KEYS:
        // Currently unused, so avoid writing dead code
        CHECK_NONFATAL(false);
    } // no default case, so the compiler can warn about missing cases
    CHECK_NONFATAL(false);
    return res; // unreachable; silences non-void return warnings
}

std::string RPCArg::ToString(const bool oneline) const
{
    if (oneline && !m_opts.oneline_description.empty()) {
        return m_opts.oneline_description;
    }

    switch (m_type) {
    case Type::STR_HEX:
    case Type::STR: {
        return "\"" + GetFirstName() + "\"";
    }
    case Type::NUM:
    case Type::RANGE:
    case Type::AMOUNT:
    case Type::BOOL: {
        return GetFirstName();
    }
    case Type::OBJ:
    case Type::OBJ_NAMED_PARAMS:
    case Type::OBJ_USER_KEYS: {
        const std::string res = Join(m_inner, std::string(","), [&](const RPCArg& i) { return i.ToStringObj(oneline); });
        if (m_type == Type::OBJ) {
            return "{" + res + "}";
        } else {
            return "{" + res + ",...}";
        }
    }
    case Type::ARR: {
        std::string res;
        for (const auto& i : m_inner) {
            res += i.ToString(oneline) + ",";
        }
        return "[" + res + "...]";
    }
    } // no default case, so the compiler can warn about missing cases
    CHECK_NONFATAL(false);
    return {}; // unreachable; silences non-void return warnings
}
