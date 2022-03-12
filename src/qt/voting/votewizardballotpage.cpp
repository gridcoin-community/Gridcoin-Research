// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/decoration.h"
#include "qt/forms/voting/ui_votewizardballotpage.h"
#include "qt/voting/votewizard.h"
#include "qt/voting/votewizardballotpage.h"
#include "qt/voting/votingmodel.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QRadioButton>

namespace {
//!
//! \brief Provides for QWizardPage::registerField() without a real widget.
//!
struct DummyField : public QWidget
{
    DummyField(QWidget* parent = nullptr) : QWidget(parent) { hide(); }
};
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Class: VoteWizardBallotPage
// -----------------------------------------------------------------------------

VoteWizardBallotPage::VoteWizardBallotPage(QWidget *parent)
    : QWizardPage(parent)
    , ui(new Ui::VoteWizardBallotPage)
    , m_choice_buttons(new QButtonGroup(this))
{
    ui->setupUi(this);

    setCommitPage(true);
    setButtonText(QWizard::CommitButton, tr("Submit Vote"));

    registerField("txid", new DummyField(this), "", "");
    registerField("responseLabels", new DummyField(this));

    #if (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
        connect(
            m_choice_buttons.get(), QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked),
            [this](QAbstractButton*) { emit completeChanged(); });
    #else
        connect(m_choice_buttons.get(), &QButtonGroup::idClicked, this, [=] { emit completeChanged(); });
    #endif
}

VoteWizardBallotPage::~VoteWizardBallotPage()
{
    delete ui;
}

void VoteWizardBallotPage::setModel(VotingModel* voting_model)
{
    m_voting_model = voting_model;
}

void VoteWizardBallotPage::setPoll(const PollItem& poll_item)
{
    ui->details->setItem(poll_item);
    m_choice_buttons->setExclusive(!poll_item.m_multiple_choice);
    m_poll_id = poll_item.m_id;

    for (const auto& choice : poll_item.m_choices) {
        QAbstractButton* button;

        if (poll_item.m_multiple_choice) {
            button = new QCheckBox(choice.m_label);
        } else {
            button = new QRadioButton(choice.m_label);
        }

        ui->choicesLayout->addWidget(button);
        m_choice_buttons->addButton(button);
    }
}

void VoteWizardBallotPage::initializePage()
{
    ui->errorLabel->hide();
}

bool VoteWizardBallotPage::validatePage()
{
    const QList<QAbstractButton*> choice_buttons = m_choice_buttons->buttons();
    std::vector<uint8_t> choices;
    QStringList labels;

    for (int i = 0; i < choice_buttons.count(); ++i) {
        if (choice_buttons[i]->isChecked()) {
            choices.emplace_back(i);
            labels << choice_buttons[i]->text();
        }
    }

    const VotingResult result = m_voting_model->sendVote(m_poll_id, choices);

    if (!result.ok()) {
        ui->errorLabel->setText(result.error());
        ui->errorLabel->show();

        return false;
    }

    setField("txid", result.txid());
    setField("responseLabels", labels.join('\n'));

    return true;
}

bool VoteWizardBallotPage::isComplete() const
{
    return m_choice_buttons->checkedId() != -1;
}
