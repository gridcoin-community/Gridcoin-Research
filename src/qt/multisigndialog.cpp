// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "multisigndialog.h"
#include "ui_multisigndialog.h"

#include "bitcoinunits.h"
#include "guiutil.h"
#include "interfaces/psgt.h"
#include "optionsmodel.h"
#include "qt/decoration.h"
#include "walletmodel.h"

#include <util/strencodings.h>

#include <string>
#include <vector>

#include <QApplication>
#include <QClipboard>
#include <QString>

namespace {

//! Strip all whitespace (newlines/spaces from pasted text) so DecodeBase64 sees
//! a clean base64 string.
std::string CleanBase64(const QString& in)
{
    std::string out;
    out.reserve(in.size());
    for (const QChar& c : in)
    {
        if (!c.isSpace())
            out.push_back(c.toLatin1());
    }
    return out;
}

} // namespace

QString MultisignPSGTDialog::FormatAmount(int64_t nValue) const
{
    // Match the rest of the GUI: format in the wallet's selected display unit
    // (GRC/mGRC/uBTC) with thin-space grouping, rather than core FormatMoney().
    int unit = BitcoinUnits::BTC; // BTC == GRC display
    if (model && model->getOptionsModel())
        unit = model->getOptionsModel()->getDisplayUnit();

    return BitcoinUnits::formatWithUnit(unit, nValue);
}

//! Build a human-readable description of a decoded PSGT (mirrors the data
//! surfaced by the decodepsgt RPC, formatted for display) from the node-side
//! value description.
QString MultisignPSGTDialog::DescribePSGT(const interfaces::PSGTDescription& d) const
{
    QString out;

    out += tr("Transaction id: %1\n").arg(QString::fromStdString(d.txid));
    out += tr("Version: %1   Time: %2   Locktime: %3\n")
               .arg(d.version)
               .arg((qlonglong)d.time)
               .arg((qlonglong)d.lock_time);
    out += tr("Inputs: %1   Outputs: %2\n\n")
               .arg(d.inputs.size())
               .arg(d.outputs.size());

    out += tr("Inputs:\n");
    for (unsigned int i = 0; i < d.inputs.size(); ++i)
    {
        const interfaces::PSGTInputInfo& in = d.inputs[i];
        QString status;
        QString amount = tr("(prev tx not loaded)");

        if (in.has_metadata)
        {
            switch (in.amount_state)
            {
            case interfaces::PSGTInputAmountState::HAVE:       amount = FormatAmount(in.amount);   break;
            case interfaces::PSGTInputAmountState::MISMATCH:   amount = tr("(prev tx mismatch)");  break;
            case interfaces::PSGTInputAmountState::NOT_LOADED: amount = tr("(prev tx not loaded)"); break;
            }

            switch (in.sig_state)
            {
            case interfaces::PSGTInputSigState::FINALIZED: status = tr("finalized"); break;
            case interfaces::PSGTInputSigState::PARTIAL:   status = tr("%n partial sig(s)", nullptr, in.partial_sig_count); break;
            case interfaces::PSGTInputSigState::UNSIGNED:  status = tr("unsigned"); break;
            }
        }
        else
        {
            status = tr("no metadata");
        }

        out += QString("  [%1] %2:%3   %4   %5\n")
                   .arg(i)
                   .arg(QString::fromStdString(in.prevout_hash))
                   .arg(in.prevout_n)
                   .arg(amount)
                   .arg(status);

        // Multisig "image": the P2SH redeem-script hash (as an address) and the
        // m-of-n threshold, next to the raw Hash160 the pool keys on.
        if (in.has_redeem)
        {
            const QString image = QString::fromStdString(in.p2sh_address);
            const QString imageHash = QString::fromStdString(in.p2sh_hash_hex);

            if (in.is_multisig)
            {
                out += tr("        multisig %1-of-%2   image P2SH:%3 (hash %4)\n")
                           .arg(in.multisig_m)
                           .arg(in.multisig_n)
                           .arg(image)
                           .arg(imageHash);
            }
            else
            {
                out += tr("        image P2SH:%1 (hash %2)\n").arg(image).arg(imageHash);
            }
        }
    }

    out += tr("\nOutputs:\n");
    for (unsigned int i = 0; i < d.outputs.size(); ++i)
    {
        const interfaces::PSGTOutputInfo& o = d.outputs[i];
        QString dest = tr("(non-standard)");

        if (o.is_standard)
        {
            QStringList addrs;
            for (const std::string& addr : o.destinations)
                addrs << QString::fromStdString(addr);
            dest = addrs.join(", ");
            if (o.n_required > 1)
                dest = QString("%1-of-%2 [%3]").arg(o.n_required).arg(o.destinations.size()).arg(dest);
        }

        out += QString("  [%1] %2   %3\n").arg(i).arg(dest).arg(FormatAmount(o.amount));
    }

    out += tr("\nSigned inputs: %1/%2   Complete: %3\n")
               .arg(d.signed_input_count)
               .arg(d.inputs.size())
               .arg(d.complete ? tr("yes") : tr("no"));

    return out;
}

