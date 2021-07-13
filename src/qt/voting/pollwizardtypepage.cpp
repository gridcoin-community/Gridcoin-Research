// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/decoration.h"
#include "qt/forms/voting/ui_pollwizardtypepage.h"
#include "qt/voting/pollwizard.h"
#include "qt/voting/pollwizardtypepage.h"
#include "qt/voting/votingmodel.h"

#include <QButtonGroup>
#include <QCommandLinkButton>
#include <QSpinBox>

// -----------------------------------------------------------------------------
// Class: PollWizardTypePage
// -----------------------------------------------------------------------------

PollWizardTypePage::PollWizardTypePage(QWidget* parent)
    : QWizardPage(parent)
    , ui(new Ui::PollWizardTypePage)
    , m_type_buttons(new QButtonGroup(this))
{
    ui->setupUi(this);

    GRC::ScaleFontPointSize(ui->pageTitleLabel, 14);
    GRC::ScaleFontPointSize(ui->typeTextLabel, 11);

    // QWizardPage::registerField() cannot bind to QButtonGroup because it
    // expects a QWidget instance so we provide a proxy widget:
    QSpinBox* type_proxy = new QSpinBox(this);
    type_proxy->setVisible(false);

    registerField("pollType*", type_proxy);
    setField("pollType", PollTypes::PollTypeUnknown);

    connect(
        m_type_buttons, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked),
        [=](QAbstractButton*) { type_proxy->setValue(m_type_buttons->checkedId()); });
}

PollWizardTypePage::~PollWizardTypePage()
{
    delete ui;
    delete m_type_buttons;
}

void PollWizardTypePage::setPollTypes(const PollTypes* const poll_types)
{
    const QIcon button_icon(":/icons/tx_contract_voting");

    // Start with "i = 1" to skip PollTypes::PollTypeUnknown:
    for (size_t i = 1, row = 0, column = 0; i < poll_types->size(); ++i) {
        const PollTypeItem& poll_type = (*poll_types)[i];

        QCommandLinkButton* button = new QCommandLinkButton(this);
        button->setText(poll_type.m_name);
        button->setDescription(poll_type.m_description);
        button->setIcon(button_icon);
        button->setCheckable(true);
        GRC::ScaleFontPointSize(button, 9);

        ui->typesButtonLayout->addWidget(button, row, column);
        m_type_buttons->addButton(button, i);

        row += column;
        column = !column;
    }
}

int PollWizardTypePage::nextId() const
{
    switch (field("pollType").toInt()) {
        case PollTypes::PollTypeProject:
            return PollWizard::PageProject;
    }

    return PollWizard::PageDetails;
}
