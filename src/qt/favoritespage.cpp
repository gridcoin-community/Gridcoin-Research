// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/addressbookpage.h"
#include "qt/decoration.h"
#include "qt/forms/ui_favoritespage.h"
#include "qt/optionsmodel.h"
#include "qt/favoritespage.h"

#include <QAction>
#include <QIcon>

FavoritesPage::FavoritesPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::FavoritesPage)
    , addressBookPage(new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::SendingTab))
    , filterLineEditIconAction(new QAction())
{
    ui->setupUi(this);
    ui->contentFrameVerticalLayout->addWidget(addressBookPage);
    ui->filterLineEdit->addAction(filterLineEditIconAction, QLineEdit::LeadingPosition);

    GRC::ScaleFontPointSize(ui->headerTitleLabel, 15);

    connect(
        ui->filterLineEdit, &QLineEdit::textChanged,
        addressBookPage, &AddressBookPage::changeFilter);
    connect(
        addressBookPage, &AddressBookPage::verifyMessage,
        [this](const QString& address) { emit verifyMessage(address); });
}

FavoritesPage::~FavoritesPage()
{
    delete ui;
    delete addressBookPage;
    delete filterLineEditIconAction;
}

void FavoritesPage::setAddressTableModel(AddressTableModel* model)
{
    addressBookPage->setModel(model);
}

void FavoritesPage::setOptionsModel(OptionsModel* model)
{
    addressBookPage->setOptionsModel(model);

    if (model) {
        connect(model, SIGNAL(walletStylesheetChanged(QString)), this, SLOT(updateIcons(QString)));
        updateIcons(model->getCurrentStyle());
    }
}

void FavoritesPage::exportClicked()
{
    addressBookPage->exportClicked();
}

void FavoritesPage::updateIcons(const QString& theme)
{
    filterLineEditIconAction->setIcon(QIcon(":/icons/" + theme + "_search"));
}
