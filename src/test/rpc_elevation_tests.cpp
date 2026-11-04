// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "init.h"
#include "key.h"
#include "key_io.h"
#include "rpc/client.h"
#include "rpc/protocol.h"
#include "rpc/server.h"
#include "support/allocators/secure.h"
#include "sync.h"
#include "test/staking_test_peers.h"
#include "util/strencodings.h"
#include "wallet/capability.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

#include <exception>
#include <iterator>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

using grc_test::EncryptedWallet;
using grc_test::SchedulerForThisCase;
using grc_test::ShutdownRequestedForThisCase;
using grc_test::SwapMainWallet;

//! The name of the elevation header as the server's header parser stores it.
const std::string ELEVATION_HEADER{"x-gridcoin-elevate"};

//! Run an RPC from a command line: the method, then its arguments, split at
//! spaces and converted as the CLI converts them.
UniValue CallRPC(const std::string& command)
{
    std::istringstream words(command);
    std::vector<std::string> args{std::istream_iterator<std::string>(words), std::istream_iterator<std::string>()};

    const std::string method = args.front();
    args.erase(args.begin());

    return tableRPC.execute(method, RPCConvertValues(method, args));
}

//! The RPC error code \p call throws, or nullopt when it returns. An exception
//! that is no RPC error reads as RPC_MISC_ERROR, so that a check on another
//! thread records it rather than ending the process.
template <typename Call>
std::optional<int> RpcErrorCode(Call&& call)
{
    try {
        call();
    } catch (const UniValue& error) {
        return find_value(error, "code").get_int();
    } catch (const std::exception&) {
        return RPC_MISC_ERROR;
    }

    return std::nullopt;
}

std::string Base64(const SecureString& passphrase)
{
    return EncodeBase64(reinterpret_cast<const unsigned char*>(passphrase.data()), passphrase.size());
}

//! Request headers as the server's parser leaves them, carrying \p elevation
//! as the elevation header's value.
std::map<std::string, std::string> HeadersWith(const std::string& elevation)
{
    return {{"authorization", "Basic dXNlcjpwYXNz"}, {ELEVATION_HEADER, elevation}};
}

//! A signmessage request for the key of \p id, which only a thread that may use
//! the wallet's keys can make.
std::string SignRequest(const CKeyID& id)
{
    return "signmessage " + EncodeDestination(id) + " elevation-test";
}

} // namespace

BOOST_AUTO_TEST_SUITE(rpc_elevation_tests)

BOOST_AUTO_TEST_CASE(rpc_request_without_the_header_runs_without_a_capability)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    BOOST_REQUIRE(fx.wallet.BeginIpcElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::Send));

    // The IPC client's send is in progress: it runs under the slot's
    // capability on the IPC server's thread, which this one stands for.
    CapabilityScope ipc_send(fx.wallet, fx.wallet.IpcCapability(), CapabilityKind::Send);
    BOOST_REQUIRE(ipc_send.Active());

    const std::string address = EncodeDestination(fx.id);

    // An RPC from another process meanwhile is served on another thread, and
    // is refused as if no capability were open. Boost.Test assertions are not
    // thread-safe, so the other thread only records what it saw.
    std::optional<int> send_refusal;
    std::optional<int> burn_refusal;

    std::thread rpc([&] {
        send_refusal = RpcErrorCode([&] { CallRPC("sendtoaddress " + address + " 1"); });
        burn_refusal = RpcErrorCode([&] { CallRPC("burn 1"); });
    });
    rpc.join();

    BOOST_CHECK(send_refusal == RPC_WALLET_UNLOCK_NEEDED);
    BOOST_CHECK(burn_refusal == RPC_WALLET_UNLOCK_NEEDED);

    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());

    fx.wallet.EndIpcElevation();
}