MultisignPSGTDialog::MultisignPSGTDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::MultisignPSGTDialog)
    , model(nullptr)
    , m_psgt_context(nullptr)
{
    ui->setupUi(this);

    resize(GRC::ScaleSize(this, width(), height()));

    ui->psgtInEdit->setFont(GUIUtil::bitcoinAddressFont());
    ui->resultOutEdit->setFont(GUIUtil::bitcoinAddressFont());
    ui->decodedView->setFont(GUIUtil::bitcoinAddressFont());
}

MultisignPSGTDialog::~MultisignPSGTDialog()
{
    delete ui;
}

void MultisignPSGTDialog::setModel(WalletModel *model)
{
    this->model = model;
}

void MultisignPSGTDialog::setPSGTPoolContext(interfaces::PSGTPoolContext* context)
{
    m_psgt_context = context;
}

void MultisignPSGTDialog::setStatus(const QString& text, bool error)
{
    ui->statusLabel->setStyleSheet(error ? "QLabel { color: red; }" : "QLabel { color: green; }");
    ui->statusLabel->setText(text);
}

bool MultisignPSGTDialog::syncWorkingFromInput()
{
    if (!m_psgt_context)
    {
        setStatus(tr("PSGT support is not available."), true);
        return false;
    }

    std::string b64 = CleanBase64(ui->psgtInEdit->toPlainText());
    if (b64.empty())
    {
        setStatus(tr("Paste a base64 PSGT first."), true);
        return false;
    }

    bool invalid = false;
    const std::vector<unsigned char> bytes = DecodeBase64(b64.c_str(), &invalid);
    if (invalid)
    {
        setStatus(tr("Could not decode PSGT: %1").arg(tr("invalid base64")), true);
        return false;
    }

    // Validate + canonicalize node-side into a temporary so a failure never
    // leaves m_working half-updated.
    interfaces::PSGTDecodeResult res = m_psgt_context->decodePSGT(bytes);
    if (!res.ok)
    {
        setStatus(tr("Could not decode PSGT: %1").arg(QString::fromStdString(res.error)), true);
        return false;
    }

    m_working = res.psgt;
    return true;
}

void MultisignPSGTDialog::setWorking(const std::vector<unsigned char>& psgt_bytes)
{
    m_working = psgt_bytes;

    const QString b64 = QString::fromStdString(EncodeBase64(m_working.data(), m_working.size()));

    // The input box stays the working PSGT so it can be re-signed/combined/finalized;
    // the result box mirrors it for copy-out. The decoded view is refreshed
    // explicitly by the caller (no hidden re-decode here).
    ui->psgtInEdit->setPlainText(b64);
    ui->resultOutEdit->setPlainText(b64);
}

