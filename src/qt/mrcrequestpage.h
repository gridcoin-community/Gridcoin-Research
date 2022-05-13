// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_MRCREQUESTPAGE_H
#define GRIDCOIN_QT_MRCREQUESTPAGE_H

#include <QAbstractButton>
#include <QDialog>

#include "bitcoinamountfield.h"
#include "mrcmodel.h"

class WalletModel;

namespace Ui {
    class MRCRequestPage;
}

class MRCRequestPage : public QDialog
{
    Q_OBJECT

public:
    explicit MRCRequestPage(QWidget* parent = nullptr, MRCModel *mrc_model = nullptr);
    ~MRCRequestPage();

private:
    Ui::MRCRequestPage *ui;
    MRCModel *m_mrc_model;
    WalletModel *m_wallet_model;

    QRect m_orig_geometry;
    QRect m_gridLayout_orig_geometry;
    QSize m_scaled_size;

    void updateMRCModel();
    void showMRCStatus(MRCModel::ModelStatus status);

private slots:
    void buttonBoxClicked(QAbstractButton* button);
    void updateMRCStatus();
    void setMRCProvidedFee();
    void submitMRC();
};

#endif // GRIDCOIN_QT_MRCREQUESTPAGE_H