BOOST_AUTO_TEST_CASE(rpc_request_scope_executes_under_capability)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    const std::string sign = SignRequest(fx.id);

    // Without the header the request is refused, as it was before there was one.
    {
        std::map<std::string, std::string> headers{{"authorization", "Basic dXNlcjpwYXNz"}};
        const std::optional<SecureString> header = TakeElevationHeader(headers);

        BOOST_CHECK(!header.has_value());
        BOOST_CHECK_EQUAL(headers.size(), 1U);
        BOOST_CHECK(RpcErrorCode([&] {
            ExecuteWithElevation(header, [&] { return CallRPC(sign); });
        }) == RPC_WALLET_UNLOCK_NEEDED);
    }

    // With the passphrase, base64-encoded, it runs. The header is gone from the
    // headers once taken, and the rest are left.
    {
        std::map<std::string, std::string> headers = HeadersWith(Base64(EncryptedWallet::PASSPHRASE));
        const std::optional<SecureString> header = TakeElevationHeader(headers);

        BOOST_REQUIRE(header.has_value());
        BOOST_CHECK(headers.count(ELEVATION_HEADER) == 0);
        BOOST_CHECK(headers.count("authorization") == 1);

        bool could_use_keys{false};
        const UniValue signature = ExecuteWithElevation(header, [&] {
            could_use_keys = fx.wallet.CanUseKeys();
            return CallRPC(sign);
        });

        BOOST_CHECK(could_use_keys);
        BOOST_CHECK(signature.isStr() && !signature.get_str().empty());

        // The capability served this request alone: the thread may no longer
        // use the keys, and the wallet-wide unlock is as it was.
        BOOST_CHECK(!fx.wallet.CanUseKeys());
        BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
        BOOST_CHECK(RpcErrorCode([&] { CallRPC(sign); }) == RPC_WALLET_UNLOCK_NEEDED);
    }

    // A batch is authorised once, and every call in it runs under the scope.
    {
        std::map<std::string, std::string> headers = HeadersWith(Base64(EncryptedWallet::PASSPHRASE));

        const UniValue replies = ExecuteWithElevation(TakeElevationHeader(headers), [&] {
            UniValue batch(UniValue::VARR);
            batch.push_back(CallRPC(sign));
            batch.push_back(CallRPC(sign));
            return batch;
        });

        BOOST_REQUIRE(replies.isArray());
        BOOST_REQUIRE_EQUAL(replies.size(), 2U);
        BOOST_CHECK(replies[0].isStr() && replies[1].isStr());
    }

    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 0U);

    // A wrong passphrase is refused before the request runs, and counted as a
    // failed passphrase attempt.
    {
        std::map<std::string, std::string> headers = HeadersWith(Base64(SecureString("wrong")));
        bool ran{false};

        BOOST_CHECK(RpcErrorCode([&] {
            ExecuteWithElevation(TakeElevationHeader(headers), [&] {
                ran = true;
                return CallRPC(sign);
            });
        }) == RPC_WALLET_PASSPHRASE_INCORRECT);

        BOOST_CHECK(!ran);
        BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 1U);
    }

    // So is a value that is not base64 in the standard alphabet with padding.
    // No passphrase was checked, so nothing more is counted. The last value is
    // valid base64 up to a NUL byte, past which a C string would not be read.
    const std::vector<std::string> malformed{
        "not base64!",
        "c3Rha2luZw",
        "c3Rha2luZy1v_ly0",
        std::string("c3Rh\0a2luZw==", 13),
    };

    for (const std::string& value : malformed) {
        std::map<std::string, std::string> headers = HeadersWith(value);
        bool ran{false};

        BOOST_CHECK(RpcErrorCode([&] {
            ExecuteWithElevation(TakeElevationHeader(headers), [&] {
                ran = true;
                return CallRPC(sign);
            });
        }) == RPC_WALLET_PASSPHRASE_INCORRECT);

        BOOST_CHECK(!ran);
        BOOST_CHECK(headers.count(ELEVATION_HEADER) == 0);
    }

    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 1U);
}

BOOST_AUTO_TEST_CASE(elevation_header_keeps_spaces_in_the_passphrase)
{
    // Spaces at either end of a passphrase, which the header parser trims from
    // a value, survive base64.
    const SecureString spaced{"  spaced passphrase  "};

    CWallet wallet;
    SwapMainWallet swap(&wallet);

    CKey key;
    key.MakeNewKey(true);
    {
        LOCK(wallet.cs_wallet);
        BOOST_REQUIRE(wallet.AddKey(key));
    }
    BOOST_REQUIRE(wallet.EncryptWallet(spaced));
    wallet.Lock();
    BOOST_REQUIRE(wallet.Unlock(spaced, UnlockScope::StakingOnly));

    // Read through the server's own header parser, padded with whitespace as a
    // client may send it.
    std::istringstream request("Authorization: Basic dXNlcjpwYXNz\r\n"
                               "X-Gridcoin-Elevate:   " + Base64(spaced) + "  \r\n"
                               "\r\n");
    std::map<std::string, std::string> headers;
    BOOST_REQUIRE(ReadHTTPHeaders(request, headers) >= 0);
    BOOST_REQUIRE(headers.count(ELEVATION_HEADER) == 1);

    const UniValue signature = ExecuteWithElevation(TakeElevationHeader(headers), [&] {
        return CallRPC(SignRequest(key.GetPubKey().GetID()));
    });

    BOOST_CHECK(signature.isStr() && !signature.get_str().empty());
    BOOST_CHECK_EQUAL(wallet.FailedUnlockCount(), 0U);

    wallet.Lock();
}

