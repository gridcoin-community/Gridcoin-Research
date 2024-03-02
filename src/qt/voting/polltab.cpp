// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/forms/voting/ui_polltab.h"
#include "qt/noresult.h"
#include "qt/voting/pollresultdialog.h"
#include "qt/voting/polltab.h"
#include "qt/voting/polltablemodel.h"
#include "qt/voting/votewizard.h"
#include "qt/voting/votingmodel.h"

#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QMenu>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QResizeEvent>

using namespace GRC;

namespace {
QString RefreshMessage()
{
    return QCoreApplication::translate("PollTab", "Press \"Refresh\" to update the list.");
}

QString WaitMessage()
{
    return QCoreApplication::translate("PollTab", "This may take several minutes.");
}

QString FullRefreshMessage()
{
    return QStringLiteral("%1 %2").arg(RefreshMessage()).arg(WaitMessage());
}
} // Anonymous namespace

//!
//! \brief An infinite progress bar that provides a loading animation while
//! refreshing the polls lists.
//!
class LoadingBar : public QProgressBar
{
    Q_OBJECT

    static constexpr int MAX = std::numeric_limits<int>::max();

public:
    LoadingBar(QWidget* parent = nullptr)
        : QProgressBar(parent)
        , m_active(false)
    {
        setRange(0, MAX);
        setTextVisible(false);
        setGraphicsEffect(&m_opacity_effect);
        hide();

        m_fade_anim.setTargetObject(&m_opacity_effect);
        m_fade_anim.setPropertyName("opacity");
        m_fade_anim.setDuration(1000);
        m_fade_anim.setStartValue(1);
        m_fade_anim.setEndValue(0);
        m_fade_anim.setEasingCurve(QEasingCurve::OutQuad);

        m_size_anim.setTargetObject(this);
        m_size_anim.setPropertyName("value");
        m_size_anim.setDuration(1000);
        m_size_anim.setStartValue(0);
        m_size_anim.setEndValue(MAX);
        m_size_anim.setEasingCurve(QEasingCurve::OutQuad);

        connect(&m_fade_anim, &QPropertyAnimation::finished, this, &QProgressBar::hide);
    }

    void start()
    {
        if (m_active) {
            return;
        }

        m_active = true;
        m_fade_anim.stop();
        m_opacity_effect.setOpacity(1);

        m_size_anim.stop();
        m_size_anim.setLoopCount(-1); // Infinite
        m_size_anim.setStartValue(0);
        m_size_anim.setEndValue(MAX);
        m_size_anim.start();

        connect(
            &m_size_anim, &QAbstractAnimation::currentLoopChanged,
            this, &LoadingBar::invertBarAnimation);

        setInvertedAppearance(false);
        reset();
        raise();
        show();
    }

    void finish()
    {
        m_active = false;
        m_size_anim.setLoopCount(m_size_anim.currentLoop() + 1);
        m_fade_anim.start();

        disconnect(
            &m_size_anim, &QAbstractAnimation::currentLoopChanged,
            this, &LoadingBar::invertBarAnimation);
    }

private:
    bool m_active;
    QGraphicsOpacityEffect m_opacity_effect;
    QPropertyAnimation m_fade_anim;
    QPropertyAnimation m_size_anim;

private slots:
    void invertBarAnimation()
    {
        const bool inverted = invertedAppearance();

        m_size_anim.setStartValue(inverted ? 0 : MAX);
        m_size_anim.setEndValue(inverted ? MAX : 0);
        setInvertedAppearance(!inverted);
    }
}; // LoadingBar

// -----------------------------------------------------------------------------
// Class: PollTab
// -----------------------------------------------------------------------------

