// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef FAVORITESPAGE_H
#define FAVORITESPAGE_H

#include <QWidget>

namespace Ui {
    class FavoritesPage;
}

class AddressBookPage;
class AddressTableModel;
class OptionsModel;

QT_BEGIN_NAMESPACE
class QAction;
class QString;
QT_END_NAMESPACE

class FavoritesPage : public QWidget
{
    Q_OBJECT

public:
    explicit FavoritesPage(QWidget* parent = nullptr);
    ~FavoritesPage();

    void setAddressTableModel(AddressTableModel* model);
    void setOptionsModel(OptionsModel* model);

private:
    Ui::FavoritesPage* ui;
    AddressBookPage* addressBookPage;
    QAction* filterLineEditIconAction;

signals:
    void verifyMessage(QString address);

public slots:
    void exportClicked();

private slots:
    void updateIcons(const QString& theme);
};

#endif // FAVORITESPAGE_H
