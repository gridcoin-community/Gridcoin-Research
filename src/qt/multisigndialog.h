// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MULTISIGNDIALOG_H
#define BITCOIN_QT_MULTISIGNDIALOG_H

#include <psgt.h>

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

    //! The PSGT the dialog currently operates on. The base64 text box stays the
    //! user-editable source of truth: every handler refreshes this from the box
    //! via syncWorkingFromInput() before acting, and the operations that produce
    //! a new PSGT (sign/combine) push the result back via setWorking(). Holding
    //! it as a member is also the seam a future Phase II PSGT-pool source (#2910)
    //! can populate without a base64 text round-trip.
    PartiallySignedTransaction m_working;

    //! Decode the input box into m_working. On failure, reports the error to the
    //! status label and leaves m_working untouched (decodes into a temporary and
    //! only assigns on success). Returns false on failure.
    bool syncWorkingFromInput();

    //! Adopt psgt as the working PSGT and write its base64 back to the input and
    //! result boxes. Used by the operations that produce a new PSGT, and the
    //! entry point a future pool source would call.
    void setWorking(const PartiallySignedTransaction& psgt);

    //! True iff some input carries a cryptographically valid partial signature
    //! from a key this wallet owns -- verified against the input's sighash, not
    //! merely present under an owned key id. This is the ">=1 valid signature"
    //! precondition a future Phase II "submit to pool" action (#2910) will gate on.
    bool walletHasSignature(const PartiallySignedTransaction& psgt) const;

    //! Render psgt into the read-only decoded view (decode plus, when the wallet
    //! is available, the "wallet's signature present" line).
    void showDecoded(const PartiallySignedTransaction& psgt);

    //! Format a satoshi amount with BitcoinUnits in the wallet's selected display
    //! unit (consistent with the rest of the GUI), falling back to GRC.
    QString FormatAmount(int64_t nValue) const;

    //! Build the human-readable decoded-PSGT description shown in the decoded view.
    QString DescribePSGT(const PartiallySignedTransaction& psgt) const;

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
