// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/decoration.h"
#include "qt/forms/voting/ui_pollwizardprojectpage.h"
#include "qt/voting/pollwizard.h"
#include "qt/voting/pollwizardprojectpage.h"
#include "qt/voting/votingmodel.h"

#include <QStringListModel>

// -----------------------------------------------------------------------------
// Class: PollWizardProjectPage
// -----------------------------------------------------------------------------

PollWizardProjectPage::PollWizardProjectPage(QWidget* parent)
    : QWizardPage(parent)
    , ui(new Ui::PollWizardProjectPage)
{
    ui->setupUi(this);

    GRC::ScaleFontPointSize(ui->pageTitleLabel, 14);

    // The asterisk denotes a mandatory field:
    registerField("projectPollAddRemoveState*", ui->addRemoveStateLineEdit);
    registerField("projectName*", ui->projectNameField);
    registerField("projectUrl*", ui->projectUrlField);

    ui->addWidget->hide();
    ui->removeWidget->hide();
    ui->addRemoveStateLineEdit->hide();

    QStringListModel* project_names_model = new QStringListModel(this);
    QStringListModel* project_urls_model = new QStringListModel(this);

    ui->projectsList->setModel(project_names_model);

    connect(ui->addRadioButton, &QAbstractButton::toggled, this, [=](bool checked) {
        if (!checked) {
            ui->criteriaCheckbox->setChecked(false);
        }

        ui->addWidget->setVisible(checked);
        setField("projectPollAddRemoveState", QVariant("Add"));
        setField("projectName", QVariant());
        setField("projectUrl", QVariant());
        emit completeChanged();
    });
    connect(ui->removeRadioButton, &QAbstractButton::toggled, this, [=](bool checked) {
        if (checked && m_voting_model) {
            project_names_model->setStringList(m_voting_model->getActiveProjectNames());
            project_urls_model->setStringList(m_voting_model->getActiveProjectUrls());
        }

        ui->removeWidget->setVisible(checked);
        setField("projectPollAddRemoveState", QVariant("Remove"));
        setField("projectName", QVariant());
        setField("projectUrl", QVariant());
        emit completeChanged();
    });
    connect(ui->criteriaCheckbox, &QAbstractButton::toggled, this, [=](bool checked) {
        Q_UNUSED(checked);
        emit completeChanged();
    });
    connect(ui->projectsList->selectionModel(), &QItemSelectionModel::selectionChanged,
        this, [=](const QItemSelection& selected, const QItemSelection& deselected) {
            Q_UNUSED(deselected);

            if (!selected.isEmpty()) {
                const QModelIndex index = selected.indexes().first();
                setField("projectName", project_names_model->data(index).toString());
                setField("projectUrl", project_urls_model->data(index).toString());
            }

            emit completeChanged();
        });
}

PollWizardProjectPage::~PollWizardProjectPage()
{
    delete ui;
}

void PollWizardProjectPage::setModel(VotingModel* voting_model)
{
    m_voting_model = voting_model;
}

void PollWizardProjectPage::initializePage()
{
    ui->criteriaCheckbox->setChecked(false);
    ui->projectsList->selectionModel()->clearSelection();
}

bool PollWizardProjectPage::validatePage()
{
    return true;
}

bool PollWizardProjectPage::isComplete() const
{
    if (!QWizardPage::isComplete()) {
        return false;
    }

    if (ui->addRadioButton->isChecked()) {
        return ui->criteriaCheckbox->isChecked();
    }

    if (ui->removeRadioButton->isChecked()) {
        return true;
    }

    return false;
}
