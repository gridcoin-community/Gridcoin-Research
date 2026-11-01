// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_UPGRADEQT_H
#define GRIDCOIN_QT_UPGRADEQT_H

#include <string>
#include <QString>
#include <QApplication>

class UpgradeQt : QObject
{
    Q_OBJECT

public:
    //!
    //! \brief Constructor.
    //!
    UpgradeQt();
    //!
    //! \brief ErrorMsg box for displaying errors that have occurred during the blockchain reset.
    //!
    //! \param text Main text displaying on QMessageBox.
    //! \param informativetext Informative text displaying on QMessageBox.
    //!
    static void ErrorMsg(const std::string& text, const std::string& informativetext);
    //!
    //! \brief Msg box for displaying informative information during snapshot process.
    //!
    //! \param text Main text displaying on QMessageBox.
    //! \param informativetext Informative text displaying on QMessageBox.
    //! \param question Are we requiring a response from the user other then 'ok'.
    //!
    //! \return Response made by user.
    //!
    static int Msg(const std::string& text, const std::string& informativetext, bool question = false);
    //!
    //! \brief Function to convert std::string to QString to keep code cleaner
    //!
    //! \param String to convert to QString
    //!
    //! \return QString
    static QString ToQString(const std::string& string);
    //!
    //! \brief Main function for sync from zero task.
    //!
    //! \return Returns success of blockchain data cleanup task.
    //!
    static bool ResetBlockchain(QApplication& ResetBlockchainApp);
};

#endif // GRIDCOIN_QT_UPGRADEQT_H
