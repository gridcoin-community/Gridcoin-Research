// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2012-2020 The Bitcoin developers
// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_UI_INTERFACE_H
#define BITCOIN_NODE_UI_INTERFACE_H

#include <functional>
#include <memory>
#include <string>

class CBasicKeyStore;
class CWallet;
class uint256;
enum class scrapereventtypes;

namespace boost {
namespace signals2 {
class connection;
}
} // namespace boost

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

#define ADD_SIGNALS_DECL_WRAPPER(signal_name, rtype, ...)                                  \
    rtype signal_name(__VA_ARGS__);                                                        \
    using signal_name##Sig = rtype(__VA_ARGS__);                                           \
    boost::signals2::connection signal_name##_connect(std::function<signal_name##Sig> fn);

    /** Show message box. */
    ADD_SIGNALS_DECL_WRAPPER(ThreadSafeMessageBox, void, const std::string& message, const std::string& caption, int style);

    /** Update notification message box. */
    ADD_SIGNALS_DECL_WRAPPER(UpdateMessageBox, void, const std::string& version, const int& update_type, const std::string& message);

    /** Ask the user whether they want to pay a fee or not. */
    ADD_SIGNALS_DECL_WRAPPER(ThreadSafeAskFee, bool, int64_t nFeeRequired, const std::string& strCaption);

	/** Ask the user a question */
    ADD_SIGNALS_DECL_WRAPPER(ThreadSafeAskQuestion, bool, std::string caption, std::string body);

    /** Handle a URL passed at the command line. */
    ADD_SIGNALS_DECL_WRAPPER(ThreadSafeHandleURI, void, const std::string& strURI);

    /** Progress message during initialization. */
    ADD_SIGNALS_DECL_WRAPPER(InitMessage, void, const std::string &message);

    /** Initiate client shutdown. */
    ADD_SIGNALS_DECL_WRAPPER(QueueShutdown, void);

    /** Translate a message to the native language of the user. */
    ADD_SIGNALS_DECL_WRAPPER(Translate, std::string, const char* psz);

    /** Block chain changed. */
    ADD_SIGNALS_DECL_WRAPPER(NotifyBlocksChanged, void, bool syncing, int height, int64_t best_time, uint32_t target_bits);

    /** Number of network connections changed. */
    ADD_SIGNALS_DECL_WRAPPER(NotifyNumConnectionsChanged, void, int newNumConnections);

    /** Ban list changed. */
    ADD_SIGNALS_DECL_WRAPPER(BannedListChanged, void);

    /** Miner status changed. */
    ADD_SIGNALS_DECL_WRAPPER(MinerStatusChanged, void, bool staking, double coin_weight);

    /** Researcher context changed */
    ADD_SIGNALS_DECL_WRAPPER(ResearcherChanged, void);

    /** Walletholder accrual changed as a result of stake or MRC to the walletholder */
    ADD_SIGNALS_DECL_WRAPPER(AccrualChangedFromStakeOrMRC, void);

    /** MRC state changed */
    ADD_SIGNALS_DECL_WRAPPER(MRCChanged, void);

    /** Beacon changed */
    ADD_SIGNALS_DECL_WRAPPER(BeaconChanged, void);

    /** New poll received **/
    ADD_SIGNALS_DECL_WRAPPER(NewPollReceived, void, int64_t poll_time);

    /** New vote received **/
    ADD_SIGNALS_DECL_WRAPPER(NewVoteReceived, void, const uint256& poll_txid);

    /** Read-write settings file updated **/
    ADD_SIGNALS_DECL_WRAPPER(RwSettingsUpdated, void);

    /**
     * New, updated or cancelled alert.
     * @note called with lock cs_mapAlerts held.
     */
    ADD_SIGNALS_DECL_WRAPPER(NotifyAlertChanged, void, const uint256 &hash, ChangeType status);

    /**
     * Scraper event type - new or update
     * @note called with lock cs_ConvergedScraperStatsCache held.
     */
    ADD_SIGNALS_DECL_WRAPPER(NotifyScraperEvent, void, const scrapereventtypes& ScraperEventtype, ChangeType status, const std::string& message);
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
    return uiInterface.Translate(psz);
}

#endif
