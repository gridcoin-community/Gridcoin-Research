// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/bitcoinunits.h"
#include "qt/decoration.h"
#include "qt/forms/voting/ui_pollwizarddetailspage.h"
#include "qt/optionsmodel.h"
#include "qt/voting/pollwizard.h"
#include "qt/voting/pollwizarddetailspage.h"
#include "qt/voting/votingmodel.h"

#include <QMessageBox>
#include <QStringListModel>
#include <QStyledItemDelegate>

namespace {
//!
//! \brief Applies custom appearance and behavior to items in the poll choices
//! editor.
//!
class ChoicesListDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    ChoicesListDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent)
    {
    }

    QWidget* createEditor(
        QWidget* parent,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const override
    {
        QWidget* editor = QStyledItemDelegate::createEditor(parent, option, index);

        if (QLineEdit* line_edit = qobject_cast<QLineEdit*>(editor)) {
            line_edit->setMaxLength(VotingModel::maxPollChoiceLabelLength());
        }

        return editor;
    }

private:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);

        // Display a row number before each choice label:
        option->text = QStringLiteral("%1. %2").arg(index.row() + 1).arg(option->text);
    }
}; // ChoicesListDelegate

//!
//! \brief Provides for QWizardPage::registerField() without a real widget.
//!
struct DummyField : public QWidget
{
    DummyField(QWidget* parent = nullptr) : QWidget(parent) { hide(); }
};
} // Anonymous namespace

//!
//! \brief Manages the set of choices for the poll choices editor.
//!
class ChoicesListModel : public QStringListModel
{
    Q_OBJECT

public:
    ChoicesListModel(QObject* parent = nullptr) : QStringListModel(parent)
    {
    }

    bool isComplete(const int response_type) const
    {
        if (response_type == 0) {
            return true;
        }

        return rowCount(QModelIndex()) >= 2;
    }

    QModelIndex addItem()
    {
        const int row = rowCount(QModelIndex());

        if (!insertRows(row, 1)) {
            return QModelIndex();
        }

        return index(row);
    }

    void removeItem(const QModelIndex& index)
    {
        if (index.isValid()) {
            removeRows(index.row(), 1);
        }

        emit completeChanged();
    }

    bool setData(const QModelIndex& index, const QVariant& value, int role) override
    {
        emit completeChanged();

        return QStringListModel::setData(index, value.toString().trimmed(), role);
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        if (!index.isValid()) {
            return QStringListModel::flags(index) | Qt::ItemIsDropEnabled;
        }

        return QStringListModel::flags(index) & ~Qt::ItemIsDropEnabled;
    }

signals:
    void completeChanged();
}; // ChoicesListModel

// -----------------------------------------------------------------------------
// Class: PollWizardDetailsPage
// -----------------------------------------------------------------------------

PollWizardDetailsPage::PollWizardDetailsPage(QWidget* parent)
    : QWizardPage(parent)
    , ui(new Ui::PollWizardDetailsPage)
    , m_poll_types(nullptr)
    , m_choices_model(new ChoicesListModel(this))
{
    ui->setupUi(this);

    GRC::ScaleFontPointSize(ui->pageTitleLabel, 14);

    setCommitPage(true);
    setButtonText(QWizard::CommitButton, tr("Create Poll"));

    registerField("title*", ui->titleField);
    registerField("durationDays", ui->durationField);
    registerField("question", ui->questionField);
    registerField("url*", ui->urlField);
    registerField("weightType", ui->weightTypeList);
    registerField("responseType", ui->responseTypeList);
    registerField("txid", new DummyField(this), "", "");

    ui->durationField->setMinimum(VotingModel::minPollDurationDays());
    ui->durationField->setMaximum(VotingModel::maxPollDurationDays());
    ui->titleField->setMaxLength(VotingModel::maxPollTitleLength());
    ui->questionField->setMaxLength(VotingModel::maxPollQuestionLength());
    ui->urlField->setMaxLength(VotingModel::maxPollUrlLength());

    ui->weightTypeList->addItem(tr("Balance"));
    ui->weightTypeList->addItem(tr("Magnitude+Balance"));

    ui->responseTypeList->addItem(tr("Yes/No/Abstain"));
    ui->responseTypeList->addItem(tr("Single Choice"));
    ui->responseTypeList->addItem(tr("Multiple Choice"));

    ChoicesListDelegate* choices_delegate = new ChoicesListDelegate(this);

    ui->choicesList->setModel(m_choices_model.get());
    ui->choicesList->setItemDelegate(choices_delegate);
    ui->choicesFrame->hide();
    ui->editChoiceButton->hide();
    ui->removeChoiceButton->hide();

    connect(
        ui->responseTypeList, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [this](int index) {
            ui->choicesFrame->setVisible(index > 0);
            ui->standardChoicesLabel->setVisible(index == 0);
            emit completeChanged();
        });
    connect(
        ui->choicesList->selectionModel(), &QItemSelectionModel::selectionChanged,
        [this](const QItemSelection& selected, const QItemSelection& deselected) {
            Q_UNUSED(deselected);
            ui->editChoiceButton->setVisible(!selected.isEmpty());
            ui->removeChoiceButton->setVisible(!selected.isEmpty());
        });
    connect(ui->addChoiceButton, &QAbstractButton::clicked, [this]() {
        ui->choicesList->edit(m_choices_model->addItem());
        ui->choicesList->scrollToBottom();
    });
    connect(ui->editChoiceButton, &QAbstractButton::clicked, [this]() {
        ui->choicesList->edit(ui->choicesList->selectionModel()->selectedIndexes().first());
    });
    connect(ui->removeChoiceButton, &QAbstractButton::clicked, [this]() {
        m_choices_model->removeItem(ui->choicesList->selectionModel()->selectedIndexes().first());
    });
    connect(m_choices_model.get(), &ChoicesListModel::completeChanged, [this]() {
        emit completeChanged();
    });
}

