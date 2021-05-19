// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RECEIVECOINSPAGE_H
#define RECEIVECOINSPAGE_H

#include "walletmodel.h"

#include <QWidget>

namespace Ui {
    class ReceiveCoinsPage;
}

class AddressBookPage;
class AddressTableModel;
class OptionsModel;

QT_BEGIN_NAMESPACE
class QAction;
class QString;
QT_END_NAMESPACE

class ReceiveCoinsPage : public QWidget
{
    Q_OBJECT

public:
    explicit ReceiveCoinsPage(QWidget *parent = nullptr);
    ~ReceiveCoinsPage();

    void setAddressTableModel(AddressTableModel *model);
    void setOptionsModel(OptionsModel *model);

private:
    Ui::ReceiveCoinsPage *ui;
    AddressBookPage *addressBookPage;
    QAction *filterLineEditIconAction;

signals:
    void signMessage(QString address);

public slots:
    void exportClicked();

private slots:
    void updateIcons(const QString& theme);
};

#endif // RECEIVECOINSPAGE_H
