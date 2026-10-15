// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/widgetsfrontend.h>

#include <qt/bitcoingui.h>
#include <qt/splashscreen.h>

WidgetsFrontEnd::WidgetsFrontEnd() = default;

// Out of line so the unique_ptr members are destroyed where BitcoinGUI and
// SplashScreen are complete types.
WidgetsFrontEnd::~WidgetsFrontEnd() = default;

QObject* WidgetsFrontEnd::construct()
{
    m_window = std::make_unique<BitcoinGUI>();
    return m_window.get();
}

void WidgetsFrontEnd::destroyMain()
{
    m_window.reset();
}

QObject* WidgetsFrontEnd::createSplash()
{
    m_splash = std::make_unique<SplashScreen>();
    m_splash->show();
    return m_splash.get();
}

void WidgetsFrontEnd::finishSplash()
{
    if (m_splash) m_splash->finish();
}

void WidgetsFrontEnd::destroySplash()
{
    m_splash.reset();
}

void WidgetsFrontEnd::showBuildMismatchWarning(const QString& gui_commit, const QString& node_commit)
{
    m_window->showBuildMismatchWarning(gui_commit, node_commit);
}

void WidgetsFrontEnd::setIpcConnectionInfo(const GuiIpcInfo& info)
{
    m_window->setIpcConnectionInfo(info);
}

void WidgetsFrontEnd::attachModels(const ModelBundle& models)
{
    // The order StartGridcoinQt has always attached in. The PSGT context goes
    // first: setWalletModel() builds the PSGT page's interface-backed table model.
    m_window->setPSGTPoolContext(models.psgt);
    m_window->setResearcherModel(&models.researcher);
    m_window->setClientModel(&models.client);
    m_window->setWalletModel(&models.wallet);
    m_window->setMRCModel(&models.mrc);
    m_window->setVotingModel(&models.voting);
}

void WidgetsFrontEnd::showMain(bool minimized)
{
    if (minimized) {
        m_window->showMinimized();
    } else {
        m_window->show();
    }
}

WId WidgetsFrontEnd::nativeWindowId()
{
    return m_window->winId();
}

void WidgetsFrontEnd::requestQuit()
{
    BitcoinGUI::requestQuit();
}

void WidgetsFrontEnd::hideMain()
{
    if (m_window) m_window->hide();
}

void WidgetsFrontEnd::detachClient()
{
    if (m_window) m_window->setClientModel(nullptr);
}

void WidgetsFrontEnd::detachWallet()
{
    // Via BitcoinGUI -> {transactionView->setModel,
    // overviewPage->setWalletModel}(nullptr): destroys OverviewTxModel and
    // DetailedTxModel while the tx source is still alive.
    if (m_window) m_window->setWalletModel(nullptr);
}

void WidgetsFrontEnd::detachMRC()
{
    // BitcoinGUI and OverviewPage each keep a raw copy, and
    // OverviewPage::onMRCRequestClicked only checks its copy for null.
    if (m_window) m_window->setMRCModel(nullptr);
}

void WidgetsFrontEnd::detachResearcher()
{
    if (m_window) m_window->setResearcherModel(nullptr);
}

void WidgetsFrontEnd::detachVoting()
{
    // Propagates to PollTableModel::setModel(nullptr), which drains any
    // in-flight QtConcurrent refresh worker.
    if (m_window) m_window->setVotingModel(nullptr);
}

void WidgetsFrontEnd::detachPSGT()
{
    if (m_window) m_window->setPSGTPoolContext(nullptr);
}

std::unique_ptr<GuiFrontEnd> MakeWidgetsFrontEnd()
{
    return std::make_unique<WidgetsFrontEnd>();
}
