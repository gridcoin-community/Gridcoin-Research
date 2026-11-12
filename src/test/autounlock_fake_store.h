// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_AUTOUNLOCK_FAKE_STORE_H
#define GRIDCOIN_TEST_AUTOUNLOCK_FAKE_STORE_H

#include "support/allocators/secure.h"
#include "wallet/autounlock_store.h"

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace grc_test {

//! What a FakeAutoUnlockStore was asked to do, and what it keeps. Plain fields: the
//! test reads them once the calls have returned, and calls that reach the store from
//! more than one thread are serialised by the wallet's passphrase turn.
struct FakeStoreCalls
{
    int stores{0};
    int removes{0};

    //! The passphrase the last Store() was given.
    SecureString last;

    //! What is kept: set by a Store() that keeps it, emptied by a Remove() that succeeds.
    //! Status() reports it as configured.
    std::optional<SecureString> kept;

    //! Called at the start of each Store(), with its passphrase, before it is recorded.
    std::function<void(const SecureString&)> on_store;
};

//! A store that records its calls and answers as told.
class FakeAutoUnlockStore final : public AutoUnlockSecretStore
{
public:
    enum class Outcome { Succeeds, Unverified, Fails, Throws };

    FakeAutoUnlockStore(std::shared_ptr<FakeStoreCalls> calls, Outcome store_outcome, Outcome remove_outcome,
                        bool supported)
        : m_calls(std::move(calls)), m_store_outcome(store_outcome), m_remove_outcome(remove_outcome),
          m_supported(supported)
    {
    }

    AutoUnlockStoreOutcome Store(const SecureString& passphrase, std::string& error) override
    {
        if (m_calls->on_store) m_calls->on_store(passphrase);

        ++m_calls->stores;
        m_calls->last = passphrase;

        switch (m_store_outcome) {
        case Outcome::Throws:
            throw std::runtime_error("the fake store throws");
        case Outcome::Fails:
            error = "the fake store refuses";
            return AutoUnlockStoreOutcome::Failed;
        case Outcome::Unverified:
            m_calls->kept = passphrase;
            return AutoUnlockStoreOutcome::Unverified;
        case Outcome::Succeeds:
            m_calls->kept = passphrase;
            return AutoUnlockStoreOutcome::Verified;
        }

        return AutoUnlockStoreOutcome::Failed;
    }

    bool Remove(std::string& error) override
    {
        ++m_calls->removes;

        switch (m_remove_outcome) {
        case Outcome::Throws:
            throw std::runtime_error("the fake store throws");
        case Outcome::Fails:
            error = "the fake store refuses";
            return false;
        case Outcome::Unverified:
        case Outcome::Succeeds:
            m_calls->kept.reset();
            return true;
        }

        return false;
    }

    AutoUnlockStoreStatus Status() override
    {
        AutoUnlockStoreStatus status;
        status.supported = m_supported;
        status.configured = m_calls->kept.has_value();
        status.scope = "machine";
        status.location = LOCATION;
        return status;
    }

    //! Where Status() says the passphrase is kept.
    static constexpr const char* LOCATION{"fake-autounlock-store"};

private:
    std::shared_ptr<FakeStoreCalls> m_calls;
    Outcome m_store_outcome{Outcome::Succeeds};
    Outcome m_remove_outcome{Outcome::Succeeds};
    bool m_supported{true};
};

//! Installs a FakeAutoUnlockStore for one scope and restores the platform's after it.
//! \p configured starts it keeping a passphrase of its own.
struct FakeStoreInstalled
{
    std::shared_ptr<FakeStoreCalls> calls{std::make_shared<FakeStoreCalls>()};

    explicit FakeStoreInstalled(bool configured,
                                FakeAutoUnlockStore::Outcome store_outcome = FakeAutoUnlockStore::Outcome::Succeeds,
                                FakeAutoUnlockStore::Outcome remove_outcome = FakeAutoUnlockStore::Outcome::Succeeds,
                                bool supported = true)
    {
        if (configured) calls->kept = SecureString("an earlier autounlock passphrase");

        SetAutoUnlockSecretStoreForTesting(
            std::make_unique<FakeAutoUnlockStore>(calls, store_outcome, remove_outcome, supported));
    }

    ~FakeStoreInstalled()
    {
        SetAutoUnlockSecretStoreForTesting(nullptr);
    }

    FakeStoreInstalled(const FakeStoreInstalled&) = delete;
    FakeStoreInstalled& operator=(const FakeStoreInstalled&) = delete;
};

} // namespace grc_test

#endif // GRIDCOIN_TEST_AUTOUNLOCK_FAKE_STORE_H
