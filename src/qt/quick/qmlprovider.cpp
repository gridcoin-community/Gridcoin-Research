// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/quick/qmlprovider.h>

#include <qt/quick/aboutinfo.h>
#include <qt/quick/clientstatus.h>
#include <qt/quick/clipboard.h>
#include <qt/quick/recenttransactions.h>
#include <qt/quick/researchersummary.h>
#include <qt/quick/sendadapter.h>
#include <qt/quick/shelladapter.h>
#include <qt/quick/splashadapter.h>
#include <qt/quick/votingsummary.h>
#include <qt/quick/walletopgate.h>
#include <qt/quick/walletpromptlatch.h>
#include <qt/quick/walletsummary.h>

#include <atomic>

namespace {
//! The next provider's marker; each provider takes one.
std::atomic<int> g_next_marker{1001};
//! The installed provider (GUI thread only).
QmlProvider* g_installed = nullptr;
} // namespace

QmlProvider::QmlProvider(GuiFrontEnd& frontend)
    : m_marker(g_next_marker++),
      m_shell(std::make_unique<ShellAdapter>(frontend, m_marker)),
      m_splash(std::make_unique<SplashAdapter>(m_marker))
{
    // The status adapters are built here, in the body, so one can take
    // another that is declared after it: the clipboard comes first.
    m_clipboard = std::make_unique<Clipboard>(m_marker);
    m_client_status = std::make_unique<ClientStatus>(m_marker);
    m_wallet_summary = std::make_unique<WalletSummary>(m_marker, *m_clipboard);
    m_researcher_summary = std::make_unique<ResearcherSummary>(m_marker);
    m_recent_transactions = std::make_unique<RecentTransactions>(m_marker);
    m_voting_summary = std::make_unique<VotingSummary>(m_marker);
    m_about_info = std::make_unique<AboutInfo>(m_marker);
    // The wallet-operation gate keeps a reference to the prompt latch, which
    // the wallet's other passphrase prompts take too in a later change, so
    // the latch comes first.
    m_wallet_prompt_latch = std::make_unique<WalletPromptLatch>();
    m_wallet_ops = std::make_unique<WalletOpGate>(m_marker, *m_wallet_prompt_latch);
    m_send_adapter = std::make_unique<SendAdapter>(m_marker, *m_wallet_ops);
}

QmlProvider::~QmlProvider()
{
    if (installed() == this) install(nullptr);
}

void QmlProvider::install(QmlProvider* provider)
{
    g_installed = provider;
}

QmlProvider* QmlProvider::installed()
{
    return g_installed;
}

ShellAdapter& QmlProvider::shell()
{
    return *m_shell;
}

SplashAdapter& QmlProvider::splash()
{
    return *m_splash;
}

ClientStatus& QmlProvider::clientStatus()
{
    return *m_client_status;
}

WalletSummary& QmlProvider::walletSummary()
{
    return *m_wallet_summary;
}

ResearcherSummary& QmlProvider::researcherSummary()
{
    return *m_researcher_summary;
}

RecentTransactions& QmlProvider::recentTransactions()
{
    return *m_recent_transactions;
}

VotingSummary& QmlProvider::votingSummary()
{
    return *m_voting_summary;
}

AboutInfo& QmlProvider::aboutInfo()
{
    return *m_about_info;
}

Clipboard& QmlProvider::clipboard()
{
    return *m_clipboard;
}

WalletPromptLatch& QmlProvider::walletPromptLatch()
{
    return *m_wallet_prompt_latch;
}

WalletOpGate& QmlProvider::walletOps()
{
    return *m_wallet_ops;
}

SendAdapter& QmlProvider::sendAdapter()
{
    return *m_send_adapter;
}

int QmlProvider::marker() const
{
    return m_marker;
}
