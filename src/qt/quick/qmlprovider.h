// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_QMLPROVIDER_H
#define BITCOIN_QT_QUICK_QMLPROVIDER_H

#include <memory>

class GuiFrontEnd;
class ShellAdapter;
class SplashAdapter;

//! The process-wide provider of the module's singletons. It owns them, so they
//! outlive any engine; the singletons' create() functions hand QML the
//! installed provider's instances with C++ ownership.
class QmlProvider
{
public:
    //! Builds the singletons. Each provider has its own marker, so a test can
    //! tell which provider a QML expression reached.
    explicit QmlProvider(GuiFrontEnd& frontend);
    //! Uninstalls itself if it is the installed provider.
    ~QmlProvider();
    QmlProvider(const QmlProvider&) = delete;
    QmlProvider& operator=(const QmlProvider&) = delete;

    //! Makes `provider` the one the singletons' create() functions use; null
    //! uninstalls. GUI thread only.
    static void install(QmlProvider* provider);
    //! The installed provider, or null.
    static QmlProvider* installed();

    ShellAdapter& shell();
    SplashAdapter& splash();
    int marker() const;

private:
    int m_marker;
    std::unique_ptr<ShellAdapter> m_shell;
    std::unique_ptr<SplashAdapter> m_splash;
};

#endif // BITCOIN_QT_QUICK_QMLPROVIDER_H