PollWizardDetailsPage::~PollWizardDetailsPage()
{
    delete ui;
}

void PollWizardDetailsPage::setModel(VotingModel* voting_model)
{
    m_voting_model = voting_model;

    updateIcons();
}

void PollWizardDetailsPage::setPollTypes(const PollTypes* const poll_types)
{
    m_poll_types = poll_types;
}

void PollWizardDetailsPage::initializePage()
{
    if (!m_poll_types) {
        return;
    }

    ui->errorLabel->hide();

    const int type_id = field("pollType").toInt();
    const PollTypeItem& poll_type = (*m_poll_types)[type_id];

    ui->pollTypeLabel->setText(poll_type.m_name);
    ui->durationField->setMinimum(poll_type.m_min_duration_days);
    ui->durationField->setValue(poll_type.m_min_duration_days);

    if (type_id != (int) GRC::PollType::SURVEY) {
        ui->pollTypeAlert->show();
        ui->weightTypeList->setCurrentIndex(1); // Magnitude+Balance
        ui->weightTypeList->setDisabled(true);
    } else {
        ui->pollTypeAlert->hide();
        ui->weightTypeList->setEnabled(true);
    }

    if (type_id == (int) GRC::PollType::PROJECT) {
        ui->titleField->setText(QStringLiteral("[%1] %2")
            .arg(poll_type.m_name)
            .arg(field("projectPollTitle").toString()));
        ui->responseTypeList->setCurrentIndex(0); // Yes/No/Abstain
        ui->responseTypeList->setDisabled(true);
    } else {
        ui->responseTypeList->setEnabled(true);
    }
}

bool PollWizardDetailsPage::validatePage()
{
    if (!m_voting_model) {
        return false;
    }

    const CAmount burn_fee = m_voting_model->estimatePollFee();
    const QMessageBox::StandardButton pressed = QMessageBox::question(
        this,
        tr("Create Poll"),
        tr("This poll will cost %1 plus a transaction fee. Continue?")
            .arg(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, burn_fee))
    );

    if (pressed != QMessageBox::Yes) {
        return false;
    }

    const int type_id = field("pollType").toInt();
    const GRC::PollType& core_poll_type = GRC::Poll::POLL_TYPES[type_id];

    const VotingResult result = m_voting_model->sendPoll(
        core_poll_type,
        field("title").toString(),
        field("durationDays").toInt(),
        field("question").toString(),
        field("url").toString(),
        // The dropdown list only contains non-deprecated weight type options
        // which start from offset 2:
        field("weightType").toInt() + 2,
        field("responseType").toInt() + 1,
        m_choices_model->stringList());

    if (!result.ok()) {
        ui->errorLabel->setText(result.error());
        ui->errorLabel->show();

        return false;
    }

    setField("txid", result.txid());

    return true;
}

bool PollWizardDetailsPage::isComplete() const
{
    return QWizardPage::isComplete()
        && m_choices_model->isComplete(field("responseType").toInt());
}

void PollWizardDetailsPage::updateIcons()
{
    if (!m_voting_model) {
        return;
    }

    const QString theme = m_voting_model->getOptionsModel().getCurrentStyle();

    ui->addChoiceButton->setIcon(QIcon(":/icons/" + theme + "_add"));
    ui->editChoiceButton->setIcon(QIcon(":/icons/" + theme + "_edit"));
    ui->removeChoiceButton->setIcon(QIcon(":/icons/" + theme + "_remove"));
}

#include "qt/voting/pollwizarddetailspage.moc"