bool MultisignPSGTDialog::walletHasSignature() const
{
    return m_psgt_context && m_psgt_context->walletHasSignature(m_working);
}

void MultisignPSGTDialog::renderDecoded(const interfaces::PSGTDescription& description)
{
    QString text = DescribePSGT(description);

    if (m_psgt_context)
    {
        text += tr("This wallet's signature present: %1\n")
                    .arg(walletHasSignature() ? tr("yes") : tr("no"));
    }

    ui->decodedView->setPlainText(text);
}

void MultisignPSGTDialog::on_inspectButton_clicked()
{
    if (!syncWorkingFromInput())
        return;

    // Inspect is read-only: it refreshes the decoded view but never rewrites the
    // input box.
    const interfaces::PSGTDescription description = m_psgt_context->describePSGT(m_working);
    renderDecoded(description);
    setStatus(tr("Decoded %1 input(s), %2 output(s).")
                  .arg(description.inputs.size())
                  .arg(description.outputs.size()),
              false);
}

void MultisignPSGTDialog::on_signButton_clicked()
{
    if (!model || !m_psgt_context)
    {
        setStatus(tr("Wallet is not available."), true);
        return;
    }

    if (!syncWorkingFromInput())
        return;

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if (!ctx.isValid())
    {
        setStatus(tr("Wallet unlock was cancelled."), true);
        return;
    }

    interfaces::PSGTSignResult res = m_psgt_context->signPSGT(m_working);
    if (!res.ok)
    {
        setStatus(tr("Signing failed: %1").arg(QString::fromStdString(res.error)), true);
        return;
    }

    // The signed PSGT becomes the new working PSGT so it can be combined/finalized.
    setWorking(res.psgt);
    renderDecoded(m_psgt_context->describePSGT(m_working));

    QString msg = tr("Signed %1 input(s) with this wallet. Complete: %2.")
                      .arg(res.signed_now)
                      .arg(res.complete ? tr("yes") : tr("no"));
    if (res.needs_data > 0)
    {
        msg += " " + tr("%n input(s) still need a previous transaction or redeem "
                        "script before they can be signed.", nullptr, res.needs_data);
    }
    if (res.could_not_sign > 0)
    {
        msg += " " + tr("%n input(s) the wallet holds keys for could not be signed.",
                        nullptr, res.could_not_sign);
    }
    setStatus(msg, res.could_not_sign > 0 || !res.complete);
}

void MultisignPSGTDialog::on_combineButton_clicked()
{
    if (!syncWorkingFromInput())
        return;

    std::vector<std::vector<unsigned char>> psgts;
    psgts.push_back(m_working);

    // Plain split (no Qt::SkipEmptyParts, which is Qt >= 5.14 only): the loop
    // below skips empty/whitespace-only lines after CleanBase64.
    const QStringList lines = ui->combineInEdit->toPlainText().split('\n');
    for (const QString& line : lines)
    {
        std::string b64 = CleanBase64(line);
        if (b64.empty())
            continue;

        bool invalid = false;
        std::vector<unsigned char> bytes = DecodeBase64(b64.c_str(), &invalid);
        if (invalid)
        {
            setStatus(tr("Could not decode a co-signer PSGT: %1").arg(tr("invalid base64")), true);
            return;
        }
        psgts.push_back(std::move(bytes));
    }

    if (psgts.size() < 2)
    {
        setStatus(tr("Add at least one co-signer PSGT to combine."), true);
        return;
    }

    interfaces::PSGTCombineResult res = m_psgt_context->combinePSGTs(psgts);
    if (!res.ok)
    {
        // A non-empty error is a decode failure of one of the inputs; an empty
        // error means the PSGTs did not refer to the same transaction.
        if (!res.error.empty())
            setStatus(tr("Could not decode a co-signer PSGT: %1").arg(QString::fromStdString(res.error)), true);
        else
            setStatus(tr("PSGTs do not refer to the same transaction."), true);
        return;
    }

    setWorking(res.psgt);
    renderDecoded(m_psgt_context->describePSGT(m_working));

    setStatus(tr("Combined %1 PSGT(s).").arg(psgts.size()), false);
}

