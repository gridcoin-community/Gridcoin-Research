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

//!
//! \brief The MRCRequestPage class implements the GUI MRC request form as a non-model dialog
//!
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

    void updateMRCModel();

    //!
    //! \brief showMRCStatus shows or hides the MRC status and submission widgets.
    //! \param status
    //!
    //! showMRCStatus shows or hides the MRC status and submission widgets based on the value of the status parameter.
    //! If the status is VALID, then the widgets allowing the view of the queue state and the submission are shown.
    //! In any other state, a customized blank form with a specialized message is shown based on the status value.
    void showMRCStatus(const MRCModel::ModelStatus& status);

private slots:
    void buttonBoxClicked(QAbstractButton* button);
    void updateMRCStatus();
    void setMRCFeeBoost();
    void setMRCFeeBoostToSubmitMinimum();
    void submitMRC();
};

#endif // GRIDCOIN_QT_MRCREQUESTPAGE_H
