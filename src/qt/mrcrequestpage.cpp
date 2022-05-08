// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "mrcrequestpage.h"
#include "ui_mrcrequestpage.h"
#include "mrcmodel.h"
#include "qt/decoration.h"

MRCRequestPage::MRCRequestPage(
    QWidget *parent,
    MRCModel* mrc_model)
    : QWidget(parent)
    , ui(new Ui::MRCRequestPage)
    , m_mrc_model(mrc_model)
{
    ui->setupUi(this);
}

MRCRequestPage::~MRCRequestPage()
{
    delete ui;
}
