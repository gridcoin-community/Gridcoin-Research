// Copyright (c) 2010-2020 The Bitcoin Core developers
// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "uint256.h"
#include <node/ui_interface.h>

#include <boost/signals2/optional_last_value.hpp>
#include <boost/signals2/signal.hpp>


CClientUIInterface uiInterface;


struct UISignals {
    boost::signals2::signal<CClientUIInterface::ThreadSafeMessageBoxSig> ThreadSafeMessageBox;
    boost::signals2::signal<CClientUIInterface::ThreadSafeAskQuestionSig> ThreadSafeAskQuestion;
    boost::signals2::signal<CClientUIInterface::InitMessageSig> InitMessage;
    boost::signals2::signal<CClientUIInterface::NotifyNumConnectionsChangedSig> NotifyNumConnectionsChanged;
    boost::signals2::signal<CClientUIInterface::NotifyAlertChangedSig> NotifyAlertChanged;
    boost::signals2::signal<CClientUIInterface::BannedListChangedSig> BannedListChanged;
    boost::signals2::signal<CClientUIInterface::MinerStatusChangedSig> MinerStatusChanged;
    boost::signals2::signal<CClientUIInterface::ResearcherChangedSig> ResearcherChanged;
    boost::signals2::signal<CClientUIInterface::AccrualChangedFromStakeOrMRCSig> AccrualChangedFromStakeOrMRC;
    boost::signals2::signal<CClientUIInterface::MRCChangedSig> MRCChanged;
    boost::signals2::signal<CClientUIInterface::BeaconChangedSig> BeaconChanged;
    boost::signals2::signal<CClientUIInterface::NewPollReceivedSig> NewPollReceived;
    boost::signals2::signal<CClientUIInterface::NewVoteReceivedSig> NewVoteReceived;
    boost::signals2::signal<CClientUIInterface::NotifyScraperEventSig> NotifyScraperEvent;
    boost::signals2::signal<CClientUIInterface::ThreadSafeAskFeeSig> ThreadSafeAskFee;
    boost::signals2::signal<CClientUIInterface::ThreadSafeHandleURISig> ThreadSafeHandleURI;
    boost::signals2::signal<CClientUIInterface::QueueShutdownSig> QueueShutdown;
    boost::signals2::signal<CClientUIInterface::TranslateSig> Translate;
    boost::signals2::signal<CClientUIInterface::NotifyBlocksChangedSig> NotifyBlocksChanged;
    boost::signals2::signal<CClientUIInterface::UpdateMessageBoxSig> UpdateMessageBox;
    boost::signals2::signal<CClientUIInterface::RwSettingsUpdatedSig> RwSettingsUpdated;
};
static UISignals g_ui_signals;

#define ADD_SIGNALS_IMPL_WRAPPER(signal_name)                                                                 \
    boost::signals2::connection CClientUIInterface::signal_name##_connect(std::function<signal_name##Sig> fn) \
    {                                                                                                         \
        return g_ui_signals.signal_name.connect(fn);                                                          \
    }

ADD_SIGNALS_IMPL_WRAPPER(ThreadSafeMessageBox);
ADD_SIGNALS_IMPL_WRAPPER(ThreadSafeAskQuestion);
ADD_SIGNALS_IMPL_WRAPPER(InitMessage);
ADD_SIGNALS_IMPL_WRAPPER(NotifyNumConnectionsChanged);
ADD_SIGNALS_IMPL_WRAPPER(NotifyAlertChanged);
ADD_SIGNALS_IMPL_WRAPPER(BannedListChanged);
ADD_SIGNALS_IMPL_WRAPPER(MinerStatusChanged);
ADD_SIGNALS_IMPL_WRAPPER(ResearcherChanged);
ADD_SIGNALS_IMPL_WRAPPER(AccrualChangedFromStakeOrMRC);
ADD_SIGNALS_IMPL_WRAPPER(MRCChanged);
ADD_SIGNALS_IMPL_WRAPPER(BeaconChanged);
ADD_SIGNALS_IMPL_WRAPPER(NewPollReceived);
ADD_SIGNALS_IMPL_WRAPPER(NewVoteReceived);
ADD_SIGNALS_IMPL_WRAPPER(NotifyScraperEvent);
ADD_SIGNALS_IMPL_WRAPPER(ThreadSafeAskFee);
ADD_SIGNALS_IMPL_WRAPPER(ThreadSafeHandleURI);
ADD_SIGNALS_IMPL_WRAPPER(QueueShutdown);
ADD_SIGNALS_IMPL_WRAPPER(Translate);
ADD_SIGNALS_IMPL_WRAPPER(NotifyBlocksChanged);
ADD_SIGNALS_IMPL_WRAPPER(UpdateMessageBox);
ADD_SIGNALS_IMPL_WRAPPER(RwSettingsUpdated);