void MultisignPSGTDialog::on_submitToPoolButton_clicked()
{
    if (!syncWorkingFromInput())
        return;

    // Mirror EnsurePSGTPoolActive() (src/rpc/psgt.cpp): the pool is available
    // only once v15 has activated AND the node is not out of sync by age.
    if (!m_psgt_context->poolStatus().active)
    {
        setStatus(tr("The PSGT pool is unavailable: block v15 has not activated on "
                     "this network, or this node is still syncing."), true);
        return;
    }

    // Same precondition as the submitpsgt RPC and the pool's own acceptance:
    // do not relay a PSGT this wallet has not signed.
    if (!walletHasSignature())
    {
        setStatus(tr("Sign the PSGT with this wallet before submitting it to the pool."), true);
        return;
    }

    interfaces::PSGTSubmitResult res = m_psgt_context->submitPSGTToPool(m_working);
    if (!res.decoded)
    {
        setStatus(tr("Could not decode PSGT: %1").arg(QString::fromStdString(res.error)), true);
        return;
    }
    if (!res.validated)
    {
        setStatus(tr("The pool rejected this PSGT (%1): %2")
                      .arg(QString::fromStdString(res.reject_text))
                      .arg(QString::fromStdString(res.error)),
                  true);
        return;
    }

    switch (res.add_status)
    {
    case interfaces::PSGTPoolAddStatus::ACCEPTED_NEW:
    case interfaces::PSGTPoolAddStatus::ACCEPTED_REPLACEMENT:
        setStatus(res.add_status == interfaces::PSGTPoolAddStatus::ACCEPTED_REPLACEMENT
                      ? tr("Submitted to the pool, superseding your earlier PSGT. "
                           "Co-signers have been notified.")
                      : tr("Submitted to the pool. Co-signers have been notified."),
                  false);
        break;

    case interfaces::PSGTPoolAddStatus::DUPLICATE:
    case interfaces::PSGTPoolAddStatus::REJECTED_POOL_FULL:
    case interfaces::PSGTPoolAddStatus::REJECTED_NOT_BETTER:
    case interfaces::PSGTPoolAddStatus::REJECTED_NOT_INITIATOR:
        setStatus(tr("The pool did not accept this PSGT: %1")
                      .arg(QString::fromStdString(res.error)),
                  true);
        break;
    }
}

void MultisignPSGTDialog::on_finalizeButton_clicked()
{
    if (!syncWorkingFromInput())
        return;

    interfaces::PSGTFinalizeResult res = m_psgt_context->finalizeToRawTxHex(m_working);
    if (!res.complete)
    {
        // Non-empty error is a decode failure; empty error means "not complete".
        renderDecoded(m_psgt_context->describePSGT(m_working));
        if (!res.error.empty())
            setStatus(tr("Could not decode PSGT: %1").arg(QString::fromStdString(res.error)), true);
        else
            setStatus(tr("PSGT is not complete yet; cannot finalize."), true);
        return;
    }

    ui->resultOutEdit->setPlainText(QString::fromStdString(res.raw_tx_hex));
    setStatus(tr("Finalized. Raw transaction hex is ready to broadcast (e.g. via sendrawtransaction)."), false);
}

void MultisignPSGTDialog::on_copyResultButton_clicked()
{
    QApplication::clipboard()->setText(ui->resultOutEdit->toPlainText());
}

void MultisignPSGTDialog::on_clearButton_clicked()
{
    m_working.clear();
    ui->psgtInEdit->clear();
    ui->combineInEdit->clear();
    ui->decodedView->clear();
    ui->resultOutEdit->clear();
    ui->statusLabel->clear();
    ui->psgtInEdit->setFocus();
}
