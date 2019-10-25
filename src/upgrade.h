#ifndef UPGRADE_H
#define UPGRADE_H

#include <string>
#include <memory>
#include <sstream>
#include <iomanip>
#include <vector>

#include "scraper/http.h"
#include "ui_interface.h"

/** Snapshot Extraction Status struct **/
struct struct_SnapshotExtractStatus{
    bool SnapshotExtractComplete = false;
    bool SnapshotExtractFailed = false;
    int SnapshotExtractProgress;
};

extern struct_SnapshotExtractStatus ExtractStatus;
/** Qt Side **/
extern bool fCancelOperation;

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
    //! \brief Check for latest updates on github.
    //!
    void CheckForLatestUpdate();

    //!
    //! \brief Function that will be threaded to download snapshot
    //! and provide realtime updates on the progress.
    //!
    void DownloadSnapshot();

    //!
    //! \brief Cleans up previous blockchain data if any is found
    //!
    //! \return Bool on the success of cleanup
    //!
    bool CleanupBlockchainData();

    //!
    //! \brief Extracts the snapshot zip file
    //!
    //! \return Bool on the success of extraction
    //!
    bool ExtractSnapshot();

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
    bool VerifySHA256SUM();
};

//!
//! \brief Progress class
//!
//! A Class to display progress of snapshot process to console users.
//! This is simular to what we do for QT.
//!
class Progress
{
private:

    int Type;
    int CurrentProgress;

    const char *StartBar = "[";
    const char *EndBar = "]";
    const char *Filler = "*";
    /** Keep this even for simplicity **/
    const int LengthBar = 50;
    /** Keep spaced for cleaning look thou this may not be perfect when translated **/
    const std::string StartStrings[4] = { _("Stage (1/4): Downloading snapshot.zip:              "),
                                          _("Stage (2/4): Verify SHA256SUM of snapshot.zip:      "),
                                          _("Stage (3/4): Cleanup blockchain data:               "),
                                          _("Stage (4/4): Extracting snapshot.zip:               ") };

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
    void SetType(int Typein)
    {
        Type = Typein;

        Reset(true);
    }

    //!
    //! \brief Update the progress for the user.
    //!
    //! \param Current progress amount reported.
    //! \param Current download speed when applicable.
    //!
    //! \return The need if progress bar needs to be updated
    //!
    bool Update(int ProgressAmount, double ProgressBytes = 0)
    {
        /** Incase no change **/
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

            if (Type == 0)
            {
                double NewBytes;

                if (ProgressBytes < 1000000 && ProgressBytes > 0)
                {
                    NewBytes = ProgressBytes / (double)1000;

                    ProgressString << " " << std::fixed << std::setprecision(1) << NewBytes << _(" KB/s");
                }

                else
                {
                    NewBytes = ProgressBytes / (double)1000000;

                    ProgressString << " " << std::fixed << std::setprecision(1) << NewBytes << _(" MB/s");
                }
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
            ProgressString << "\r";

        else
            CurrentProgress = 0;

        ProgressString << StartStrings[Type] << StartBar;
    }
};

/** Unique Pointer for CScheduler for update checks **/
extern std::unique_ptr<Upgrade> g_UpdateChecker;

#endif // UPGRADE_H

