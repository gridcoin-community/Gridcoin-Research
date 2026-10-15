// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/guifrontend.h>

#include <qt/guilog.h>

#include <exception>
#include <iterator>

namespace {
//! The number of hooks GuiFrontEnd::detachModels() runs.
constexpr int DETACH_HOOK_COUNT = 7;
} // namespace

GuiFrontEnd::~GuiFrontEnd() = default;

void GuiFrontEnd::detachModels()
{
    // The established teardown order. Each entry is a virtual hook, so the call
    // below reaches the front end's override.
    using Hook = void (GuiFrontEnd::*)();
    static constexpr Hook hooks[] = {
        &GuiFrontEnd::hideMain,
        &GuiFrontEnd::detachClient,
        &GuiFrontEnd::detachWallet,
        &GuiFrontEnd::detachMRC,
        &GuiFrontEnd::detachResearcher,
        &GuiFrontEnd::detachVoting,
        &GuiFrontEnd::detachPSGT,
    };
    static_assert(std::size(hooks) == DETACH_HOOK_COUNT);

    while (m_next_hook < DETACH_HOOK_COUNT) {
        // Move past the hook BEFORE running it: a hook that throws is then never
        // re-entered, and a later call resumes at the next one.
        const Hook hook = hooks[m_next_hook];
        ++m_next_hook;
        (this->*hook)();
    }
}

bool GuiFrontEnd::detached() const
{
    return m_next_hook >= DETACH_HOOK_COUNT;
}

FrontEndDetachGuard::~FrontEndDetachGuard()
{
    // A destructor must not throw: this one can run during stack unwinding.
    // Every pass starts at least one hook, because detachModels() moves past a
    // hook before running it, so the loop ends after at most one pass per hook.
    while (!frontend.detached()) {
        try {
            frontend.detachModels();
        } catch (const std::exception& e) {
            GUILogPrintf("WARNING: front-end detach failed: %s", e.what());
        } catch (...) {
            GUILogPrintf("WARNING: front-end detach failed");
        }
    }
}
