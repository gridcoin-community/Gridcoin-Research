// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MULTISIGNDIALOG_H
#define BITCOIN_QT_MULTISIGNDIALOG_H

#include <QDialog>

#include <cstdint>
#include <vector>

namespace Ui {
    class MultisignPSGTDialog;
}
class WalletModel;

namespace interfaces {
class PSGTPoolContext;
struct PSGTDescription;
} // namespace interfaces

/**
 * Dialog for working with Partially Signed Gridcoin Transactions (PSGT):
 * load a base64 PSGT, inspect its inputs/outputs and signing status, sign the
 * inputs this wallet holds keys for, combine co-signers' PSGTs, and finalize a
 * fully-signed PSGT into a broadcast-ready raw transaction.
 *
 * All PSGT operations run node-side behind interfaces::PSGTPoolContext (Phase
 * 1d-v); the dialog holds the working PSGT as serialized bytes and never a core
 * PartiallySignedTransaction. Base64 text I/O stays here.
 */
class MultisignPSGTDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MultisignPSGTDialog(QWidget* parent = nullptr);
    ~MultisignPSGTDialog();

    void setModel(WalletModel *model);

    //! Provide the PSGT pool + workbench interface (Phase 1d-v). Every PSGT
    //! operation routes through it; must be set before the dialog is used.
    void setPSGTPoolContext(interfaces::PSGTPoolContext* context);

    //! Adopt the serialized PSGT as the working PSGT and write its base64 back to
    //! the input and result boxes. The entry point the PSGT Pool page calls to
    //! open a pooled PSGT in this dialog without a base64 text round-trip.
    void setWorking(const std::vector<unsigned char>& psgt_bytes);

private:
    Ui::MultisignPSGTDialog *ui;
    WalletModel *model;
    interfaces::PSGTPoolContext *m_psgt_context;

    //! The working PSGT as serialized bytes (interfaces::PSGTBytes). The base64
    //! text box stays the user-editable source of truth: every handler refreshes
    //! this from the box via syncWorkingFromInput() before acting, and the
    //! operations that produce a new PSGT (sign/combine) push the result back via
    //! setWorking().
    std::vector<unsigned char> m_working;

    //! Decode the input box into m_working (base64 -> bytes, validated by the
    //! interface). On failure, reports the error and leaves m_working untouched.
    bool syncWorkingFromInput();

    //! True iff this wallet contributed at least one cryptographically valid
    //! signature to the working PSGT (the pool-submit precondition).
    bool walletHasSignature() const;

    //! Render a described PSGT into the read-only decoded view (plus, when the
    //! interface is available, the "wallet's signature present" line).
    void renderDecoded(const interfaces::PSGTDescription& description);

    //! Format a satoshi amount with BitcoinUnits in the wallet's selected display
    //! unit (consistent with the rest of the GUI), falling back to GRC.
    QString FormatAmount(int64_t nValue) const;

    //! Build the human-readable decoded-PSGT description shown in the decoded view.
    QString DescribePSGT(const interfaces::PSGTDescription& description) const;

    void setStatus(const QString& text, bool error);

private slots:
    void on_inspectButton_clicked();
    void on_signButton_clicked();
    void on_combineButton_clicked();
    void on_submitToPoolButton_clicked();
    void on_finalizeButton_clicked();
    void on_copyResultButton_clicked();
    void on_clearButton_clicked();
};

#endif // BITCOIN_QT_MULTISIGNDIALOG_H
