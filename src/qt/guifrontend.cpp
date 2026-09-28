// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/guifrontend.h>

#include <qt/guilog.h>

#include <exception>

GuiFrontEnd::~GuiFrontEnd() = default;

void GuiFrontEnd::detachModels()
{
    if (m_detached) return;
    m_detached = true;

    hideMain();
    detachClient();
    detachWallet();
    detachMRC();
    detachResearcher();
    detachVoting();
    detachPSGT();
}

FrontEndDetachGuard::~FrontEndDetachGuard()
{
    // A destructor must not throw: this one can run during stack unwinding.
    try {
        frontend.detachModels();
    } catch (const std::exception& e) {
        GUILogPrintf("WARNING: front-end detach failed: %s", e.what());
    } catch (...) {
        GUILogPrintf("WARNING: front-end detach failed");
    }
}
