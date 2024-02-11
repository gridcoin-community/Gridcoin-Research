// Copyright (c) 2014-2024 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_UPDATEDIALOG_H
#define BITCOIN_QT_UPDATEDIALOG_H

#include "gridcoin/upgrade.h"
#include <QDialog>

namespace Ui {
class UpdateDialog;
}

class UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateDialog(QWidget* parent = nullptr);
    ~UpdateDialog();

    void setVersion(QString version);
    void setDetails(QString message);
    void setUpgradeType(GRC::Upgrade::UpgradeType upgrade_type);

private:
    Ui::UpdateDialog *ui;

};

#endif // BITCOIN_QT_UPDATEDIALOG_H