PollTab::PollTab(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::PollTab)
    , m_polltable_model(new PollTableModel(this))
    , m_no_result(new NoResult(this))
    , m_loading(new LoadingBar(this))
{
    ui->setupUi(this);

    ui->tabLayout->addWidget(m_no_result.get());
    ui->stack->hide();

    ui->table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->table->sortByColumn(PollTableModel::Expiration, Qt::AscendingOrder);

    m_no_result->setContentWidget(new QLabel(FullRefreshMessage()));

    connect(ui->cards, &PollCardView::voteRequested, this, &PollTab::showVoteRowDialog);
    connect(ui->cards, &PollCardView::detailsRequested, this, &PollTab::showDetailsRowDialog);
    connect(ui->table, &QAbstractItemView::doubleClicked, this, &PollTab::showPreferredDialog);
    connect(ui->table, &QWidget::customContextMenuRequested, this, &PollTab::showTableContextMenu);
    connect(m_polltable_model.get(), &PollTableModel::layoutChanged, this, &PollTab::finishRefresh);

    // Forward the polltable model signal to the Poll Tab signal to avoid having to directly include the PollTableModel
    // in the voting page.
    connect(m_polltable_model.get(), &PollTableModel::newVoteReceivedAndPollMarkedDirty,
            this, &PollTab::newVoteReceivedAndPollMarkedDirty);
}

PollTab::~PollTab()
{
    delete ui;
}

void PollTab::setVotingModel(VotingModel* model)
{
    m_voting_model = model;
    m_polltable_model->setModel(model);

    ui->cards->setModel(m_polltable_model.get());
    ui->table->setModel(m_polltable_model.get());
}

void PollTab::setPollFilterFlags(PollFilterFlag flags)
{
    m_polltable_model->setPollFilterFlags(flags);
}

void PollTab::changeViewMode(const ViewId view_id)
{
    ui->stack->setCurrentIndex(view_id);
}

void PollTab::refresh()
{
    if (m_polltable_model->empty()) {
        m_no_result->showDefaultLoadingTitle();
        m_no_result->contentWidgetAs<QLabel>()->setText(WaitMessage());
    }

    m_loading->start();
    m_polltable_model->refresh();
}

void PollTab::filter(const QString& needle)
{
    if (needle != m_last_filter) {
        m_polltable_model->changeTitleFilter(needle);
        m_last_filter = needle;
    }
}

void PollTab::sort(const int column)
{
    const Qt::SortOrder order = m_polltable_model->custom_sort(column);
    ui->table->horizontalHeader()->setSortIndicator(column, order);
}

void PollTab::updateIcons(const QString& theme)
{
    ui->cards->updateIcons(theme);
}

const PollItem* PollTab::selectedTableItem() const
{
    if (!ui->table->selectionModel()->hasSelection()) {
        return nullptr;
    }

    return m_polltable_model->rowItem(
        ui->table->selectionModel()->selectedIndexes().first().row());
}

void PollTab::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_loading->setFixedWidth(event->size().width());
}

void PollTab::finishRefresh()
{
    m_loading->finish();
    ui->stack->setVisible(!m_polltable_model->empty());
    m_no_result->setVisible(m_polltable_model->empty());

    if (m_polltable_model->empty()) {
        m_no_result->showDefaultNoResultTitle();
        m_no_result->contentWidgetAs<QLabel>()->setText(FullRefreshMessage());
    }
}

void PollTab::showVoteRowDialog(int row)
{
    if (const PollItem* const poll_item = m_polltable_model->rowItem(row)) {
        showVoteDialog(*poll_item);
    }
}

void PollTab::showVoteDialog(const PollItem& poll_item)
{
    (new VoteWizard(poll_item, *m_voting_model, this))->show();
}

void PollTab::showDetailsRowDialog(int row)
{
    if (const PollItem* const poll_item = m_polltable_model->rowItem(row)) {
        showDetailsDialog(*poll_item);
    }
}

void PollTab::showDetailsDialog(const PollItem& poll_item)
{
    (new PollResultDialog(poll_item, this))->show();
}

void PollTab::showPreferredDialog(const QModelIndex& index)
{
    if (const PollItem* const poll_item = m_polltable_model->rowItem(index.row())) {
        if (poll_item->m_finished) {
            showDetailsDialog(*poll_item);
        } else {
            showVoteDialog(*poll_item);
        }
    }
}

void PollTab::showTableContextMenu(const QPoint& pos)
{
    if (const PollItem* const poll_item = selectedTableItem()) {
        QMenu menu;
        menu.addAction(tr("Show Results"), [this, poll_item]() {
            showDetailsDialog(*poll_item);
        });

        if (!poll_item->m_finished) {
            menu.addAction(tr("Vote"), [this, poll_item]() {
                showVoteDialog(*poll_item);
            });
        }

        menu.exec(ui->table->viewport()->mapToGlobal(pos));
    }
}

#include "qt/voting/polltab.moc"