BOOST_AUTO_TEST_CASE(elevation_ends_with_the_request)
{
    EncryptedWallet fx;
    SchedulerForThisCase scheduler;
    BOOST_REQUIRE(fx.wallet.IsLocked());

    const std::string sign = SignRequest(fx.id);

    // On a locked wallet the request unlocks it for staking only, for itself,
    // and it is locked again when the request is over.
    {
        std::map<std::string, std::string> headers = HeadersWith(Base64(EncryptedWallet::PASSPHRASE));
        bool staking_only_with_keys{false};

        const UniValue signature = ExecuteWithElevation(TakeElevationHeader(headers), [&] {
            staking_only_with_keys = fx.wallet.IsUnlockedForStakingOnly() && fx.wallet.CanUseKeys();
            return CallRPC(sign);
        });

        BOOST_CHECK(staking_only_with_keys);
        BOOST_CHECK(signature.isStr());
        BOOST_CHECK(fx.wallet.IsLocked());
    }

    // Also when the request throws.
    {
        std::map<std::string, std::string> headers = HeadersWith(Base64(EncryptedWallet::PASSPHRASE));
        bool ran{false};

        BOOST_CHECK(RpcErrorCode([&] {
            ExecuteWithElevation(TakeElevationHeader(headers), [&]() -> UniValue {
                ran = true;
                throw JSONRPCError(RPC_MISC_ERROR, "the request fails");
            });
        }) == RPC_MISC_ERROR);

        BOOST_CHECK(ran);
        BOOST_CHECK(fx.wallet.IsLocked());
        BOOST_CHECK(!fx.wallet.CanUseKeys());
    }
}

BOOST_AUTO_TEST_CASE(elevation_header_without_an_encrypted_wallet)
{
    // An unencrypted wallet has no passphrase to check, so a request with a
    // well-formed header runs as it would without one. A malformed one is
    // still refused.
    {
        CWallet plain;
        SwapMainWallet swap(&plain);

        std::map<std::string, std::string> headers = HeadersWith(Base64(SecureString("anything")));
        bool ran{false};

        ExecuteWithElevation(TakeElevationHeader(headers), [&] {
            ran = true;
            return NullUniValue;
        });
        BOOST_CHECK(ran);

        headers = HeadersWith("not base64!");
        ran = false;

        BOOST_CHECK(RpcErrorCode([&] {
            ExecuteWithElevation(TakeElevationHeader(headers), [&] {
                ran = true;
                return NullUniValue;
            });
        }) == RPC_WALLET_PASSPHRASE_INCORRECT);
        BOOST_CHECK(!ran);
    }

    // With no wallet there is nothing to authorise, and the header is ignored.
    {
        SwapMainWallet swap(nullptr);

        std::map<std::string, std::string> headers = HeadersWith("not base64!");
        const std::optional<SecureString> header = TakeElevationHeader(headers);
        BOOST_CHECK(header.has_value());
        BOOST_CHECK(headers.count(ELEVATION_HEADER) == 0);

        bool ran{false};

        ExecuteWithElevation(header, [&] {
            ran = true;
            return NullUniValue;
        });
        BOOST_CHECK(ran);
    }
}

