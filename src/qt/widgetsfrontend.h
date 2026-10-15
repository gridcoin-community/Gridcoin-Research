// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_WIDGETSFRONTEND_H
#define BITCOIN_QT_WIDGETSFRONTEND_H

#include <qt/guifrontend.h>

#include <memory>

class BitcoinGUI;
class SplashScreen;

//! The Qt Widgets front end: BitcoinGUI as the main window and SplashScreen as
//! the splash.
class WidgetsFrontEnd final : public GuiFrontEnd
{
public:
    WidgetsFrontEnd();
    ~WidgetsFrontEnd() override;

    QObject* construct() override;
    void destroyMain() override;
    QObject* createSplash() override;
    void finishSplash() override;
    void destroySplash() override;
    void showBuildMismatchWarning(const QString& gui_commit, const QString& node_commit) override;
    void setIpcConnectionInfo(const GuiIpcInfo& info) override;
    void attachModels(const ModelBundle& models) override;
    void showMain(bool minimized) override;
    WId nativeWindowId() override;
    void requestQuit() override;

protected:
    void hideMain() override;
    void detachClient() override;
    void detachWallet() override;
    void detachMRC() override;
    void detachResearcher() override;
    void detachVoting() override;
    void detachPSGT() override;

private:
    std::unique_ptr<BitcoinGUI> m_window;
    std::unique_ptr<SplashScreen> m_splash;
};

//! The factory GuiMain takes for the Widgets front end.
std::unique_ptr<GuiFrontEnd> MakeWidgetsFrontEnd();

#endif // BITCOIN_QT_WIDGETSFRONTEND_H
