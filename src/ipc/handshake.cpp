// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/handshake.h"

#include "chainparams.h"
#include "clientversion.h"
#include "random.h"
#include "tinyformat.h"
#include "util.h"
#include "util/strencodings.h"

#include <univalue.h>

#include <array>
#include <cstddef>
#include <fstream>
#include <sys/stat.h>

namespace ipc {
namespace {
const char* const COOKIE_FILE = "ipc.cookie";
const char* const IDENTITY_FILE = "node_identity.json";

//! Read an entire file into a string; nullopt if it cannot be opened.
std::optional<std::string> ReadFile(const fs::path& path)
{
    std::ifstream in(path.string(), std::ios::binary);
    if (!in.good()) return std::nullopt;
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return contents;
}

//! Write \p contents to \p path atomically (temp file + rename) with mode 0600.
void WriteFileAtomic(const fs::path& path, const std::string& contents)
{
    fs::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp.string(), std::ios::binary | std::ios::trunc);
        out << contents;
    }
    ::chmod(tmp.string().c_str(), 0600);
    fs::rename(tmp, path);
}

//! 16 strong random bytes as hex -- a per-datadir node identifier.
std::string GenerateNodeId()
{
    std::array<unsigned char, 16> buf{};
    GetStrongRandBytes(buf);
    return HexStr(buf);
}
} // namespace

interfaces::BuildInfo GetLocalBuildInfo()
{
    interfaces::BuildInfo info;
    info.git_commit = FormatFullVersion();
    info.built_at = __DATE__ " " __TIME__;
    info.schema_major = IPC_SCHEMA_MAJOR;
    info.schema_minor = IPC_SCHEMA_MINOR;
    info.protocol_version = IPC_PROTOCOL_VERSION;
    return info;
}

bool ConstantTimeEqual(const std::string& a, const std::string& b)
{
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    }
    return diff == 0;
}

std::string WriteCookie(const fs::path& dir)
{
    std::array<unsigned char, 32> buf{};
    GetStrongRandBytes(buf);
    const std::string cookie = HexStr(buf);
    WriteFileAtomic(dir / COOKIE_FILE, cookie);
    return cookie;
}

std::optional<std::string> ReadCookie(const fs::path& dir)
{
    auto contents = ReadFile(dir / COOKIE_FILE);
    if (!contents) return std::nullopt;
    // Trim trailing whitespace/newline defensively.
    while (!contents->empty() && (contents->back() == '\n' || contents->back() == '\r' || contents->back() == ' ')) {
        contents->pop_back();
    }
    return contents;
}

interfaces::NodeIdentity WriteIdentity(const fs::path& dir)
{
    const fs::path path = dir / IDENTITY_FILE;

    // Preserve the existing per-datadir node_id across restarts (the GUI binds to
    // it); generate one only on first run.
    std::string node_id;
    if (auto existing = ReadFile(path)) {
        UniValue obj;
        if (obj.read(*existing) && obj.isObject() && obj.exists("node_id")) {
            node_id = obj["node_id"].get_str();
        }
    }
    if (node_id.empty()) node_id = GenerateNodeId();

    interfaces::NodeIdentity id;
    id.node_id = node_id;
    id.network = Params().NetworkIDString();
    id.datadir = GetDataDir().string();
    id.genesis_hash = Params().GetConsensus().hashGenesisBlock.ToString();

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("node_id", id.node_id);
    obj.pushKV("network", id.network);
    obj.pushKV("datadir", id.datadir);
    obj.pushKV("genesis_hash", id.genesis_hash);
    WriteFileAtomic(path, obj.write(2) + "\n");
    return id;
}

std::optional<interfaces::NodeIdentity> ReadIdentity(const fs::path& dir)
{
    auto contents = ReadFile(dir / IDENTITY_FILE);
    if (!contents) return std::nullopt;
    UniValue obj;
    if (!obj.read(*contents) || !obj.isObject()) return std::nullopt;

    interfaces::NodeIdentity id;
    if (obj.exists("node_id")) id.node_id = obj["node_id"].get_str();
    if (obj.exists("network")) id.network = obj["network"].get_str();
    if (obj.exists("datadir")) id.datadir = obj["datadir"].get_str();
    if (obj.exists("genesis_hash")) id.genesis_hash = obj["genesis_hash"].get_str();
    if (id.node_id.empty()) return std::nullopt;
    return id;
}

bool ClientAuthenticateAndCheck(interfaces::Init& init, const std::string& cookie, std::string& error_out)
{
    if (!init.authenticate(cookie)) {
        error_out = "The node rejected the authentication cookie. It may have restarted since the "
                    "cookie was written; try reconnecting.";
        return false;
    }

    const interfaces::BuildInfo remote = init.getBuildInfo();
    const interfaces::BuildInfo local = GetLocalBuildInfo();

    if (remote.schema_major != local.schema_major) {
        error_out = strprintf("Incompatible IPC schema: this GUI speaks schema major %u but the node "
                              "speaks %u. Use matching builds of gridcoinresearch and "
                              "gridcoinresearchd.",
                              local.schema_major, remote.schema_major);
        return false;
    }
    if (remote.protocol_version != local.protocol_version) {
        error_out = strprintf("Incompatible IPC protocol version: GUI %u, node %u.",
                              local.protocol_version, remote.protocol_version);
        return false;
    }
    if (local.schema_minor > remote.schema_minor) {
        error_out = strprintf("This GUI is newer than the node (IPC schema minor GUI %u > node %u). "
                              "Update the node.",
                              local.schema_minor, remote.schema_minor);
        return false;
    }
    // GUI schema_minor < node: forward-compatible, accepted. A git_commit
    // mismatch is a dismissible soft-warn handled GUI-side, not a failure here.
    return true;
}

} // namespace ipc
