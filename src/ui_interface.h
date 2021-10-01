// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UI_INTERFACE_H
#define BITCOIN_UI_INTERFACE_H

#ifndef Q_MOC_RUN
#include <boost/signals2/last_value.hpp>
#include <boost/signals2/signal.hpp>
#endif

#include <string>
#include <stdint.h>

#include "gridcoin/scraper/fwd.h"

class CBasicKeyStore;
class CWallet;
class uint256;

/** General change type (added, updated, removed). */
enum ChangeType
{
    CT_NEW,
    CT_UPDATING,
    CT_UPDATED,
    CT_DELETED
};

/** Signals for UI communication. */
class CClientUIInterface
{
public:
    /** Flags for CClientUIInterface::ThreadSafeMessageBox */
    enum MessageBoxFlags
    {
        ICON_INFORMATION    = 0,
        ICON_WARNING        = (1U << 0),
        ICON_ERROR          = (1U << 1),
        /**
         * Mask of all available icons in CClientUIInterface::MessageBoxFlags
         * This needs to be updated, when icons are changed there!
         */
        ICON_MASK = (ICON_INFORMATION | ICON_WARNING | ICON_ERROR),

        /** These values are taken from qmessagebox.h "enum StandardButton" to be directly usable */
        BTN_OK      = 0x00000400U, // QMessageBox::Ok
        BTN_YES     = 0x00004000U, // QMessageBox::Yes
        BTN_NO      = 0x00010000U, // QMessageBox::No
        BTN_ABORT   = 0x00040000U, // QMessageBox::Abort
        BTN_RETRY   = 0x00080000U, // QMessageBox::Retry
        BTN_IGNORE  = 0x00100000U, // QMessageBox::Ignore
        BTN_CLOSE   = 0x00200000U, // QMessageBox::Close
        BTN_CANCEL  = 0x00400000U, // QMessageBox::Cancel
        BTN_DISCARD = 0x00800000U, // QMessageBox::Discard
        BTN_HELP    = 0x01000000U, // QMessageBox::Help
        BTN_APPLY   = 0x02000000U, // QMessageBox::Apply
        BTN_RESET   = 0x04000000U, // QMessageBox::Reset
        /**
         * Mask of all available buttons in CClientUIInterface::MessageBoxFlags
         * This needs to be updated, when buttons are changed there!
         */
        BTN_MASK = (BTN_OK | BTN_YES | BTN_NO | BTN_ABORT | BTN_RETRY | BTN_IGNORE |
                    BTN_CLOSE | BTN_CANCEL | BTN_DISCARD | BTN_HELP | BTN_APPLY | BTN_RESET),

        /** Force blocking, modal message box dialog (not just OS notification) */
        MODAL               = 0x10000000U,

        /** Do not print contents of message to debug log */
        SECURE              = 0x40000000U,

        /** Predefined combinations for certain default usage cases */
        MSG_INFORMATION = ICON_INFORMATION,
        MSG_WARNING = (ICON_WARNING | BTN_OK | MODAL),
        MSG_ERROR = (ICON_ERROR | BTN_OK | MODAL)
    };

    /** Show message box. */
    boost::signals2::signal<void (const std::string& message, const std::string& caption, int style)> ThreadSafeMessageBox;

    /** Update notification message box. */
    boost::signals2::signal<void (const std::string&version, const std::string& message)> UpdateMessageBox;

    /** Ask the user whether they want to pay a fee or not. */
    boost::signals2::signal<bool (int64_t nFeeRequired, const std::string& strCaption), boost::signals2::last_value<bool> > ThreadSafeAskFee;

	/** Ask the user a question */
    boost::signals2::signal<bool (std::string caption, std::string body), boost::signals2::last_value<bool> > ThreadSafeAskQuestion;

    /** Handle a URL passed at the command line. */
    boost::signals2::signal<void (const std::string& strURI)> ThreadSafeHandleURI;

    /** Progress message during initialization. */
    boost::signals2::signal<void (const std::string &message)> InitMessage;

    /** Initiate client shutdown. */
    boost::signals2::signal<void ()> QueueShutdown;

    /** Translate a message to the native language of the user. */
    boost::signals2::signal<std::string (const char* psz)> Translate;

    /** Block chain changed. */
    boost::signals2::signal<void (bool syncing, int height, int64_t best_time, uint32_t target_bits)> NotifyBlocksChanged;

    /** Number of network connections changed. */
    boost::signals2::signal<void (int newNumConnections)> NotifyNumConnectionsChanged;

    /** Ban list changed. */
    boost::signals2::signal<void ()> BannedListChanged;

    /** Miner status changed. */
    boost::signals2::signal<void (bool staking, double coin_weight)> MinerStatusChanged;

    /** Researcher context changed */
    boost::signals2::signal<void ()> ResearcherChanged;

    /** Beacon changed */
    boost::signals2::signal<void ()> BeaconChanged;

    /** New poll received **/
    boost::signals2::signal<void (int64_t poll_time)> NewPollReceived;

    /**
     * New, updated or cancelled alert.
     * @note called with lock cs_mapAlerts held.
     */
    boost::signals2::signal<void (const uint256 &hash, ChangeType status)> NotifyAlertChanged;

    /**
     * Scraper event type - new or update
     * @note called with lock cs_ConvergedScraperStatsCache held.
     */
    boost::signals2::signal<void (const scrapereventtypes& ScraperEventtype, ChangeType status, const std::string& message)> NotifyScraperEvent;
};

/** Show warning message **/
void InitWarning(const std::string& str);

/** Show error message **/
bool InitError(const std::string& str);
constexpr auto AbortError = InitError;

extern CClientUIInterface uiInterface;

/**
 * Translation function: Call Translate signal on UI interface, which returns a std::optional result.
 * If no translation slot is registered, nothing is returned, and simply return the input.
 */
inline std::string _(const char* psz)
{
    auto rv = uiInterface.Translate(psz);
    return rv ? (*rv) : psz;
}

#endif
