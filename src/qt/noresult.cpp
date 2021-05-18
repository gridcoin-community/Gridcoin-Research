// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/decoration.h"
#include "qt/forms/ui_noresult.h"
#include "qt/noresult.h"

NoResult::NoResult(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::NoResult)
    , m_content_widget(nullptr)
{
    ui->setupUi(this);

    GRC::ScaleFontPointSize(ui->titleLabel, 13);
}

NoResult::~NoResult()
{
    delete ui;
}

QWidget* NoResult::contentWidget()
{
    return m_content_widget;
}

void NoResult::setTitle(const QString& title)
{
    ui->titleLabel->setText(title);
}

void NoResult::setContentWidget(QWidget* widget)
{
    if (m_content_widget != nullptr) {
        ui->verticalLayout->removeWidget(m_content_widget);
    }

    m_content_widget = widget;

    if (widget != nullptr) {
        // Insert the widget above the bottom spacer:
        const int index = ui->verticalLayout->count() - 1;
        ui->verticalLayout->insertWidget(index, widget, 0, Qt::AlignHCenter);
    }
}
