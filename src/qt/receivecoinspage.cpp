// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/addressbookpage.h"
#include "qt/decoration.h"
#include "qt/forms/ui_receivecoinspage.h"
#include "qt/optionsmodel.h"
#include "qt/receivecoinspage.h"

#include <QAction>
#include <QIcon>

ReceiveCoinsPage::ReceiveCoinsPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ReceiveCoinsPage)
    , addressBookPage(new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::ReceivingTab))
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
        addressBookPage, &AddressBookPage::signMessage,
        [this](const QString& address) { emit signMessage(address); });
}

ReceiveCoinsPage::~ReceiveCoinsPage()
{
    delete ui;
    delete addressBookPage;
    delete filterLineEditIconAction;
}

void ReceiveCoinsPage::setAddressTableModel(AddressTableModel *model)
{
    addressBookPage->setModel(model);
}

void ReceiveCoinsPage::setOptionsModel(OptionsModel *model)
{
    addressBookPage->setOptionsModel(model);

    if (model) {
        connect(model, SIGNAL(walletStylesheetChanged(QString)), this, SLOT(updateIcons(QString)));
        updateIcons(model->getCurrentStyle());
    }
}

void ReceiveCoinsPage::exportClicked()
{
    addressBookPage->exportClicked();
}

void ReceiveCoinsPage::updateIcons(const QString& theme)
{
    filterLineEditIconAction->setIcon(QIcon(":/icons/" + theme + "_search"));
}
