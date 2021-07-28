// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <memory>
#include <sstream>
#include <iomanip>
#include <vector>

#include "gridcoin/scraper/http.h"
#include "ui_interface.h"

namespace GRC {

/** Snapshot Extraction Status **/
class SnapshotExtractStatus
{

public:
    void Reset()
    {
        SnapshotZipInvalid = false;
        SnapshotExtractComplete = false;
        SnapshotExtractFailed = false;
        SnapshotExtractProgress = 0;
    }

    bool GetSnapshotZipInvalid()
    {
        LOCK(cs_lock);

        return SnapshotZipInvalid;
    }

    bool GetSnapshotExtractComplete()
    {
        LOCK(cs_lock);

        return SnapshotExtractComplete;
    }

    bool GetSnapshotExtractFailed()
    {
        LOCK(cs_lock);

        return SnapshotExtractFailed;
    }

    int GetSnapshotExtractProgress()
    {
        LOCK(cs_lock);

        return SnapshotExtractProgress;
    }

    void SetSnapshotZipInvalid(bool SnapshotZipInvalid_in)
    {
        LOCK(cs_lock);

        SnapshotZipInvalid = SnapshotZipInvalid_in;
    }

    void SetSnapshotExtractComplete(bool SnapshotExtractComplete_in)
    {
        LOCK(cs_lock);

        SnapshotExtractComplete = SnapshotExtractComplete_in;
    }

    void SetSnapshotExtractFailed(bool SnapshotExtractFailed_in)
    {
        LOCK(cs_lock);

        SnapshotExtractFailed = SnapshotExtractFailed_in;
    }

    void SetSnapshotExtractProgress(int SnapshotExtractProgress_in)
    {
        LOCK(cs_lock);

        SnapshotExtractProgress = SnapshotExtractProgress_in;
    }

private:
    CCriticalSection cs_lock;

    bool SnapshotZipInvalid = false;
    bool SnapshotExtractComplete = false;
    bool SnapshotExtractFailed = false;
    int SnapshotExtractProgress = 0;
};

extern SnapshotExtractStatus ExtractStatus;
/** Qt Side **/
extern bool fCancelOperation;

class Progress;

/** A Class to support update checks and allow easy application of the latest snapshot **/
//!
//! \brief Upgrade class
//!
//! A easy method to allow update checks to be performed as well as
//! allow easy application of latest snapshot if a wallet user wants to do so.
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
        CleanUp,
        UpdateAvailable,
        GithubResponse
    };

    //!
    //! \brief Scheduler call to CheckForLatestUpdate
    //!
    static void ScheduledUpdateCheck();

    //!
    //! \brief Check for latest updates on GitHub.
    //!
    static bool CheckForLatestUpdate(std::string& client_message_out, bool ui_dialog = true, bool snapshotrequest = false);

    //!
    //! \brief Function that will be threaded to download snapshot
    //! and provide realtime updates on the progress.
    //!
    static void DownloadSnapshot();

    //!
    //! \brief Cleans up previous blockchain data if any is found
    //!
    //! \return Bool on the success of cleanup
    //!
    static void CleanupBlockchainData();

    //!
    //! \brief This is the worker thread "main" that actually does the brunt of the snapshot download and extraction work.
    //!
    static void WorkerMain(Progress &progress);

    //!
    //! \brief Extracts the snapshot zip file
    //!
    //! \return Bool on the success of extraction
    //!
    static void ExtractSnapshot();

    //!
    //! \brief Snapshot main function that runs the full snapshot task
    //!
    //! \throws std::runtime_error if any errors occur
    //!
    void SnapshotMain();

    //!
    //! \brief Verify the SHA256SUM of snapshot.zip against snapshot.zip.sha256sum on gridcoin.us
    //!
    //! \return Bool on the success of matching SHA256SUM
    //!
    static void VerifySHA256SUM();

    //!
    //! \brief Small function to delete the snapshot.zip file
    //!
    static void DeleteSnapshot();

    //!
    //! \brief Small function to allow wallet user to clear blockchain data and sync from 0 while keeping a clean look
    //!
    //! \returns Bool on the success of blockchain cleanup
    //!
    static bool ResetBlockchainData();

    //!
    //! \brief Small function to return translated messages.
    //!
    //! \returns String containing message.
    //!
    static std::string ResetBlockchainMessages(ResetBlockchainMsg _msg);
};

