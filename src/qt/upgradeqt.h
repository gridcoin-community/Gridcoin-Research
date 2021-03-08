// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UPGRADEQT_H
#define UPGRADEQT_H

#include <string>
#include <QString>
#include <QApplication>

class UpgradeQt
{
public:
    //!
    //! \brief Constructor.
    //!
    UpgradeQt();
    //!
    //! \brief Main function for snapshot task.
    //!
    //! \return Returns success of snapshot task.
    //!
    bool SnapshotMain(QApplication& SnapshotApp);
    //!
    //! \brief Function called via thread to download snapshot and provide realtime updates of progress.
    //!
    //! \return Success of function.
    //!
    void DownloadSnapshot();
    //!
    //! \brief Function called via thread to extract snapshot and provide realtime updates of progress.
    //!
    void ExtractSnapshot();
    //!
    //! \brief ErrorMsg box for displaying errors that have occurred during snapshot process.
    //!
    //! \param text Main text displaying on QMessageBox.
    //! \param informativetext Informative text diaplying on QMessageBox.
    //!
    static void ErrorMsg(const std::string& text, const std::string& informativetext);
    //!
    //! \brief Msg box for displaying informative information during snapshot process.
    //!
    //! \param text Main text displaying on QMessageBox.
    //! \param informativetext Informative text dialaying on QMessageBox.
    //! \param question Are we requiring a response from the user other then 'ok'.
    //!
    //! \return Response made by user.
    //!
    static int Msg(const std::string& text, const std::string& informativetext, bool question = false);
    //!
    //! \brief Function return intent of user of requested action.
    //!
    //! \return bool
    //!
    static bool CancelOperation();
    //!
    //! \brief Function to convert std::string to QString to keep code cleaner
    //!
    //! \param String to convert to QString
    //!
    //! \return QString
    static QString ToQString(const std::string& string);
    //!
    //! \brief Small function to delete the snapshot.zip file
    //!
    static void DeleteSnapshot();

};
#endif // UPGRADEQT_H

