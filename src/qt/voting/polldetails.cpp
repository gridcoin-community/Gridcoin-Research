// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/decoration.h"
#include "qt/guiutil.h"
#include "qt/forms/voting/ui_polldetails.h"
#include "qt/voting/polldetails.h"
#include "qt/voting/votingmodel.h"
#include "qt/voting/additionalfieldstablemodel.h"

PollDetails::PollDetails(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::PollDetails)
    , m_additional_fields_model(new AdditionalFieldsTableModel(this))
{
    ui->setupUi(this);

    GRC::ScaleFontPointSize(ui->dateRangeLabel, 9);
    GRC::ScaleFontPointSize(ui->titleLabel, 12);
    GRC::ScaleFontPointSize(ui->questionLabel, 11);
    GRC::ScaleFontPointSize(ui->additionalFieldsLabel, 11);

    ui->additionalFieldsTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->additionalFieldsTableView->sortByColumn(AdditionalFieldsTableModel::Required, Qt::DescendingOrder);
}

PollDetails::~PollDetails()
{
    delete ui;
}

void PollDetails::setItem(const PollItem& poll_item)
{
    ui->dateRangeLabel->setText(QStringLiteral("%1 → %2")
        .arg(GUIUtil::dateTimeStr(poll_item.m_start_time))
        .arg(GUIUtil::dateTimeStr(poll_item.m_expiration)));

    ui->titleLabel->setText(poll_item.m_title);
    ui->urlLabel->setText(QStringLiteral("<a href=\"%1\">%1</a>").arg(poll_item.m_url));

    m_additional_fields_model->setPollItem(&poll_item);
    m_additional_fields_model->refresh();
    ui->additionalFieldsTableView->setModel(m_additional_fields_model.get());
    if (m_additional_fields_model->empty()) {
        ui->additionalFieldsLabel->hide();
        ui->additionalFieldsTableView->hide();
    }

    ui->questionLabel->setVisible(!poll_item.m_question.isEmpty());
    ui->questionLabel->setText(poll_item.m_question);

    ui->topAnswerTextLabel->setVisible(!poll_item.m_top_answer.isEmpty());
    ui->topAnswerLabel->setVisible(!poll_item.m_top_answer.isEmpty());
    ui->topAnswerLabel->setText(poll_item.m_top_answer);
}