BOOST_AUTO_TEST_CASE(batch_runs_every_call_under_one_capability)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    // A call that needs no key, one that fails after the key check passes, and
    // one that signs, in that order, as a client would send them.
    UniValue requests;
    BOOST_REQUIRE(requests.read(
        "[{\"id\": 1, \"method\": \"getblockcount\", \"params\": []},"
        " {\"id\": 2, \"method\": \"signmessage\", \"params\": [\"not-an-address\", \"elevation-test\"]},"
        " {\"id\": 3, \"method\": \"signmessage\", \"params\": [\"" + EncodeDestination(fx.id) + "\", \"elevation-test\"]}]"));

    std::map<std::string, std::string> headers = HeadersWith(Base64(EncryptedWallet::PASSPHRASE));
    const UniValue replies = ExecuteBatchWithElevation(requests, TakeElevationHeader(headers));

    BOOST_REQUIRE(replies.isArray());
    BOOST_REQUIRE_EQUAL(replies.size(), 3U);

    const auto error_code = [&](size_t i) {
        const UniValue error = find_value(replies[i], "error");
        return error.isNull() ? 0 : find_value(error, "code").get_int();
    };

    BOOST_CHECK_EQUAL(error_code(0), 0);
    BOOST_CHECK(find_value(replies[0], "result").isNum());

    // The failing call is answered with its own error...
    BOOST_CHECK_EQUAL(error_code(1), RPC_INVALID_ADDRESS_OR_KEY);

    // ...and the capability still serves the call after it.
    BOOST_CHECK_EQUAL(error_code(2), 0);
    BOOST_CHECK(find_value(replies[2], "result").isStr());

    // It ended with the batch.
    BOOST_CHECK(!fx.wallet.CanUseKeys());
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 0U);

    // A wrong passphrase refuses the whole batch before any call runs.
    headers = HeadersWith(Base64(SecureString("wrong")));
    BOOST_CHECK(RpcErrorCode([&] {
        ExecuteBatchWithElevation(requests, TakeElevationHeader(headers));
    }) == RPC_WALLET_PASSPHRASE_INCORRECT);
    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 1U);
}

BOOST_AUTO_TEST_CASE(consolidateunspent_refused_without_the_keys)
{
    EncryptedWallet fx;

    const std::string consolidate = "consolidateunspent " + EncodeDestination(fx.id);

    // Refused as the other spends are, before the wallet is searched for
    // inputs: locked, and unlocked for staking only.
    BOOST_CHECK(RpcErrorCode([&] { CallRPC(consolidate); }) == RPC_WALLET_UNLOCK_NEEDED);

    fx.UnlockFor(UnlockScope::StakingOnly);
    BOOST_CHECK(RpcErrorCode([&] { CallRPC(consolidate); }) == RPC_WALLET_UNLOCK_NEEDED);

    // A request that carries the passphrase gets past the check, to find
    // nothing to consolidate in this empty wallet.
    std::map<std::string, std::string> headers = HeadersWith(Base64(EncryptedWallet::PASSPHRASE));
    const UniValue result = ExecuteWithElevation(TakeElevationHeader(headers), [&] {
        return CallRPC(consolidate);
    });

    BOOST_REQUIRE(result.isObject());
    BOOST_CHECK(find_value(result, "result").isFalse());
}

BOOST_AUTO_TEST_CASE(passphrase_requests_during_shutdown_say_so)
{
    EncryptedWallet fx;
    // So that walletpassphrase, which asks for a running scheduler first, gets
    // as far as its passphrase attempt.
    SchedulerForThisCase scheduler;

    const SecureString& passphrase = EncryptedWallet::PASSPHRASE;
    const std::string pass(passphrase.begin(), passphrase.end());

    // Whether \p call fails with the shutdown refusal.
    const auto refused_for_shutdown = [](auto&& call) {
        try {
            call();
        } catch (const UniValue& error) {
            return find_value(error, "code").get_int() == RPC_WALLET_ERROR
                && find_value(error, "message").get_str().find("the wallet passphrase was not checked")
                       != std::string::npos;
        } catch (const std::exception&) {
        }

        return false;
    };

    // Each attempt is given up before its passphrase is checked, and every
    // RPC route says the node is shutting down, not that the passphrase was
    // wrong: walletpassphrase, walletpassphrasechange, and a request carrying
    // the passphrase in its header.
    {
        const ShutdownRequestedForThisCase shutdown;

        BOOST_CHECK(refused_for_shutdown([&] { CallRPC("walletpassphrase " + pass + " 60"); }));
        BOOST_CHECK(refused_for_shutdown([&] { CallRPC("walletpassphrasechange " + pass + " rpc-shutdown-new"); }));

        std::map<std::string, std::string> headers = HeadersWith(Base64(passphrase));
        BOOST_CHECK(refused_for_shutdown([&] {
            ExecuteWithElevation(TakeElevationHeader(headers), [&] { return CallRPC(SignRequest(fx.id)); });
        }));
    }

    // Nothing was counted or changed: the passphrase still opens the wallet.
    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 0U);
    BOOST_CHECK(fx.wallet.IsLocked());
    BOOST_CHECK(fx.wallet.Unlock(passphrase, UnlockScope::StakingOnly));
}

BOOST_AUTO_TEST_SUITE_END()
