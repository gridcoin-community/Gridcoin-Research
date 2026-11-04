// Copyright (c) 2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_WINSHUTDOWNMONITOR_H
#define BITCOIN_QT_WINSHUTDOWNMONITOR_H

#ifdef WIN32
#include <QByteArray>
#include <QString>

#include <windef.h> // for HWND

#include <QAbstractNativeEventFilter>

namespace interfaces { class Node; }

class WinShutdownMonitor : public QAbstractNativeEventFilter
{
public:
    //! On WM_QUERYENDSESSION, BitcoinGUI::requestQuitForSessionEnd(\p node,
    //! \p multiprocess). \p node is process-lifetime (main()'s gui_node), so
    //! holding a reference is safe.
    WinShutdownMonitor(interfaces::Node& node, bool multiprocess) : m_node(node), m_multiprocess(multiprocess) {}

    /** Implements QAbstractNativeEventFilter interface for processing Windows messages */
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEventFilter(const QByteArray &eventType, void *pMessage, qintptr *pnResult);
#else
    bool nativeEventFilter(const QByteArray &eventType, void *pMessage, long *pnResult);
#endif

    /** Register the reason for blocking shutdown on Windows to allow clean client exit */
    static void registerShutdownBlockReason(const QString& strReason, const HWND& mainWinId);

private:
    interfaces::Node& m_node;
    const bool m_multiprocess;
};
#endif

#endif // BITCOIN_QT_WINSHUTDOWNMONITOR_H
