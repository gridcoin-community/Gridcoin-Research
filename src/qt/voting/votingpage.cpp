// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/decoration.h"
#include "qt/forms/voting/ui_votingpage.h"
#include "qt/optionsmodel.h"
#include "qt/voting/polltab.h"
#include "qt/voting/polltablemodel.h"
#include "qt/voting/pollwizard.h"
#include "qt/voting/votingmodel.h"
#include "qt/voting/votingpage.h"

#include <QAction>
#include <QIcon>
#include <QMenu>

using namespace GRC;

namespace {
//!
//! \brief Provides a dropdown used to sort polls, especially for the card view.
//!
class PollSortMenu : public QMenu
{
    Q_OBJECT

public:
    PollSortMenu(VotingPage* parent) : QMenu(parent)
    {
        const PollTableModel model;
        const QModelIndex dummy;

        for (int column = 0; column < model.columnCount(dummy); ++column) {
            addAction(model.columnName(column), [parent, column]() {
                parent->currentTab().sort(column);
            });
        }
    }
}; // PollSortMenu
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Class: VotingPage
// -----------------------------------------------------------------------------

VotingPage::VotingPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::VotingPage)
    , m_filter_action(new QAction())
{
    ui->setupUi(this);

    GRC::ScaleFontPointSize(ui->headerTitleLabel, 15);

    m_tabs = {
        ui->activePollsTab,
        ui->finishedPollsTab
    };

    ui->activePollsTab->setPollFilterFlags(PollFilterFlag::ACTIVE);
    ui->finishedPollsTab->setPollFilterFlags(PollFilterFlag::FINISHED);

    m_filter_action->setShortcut(Qt::CTRL + Qt::Key_F);
    ui->filterLineEdit->addAction(m_filter_action.get(), QLineEdit::LeadingPosition);
    ui->cardsToggleButton->hide();
    ui->sortButton->setMenu(new PollSortMenu(this));

    // Move the buttons from the form into the tab widget's tab bar:
    ui->tabWidget->setCornerWidget(ui->tabButtonFrame);

    // Move the notification label from the form into the active polls tab:
    qobject_cast<QBoxLayout*>(ui->activePollsTab->layout())->insertWidget(0, ui->pollReceivedLabel);
    ui->pollReceivedLabel->hide();

    connect(m_filter_action.get(), &QAction::triggered, [this]() {
        ui->filterLineEdit->setFocus();
        ui->filterLineEdit->selectAll();
    });
    connect(ui->filterLineEdit, &QLineEdit::textChanged, [this](const QString& pattern) {
        currentTab().filter(pattern);
        m_current_filter = pattern;
    });
    connect(ui->cardsToggleButton, &QToolButton::clicked, [this]() {
        for (auto* tab : m_tabs) {
            tab->changeViewMode(PollTab::ViewCards);
        }

        ui->cardsToggleButton->hide();
        ui->tableToggleButton->show();
    });
    connect(ui->tableToggleButton, &QToolButton::clicked, [this]() {
        for (auto* tab : m_tabs) {
            tab->changeViewMode(PollTab::ViewTable);
        }

        ui->tableToggleButton->hide();
        ui->cardsToggleButton->show();
    });
    connect(ui->refreshButton, &QPushButton::clicked, [this]() {
        currentTab().refresh();

        if (ui->tabWidget->currentIndex() == PollTab::TabActive) {
            ui->pollReceivedLabel->hide();
        }
    });
    connect(ui->createPollButton, &QPushButton::clicked, [this]() {
        (new PollWizard(*m_voting_model, this))->show();
    });
    connect(ui->tabWidget, &QTabWidget::currentChanged, [this](int tab_id) {
        currentTab().filter(m_current_filter);
    });
}

VotingPage::~VotingPage()
{
    delete ui;
}

void VotingPage::setVotingModel(VotingModel* model)
{
    m_voting_model = model;

    for (auto tab : m_tabs) {
        tab->setVotingModel(model);
    }

    if (!model) {
        return;
    }

    connect(model, &VotingModel::newPollReceived, [this]() {
        ui->pollReceivedLabel->show();
    });
}

void VotingPage::setOptionsModel(OptionsModel* model)
{
    if (!model) {
        return;
    }

    connect(model, &OptionsModel::walletStylesheetChanged, this, &VotingPage::updateIcons);
    updateIcons(model->getCurrentStyle());
}

PollTab& VotingPage::currentTab()
{
    return *qobject_cast<PollTab*>(ui->tabWidget->currentWidget());
}

void VotingPage::updateIcons(const QString& theme)
{
    m_filter_action->setIcon(QIcon(":/icons/" + theme + "_search"));
    ui->cardsToggleButton->setIcon(QIcon(":/icons/" + theme + "_list_view"));
    ui->tableToggleButton->setIcon(QIcon(":/icons/" + theme + "_table_view"));
    ui->sortButton->setIcon(QIcon(":/icons/" + theme + "_sort_asc"));
    ui->refreshButton->setIcon(QIcon(":/icons/" + theme + "_refresh"));
    ui->createPollButton->setIcon(QIcon(":/icons/" + theme + "_create"));

    for (auto* tab : m_tabs) {
        tab->updateIcons(theme);
    }
}

#include "qt/voting/votingpage.moc"
