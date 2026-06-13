// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MULTISIGNDIALOG_H
#define BITCOIN_QT_MULTISIGNDIALOG_H

#include <QDialog>

namespace Ui {
    class MultisignPSGTDialog;
}
class WalletModel;

/**
 * Dialog for working with Partially Signed Gridcoin Transactions (PSGT):
 * load a base64 PSGT, inspect its inputs/outputs and signing status, sign the
 * inputs this wallet holds keys for, combine co-signers' PSGTs, and finalize a
 * fully-signed PSGT into a broadcast-ready raw transaction.
 *
 * Reuses the PSGT C++ API in src/psgt.h directly (the same functions the
 * psgt RPCs use); does not go through RPC.
 */
class MultisignPSGTDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MultisignPSGTDialog(QWidget* parent = nullptr);
    ~MultisignPSGTDialog();

    void setModel(WalletModel *model);

private:
    Ui::MultisignPSGTDialog *ui;
    WalletModel *model;

    //! Decode the working PSGT from the input box. Reports errors to the
    //! status label and returns false on failure.
    bool loadWorkingPSGT(struct PartiallySignedTransaction& psgt);

    void setStatus(const QString& text, bool error);

private slots:
    void on_inspectButton_clicked();
    void on_signButton_clicked();
    void on_combineButton_clicked();
    void on_finalizeButton_clicked();
    void on_copyResultButton_clicked();
    void on_clearButton_clicked();
};

#endif // BITCOIN_QT_MULTISIGNDIALOG_H
