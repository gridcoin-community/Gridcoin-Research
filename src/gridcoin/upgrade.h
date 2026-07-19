// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_UPGRADE_H
#define GRIDCOIN_UPGRADE_H

#include <string>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <vector>

#include "gridcoin/scraper/http.h"
#include "node/ui_interface.h"

namespace GRC {

/** A Class to support client update checks and local blockchain reset **/
//!
//! \brief Upgrade class
//!
//! Performs client update checks (version notification only -- it does not
//! download or install anything) and supports the local blockchain-data reset
//! used to force a clean resync.
//!
class Upgrade
{
public:
    //!
    //! \brief Constructor.
    //!
    Upgrade();

    //!
    //! \brief Enum for determining the type of message to be returned for ResetBlockchainData functions
    //!
    enum ResetBlockchainMsg {
        CleanUp
    };

    enum UpgradeType {
        Unknown,
        Leisure,
        Mandatory,
        Unsupported //! This is used for a running version that is greater than the current official release.
    };

    //!
    //! \brief Scheduler call to CheckForLatestUpdate
    //!
    static void ScheduledUpdateCheck();

    //!
    //! \brief Check for latest updates on GitHub.
    //!
    static bool CheckForLatestUpdate(std::string& client_message_out, std::string& change_log, UpgradeType& upgrade_type,
                                     bool ui_dialog = true);

    //!
    //! \brief Resolves symlinks to the actual path.
    //! \param actual_cleanup_path is the resolved path
    //! \return
    //!
    static bool GetActualCleanupPath(fs::path& actual_cleanup_path);

    //!
    //! \brief Cleans up previous blockchain data if any is found
    //!
    //! \return Bool on the success of cleanup
    //!
    static void CleanupBlockchainData(bool include_blockchain_data_files = true);

    //!
    //! \brief Small function to allow wallet user to clear blockchain data and sync from 0 while keeping a clean look
    //!
    //! \returns Bool on the success of blockchain cleanup
    //!
    static bool ResetBlockchainData(bool include_blockchain_data_files = true);

    //!
    //! \brief Moves the block data files from .dat to .dat.orig in preparation for reindexing.
    //! \return Boolean on success/failure
    //!
    static bool MoveBlockDataFiles(std::vector<std::pair<boost::filesystem::path, uintmax_t>>& block_data_files);

    //!
    //! \brief Utility function to support the -reindex startup parameter to rebuild txleveldb and accrual from
    //! existing blockchain data files.
    //! \return Boolean on success/failure
    //!
    static bool LoadBlockchainData(std::vector<std::pair<boost::filesystem::path, uintmax_t>>& block_data_files,
                                   bool sort,
                                   bool cleanup_imported_files);
    //!
    //! \brief Small function to return translated messages.
    //!
    //! \returns String containing message.
    //!
    static std::string ResetBlockchainMessages(ResetBlockchainMsg _msg);
};

} // namespace GRC

/** Unique Pointer for CScheduler for update checks **/
extern std::unique_ptr<GRC::Upgrade> g_UpdateChecker;

#endif // GRIDCOIN_UPGRADE_H
