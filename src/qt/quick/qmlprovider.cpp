// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/quick/qmlprovider.h>

#include <qt/quick/shelladapter.h>
#include <qt/quick/splashadapter.h>

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

int QmlProvider::marker() const
{
    return m_marker;
}
