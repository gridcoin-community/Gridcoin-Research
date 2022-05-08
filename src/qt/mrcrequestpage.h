// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_MRCREQUESTPAGE_H
#define GRIDCOIN_QT_MRCREQUESTPAGE_H

#include <QWidget>

class MRCModel;

namespace Ui {
    class MRCRequestPage;
}

class MRCRequestPage : public QWidget
{
    Q_OBJECT

public:
    explicit MRCRequestPage(QWidget* parent = nullptr, MRCModel *mrc_model = nullptr);
    ~MRCRequestPage();

private:
    Ui::MRCRequestPage *ui;
    MRCModel *m_mrc_model;

};

#endif // GRIDCOIN_QT_MRCREQUESTPAGE_H
