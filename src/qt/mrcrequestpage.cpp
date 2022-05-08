// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "mrcrequestpage.h"
#include "ui_mrcrequestpage.h"
#include "mrcmodel.h"
#include "qt/decoration.h"

MRCRequestPage::MRCRequestPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MRCRequestPage)
{
    ui->setupUi(this);
}
