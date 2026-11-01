// Copyright (c) 2014-2024 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_UPDATEDIALOG_H
#define BITCOIN_QT_UPDATEDIALOG_H

#include <QDialog>

namespace Ui {
class UpdateDialog;
}

//! GUI-local mirror of GRC::Upgrade::UpgradeType. The value is carried as a plain
//! int in interfaces::LatestVersionInfo::upgrade_type, which the About dialog
//! obtains from interfaces::Node::checkForLatestUpdate() and passes here, so this
//! dialog need not include gridcoin/upgrade.h. The enumerator values MUST stay in
//! sync with GRC::Upgrade::UpgradeType; src/node/interfaces.cpp static_asserts
//! that they do.
enum class UpdateType {
    Unknown = 0,
    Leisure = 1,
    Mandatory = 2,
    Unsupported = 3,
};

class UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateDialog(QWidget* parent = nullptr);
    ~UpdateDialog();

    void setVersion(QString version);
    void setDetails(QString message);
    //! \p upgrade_type is a UpdateType value passed as int (see the enum note).
    void setUpgradeType(int upgrade_type);

private:
    Ui::UpdateDialog *ui;

};

#endif // BITCOIN_QT_UPDATEDIALOG_H