//!
//! \brief Progress class
//!
//! A Class to display progress of snapshot process to console users.
//! This is simular to what we do for QT.
//!
class Progress
{
public:
    // The order of this enum must be correlated with the StartStrings below.
    enum Type
    {
        SnapshotDownload,
        SHA256SumVerification,
        CleanupBlockchainData,
        SnapshotExtraction,
    };

private:
    // To enforce thread safety on type enum.
    CCriticalSection cs_lock;

    Type type;

    int CurrentProgress;

    const char *StartBar = "[";
    const char *EndBar = "]";
    const char *Filler = "*";
    /** Keep this even for simplicity **/
    const int LengthBar = 50;
    /** Keep spaced for cleaner look though this may not be perfect when translated **/
    const std::string StartStrings[4] = { _("Stage (1/4): Downloading snapshot.zip:         "),
                                          _("Stage (2/4): Verify SHA256SUM of snapshot.zip: "),
                                          _("Stage (3/4): Cleanup blockchain data:          "),
                                          _("Stage (4/4): Extracting snapshot.zip:          ") };

    int Variant;

protected:
    std::stringstream ProgressString;

public:
    //!
    //! \brief Constructor.
    //!
    Progress()
    {
        Variant = (int)(100 / (double)LengthBar);
        Variant += (Variant & 1);
    }

    //!
    //! \brief Function to set what stage of the snapshot process we are at.
    //! Also resets fully when needed.
    //!
    void SetType(Type type_in)
    {
        LOCK(cs_lock);

        type = type_in;

        Reset(true);
    }

    Type GetType()
    {
        LOCK(cs_lock);

        return type;
    }

    //!
    //! \brief Update the progress for the user.
    //!
    //! \param Current progress amount reported.
    //! \param Current download speed when applicable.
    //!
    //! \return The need if progress bar needs to be updated
    //!
    bool Update(int ProgressAmount, double ProgressBytes = 0, long long ProgressNow = 0, long long ProgressTotal = 0)
    {
        /** In case no change **/
        if (ProgressAmount == CurrentProgress)
            return false;

        if (ProgressAmount > CurrentProgress)
        {
            CurrentProgress = ProgressAmount;

            Reset();

            int FillAmount = CurrentProgress / Variant;

            for (int x = 0; x < FillAmount; x++)
                ProgressString << Filler;

            for (int y = 0; y < LengthBar - FillAmount; y++)
                ProgressString << " ";

            ProgressString << EndBar;

            ProgressString << " (" << CurrentProgress << "%)";

            if (type == 0)
            {
                if (ProgressBytes < 1000000 && ProgressBytes > 0)
                {
                    ProgressString << " "
                                   << std::fixed
                                   << std::setprecision(1)
                                   << (ProgressBytes / (double)1000)
                                   << " "
                                   << _("KB/s");
                }
                else if (ProgressBytes >= 1000000)
                {
                    ProgressString << " "
                                   << std::fixed
                                   << std::setprecision(1)
                                   << (ProgressBytes / (double)1000000)
                                   << " "
                                   << _("MB/s");
                }

                // Unsupported progress
                else
                {
                    ProgressString << " " << _("N/A");
                }

                ProgressString << " ("
                               << std::fixed
                               << std::setprecision(2)
                               << (ProgressNow / (double)(1024 * 1024 * 1024))
                               << _("GB/")
                               << std::fixed
                               << std::setprecision(2)
                               << (ProgressTotal / (double)(1024 * 1024 * 1024))
                               << _("GB)");
            }
        }

        return true;
    }

    //!
    //! \brief Progress bar string
    //!
    //! \return String in std::string format.
    //!
    std::string Status()
    {
        const std::string& Data = ProgressString.str();

        return Data;
    }

private:
    //!
    //! \brief Resets the ProgressString based on needs.
    //!
    //! \param Set whether a complete reset is needed (no \r)
    //!
    void Reset(bool fullreset = false)
    {
        ProgressString.clear();
        ProgressString.str(std::string());

        if (!fullreset)
        {
            ProgressString << "\r";
        }
        else
        {
            CurrentProgress = 0;
        }

        ProgressString << StartStrings[type] << StartBar;
    }
};
} // namespace GRC

/** Unique Pointer for CScheduler for update checks **/
extern std::unique_ptr<GRC::Upgrade> g_UpdateChecker;