void CClientUIInterface::ThreadSafeMessageBox(const std::string& message, const std::string& caption, int style) { return g_ui_signals.ThreadSafeMessageBox(message, caption, style); }
void CClientUIInterface::UpdateMessageBox(const std::string& version, const int& update_type, const std::string& message) { return g_ui_signals.UpdateMessageBox(version, update_type, message); }
bool CClientUIInterface::ThreadSafeAskFee(int64_t nFeeRequired, const std::string& strCaption) { return g_ui_signals.ThreadSafeAskFee(nFeeRequired, strCaption).value_or(false); }
bool CClientUIInterface::ThreadSafeAskQuestion(std::string caption, std::string body) { return g_ui_signals.ThreadSafeAskQuestion(caption, body).value_or(false); }
void CClientUIInterface::ThreadSafeHandleURI(const std::string& strURI) { return g_ui_signals.ThreadSafeHandleURI(strURI); }
void CClientUIInterface::InitMessage(const std::string &message) { return g_ui_signals.InitMessage(message); }
void CClientUIInterface::QueueShutdown() { return g_ui_signals.QueueShutdown(); }
std::string CClientUIInterface::Translate(const char* psz) { return g_ui_signals.Translate(psz).value_or(std::string(psz)); }
void CClientUIInterface::NotifyBlocksChanged(bool syncing, int height, int64_t best_time, uint32_t target_bits) { return g_ui_signals.NotifyBlocksChanged(syncing, height, best_time, target_bits); }
void CClientUIInterface::NotifyNumConnectionsChanged(int newNumConnections) { return g_ui_signals.NotifyNumConnectionsChanged(newNumConnections); }
void CClientUIInterface::BannedListChanged() { return g_ui_signals.BannedListChanged(); }
void CClientUIInterface::MinerStatusChanged(bool staking, double coin_weight) { return g_ui_signals.MinerStatusChanged(staking, coin_weight); }
void CClientUIInterface::ResearcherChanged() { return g_ui_signals.ResearcherChanged(); }
void CClientUIInterface::AccrualChangedFromStakeOrMRC() { return g_ui_signals.AccrualChangedFromStakeOrMRC(); }
void CClientUIInterface::MRCChanged() { return g_ui_signals.MRCChanged(); }
void CClientUIInterface::BeaconChanged() { return g_ui_signals.BeaconChanged(); }
void CClientUIInterface::NewPollReceived(int64_t poll_time) { return g_ui_signals.NewPollReceived(poll_time); }
void CClientUIInterface::NewVoteReceived(const uint256& poll_txid) { return g_ui_signals.NewVoteReceived(poll_txid); }
void CClientUIInterface::NotifyAlertChanged(const uint256 &hash, ChangeType status) { return g_ui_signals.NotifyAlertChanged(hash, status); }
void CClientUIInterface::NotifyScraperEvent(const scrapereventtypes& ScraperEventtype, ChangeType status, const std::string& message) { return g_ui_signals.NotifyScraperEvent(ScraperEventtype, status, message); }
void CClientUIInterface::RwSettingsUpdated() { return g_ui_signals.RwSettingsUpdated(); }

bool InitError(const std::string &str)
{
    uiInterface.ThreadSafeMessageBox(str, "Gridcoin", CClientUIInterface::MSG_ERROR);
    return false;
}

void InitWarning(const std::string &str)
{
    uiInterface.ThreadSafeMessageBox(str, "Gridcoin", CClientUIInterface::MSG_WARNING);
}
