// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "multisigndialog.h"
#include "ui_multisigndialog.h"

#include "guiutil.h"
#include "bitcoinunits.h"
#include "qt/decoration.h"
#include "walletmodel.h"

#include <psgt.h>
#include <key_io.h>
#include <main.h>
#include <script/standard.h>
#include <streams.h>
#include <sync.h>
#include <util.h>
#include <util/strencodings.h>
#include <version.h>
#include <wallet/wallet.h>

#include <string>
#include <vector>

#include <QApplication>
#include <QClipboard>
#include <QString>

extern CWallet* pwalletMain;

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

//! Format a satoshi amount as "<value> GRC".
QString FormatAmount(int64_t nValue)
{
    return QString::fromStdString(FormatMoney(nValue)) + " GRC";
}

//! Build a human-readable description of a decoded PSGT (mirrors the data
//! surfaced by the decodepsgt RPC, formatted for display).
QString DescribePSGT(const PartiallySignedTransaction& psgt)
{
    QString out;
    CTransaction ctx(psgt.tx);

    out += QString("Transaction id: %1\n").arg(QString::fromStdString(ctx.GetHash().GetHex()));
    out += QString("Version: %1   Time: %2   Locktime: %3\n")
               .arg(psgt.tx.nVersion)
               .arg((qlonglong)psgt.tx.nTime)
               .arg((qlonglong)psgt.tx.nLockTime);
    out += QString("Inputs: %1   Outputs: %2\n\n")
               .arg(psgt.tx.vin.size())
               .arg(psgt.tx.vout.size());

    unsigned int signed_inputs = 0;

    out += "Inputs:\n";
    for (unsigned int i = 0; i < psgt.tx.vin.size(); ++i)
    {
        const CTxIn& txin = psgt.tx.vin[i];
        QString status;
        QString amount = "(prev tx not loaded)";

        if (i < psgt.inputs.size())
        {
            const PSGTInput& input = psgt.inputs[i];

            if (!input.non_witness_utxo.IsNull()
                && txin.prevout.n < input.non_witness_utxo.vout.size())
            {
                amount = FormatAmount(input.non_witness_utxo.vout[txin.prevout.n].nValue);
            }

            if (PSGTInputSigned(input))
            {
                status = "finalized";
                ++signed_inputs;
            }
            else if (!input.partial_sigs.empty())
            {
                status = QString("%1 partial sig(s)").arg(input.partial_sigs.size());
            }
            else
            {
                status = "unsigned";
            }
        }
        else
        {
            status = "no metadata";
        }

        out += QString("  [%1] %2:%3   %4   %5\n")
                   .arg(i)
                   .arg(QString::fromStdString(txin.prevout.hash.GetHex()))
                   .arg(txin.prevout.n)
                   .arg(amount)
                   .arg(status);

        // Multisig "image": the P2SH redeem-script hash (as an address) and the
        // m-of-n threshold. This is the key a Phase II PSGT pool (#2910) indexes
        // on; surfacing it here builds on the per-input signing-status display.
        if (i < psgt.inputs.size() && !psgt.inputs[i].redeem_script.empty())
        {
            const CScript& redeem = psgt.inputs[i].redeem_script;
            const QString image = QString::fromStdString(EncodeDestination(CScriptID(redeem)));

            txnouttype rtype;
            std::vector<std::vector<unsigned char>> rsols;
            if (Solver(redeem, rtype, rsols) && rtype == TX_MULTISIG && rsols.size() >= 3)
            {
                out += QString("        multisig %1-of-%2   image P2SH:%3\n")
                           .arg((int)rsols.front()[0])
                           .arg((int)rsols.size() - 2)
                           .arg(image);
            }
            else
            {
                out += QString("        image P2SH:%1\n").arg(image);
            }
        }
    }

    out += "\nOutputs:\n";
    for (unsigned int i = 0; i < psgt.tx.vout.size(); ++i)
    {
        const CTxOut& txout = psgt.tx.vout[i];
        QString dest = "(non-standard)";

        txnouttype type;
        std::vector<CTxDestination> addresses;
        int nRequired;
        if (ExtractDestinations(txout.scriptPubKey, type, addresses, nRequired))
        {
            QStringList addrs;
            for (const CTxDestination& addr : addresses)
                addrs << QString::fromStdString(EncodeDestination(addr));
            dest = addrs.join(", ");
            if (nRequired > 1)
                dest = QString("%1-of-%2 [%3]").arg(nRequired).arg(addresses.size()).arg(dest);
        }

        out += QString("  [%1] %2   %3\n").arg(i).arg(dest).arg(FormatAmount(txout.nValue));
    }

    // Completeness: attempt finalization on a copy (same approach the RPC uses).
    PartiallySignedTransaction copy = psgt;
    bool complete = FinalizePSGT(copy);

    out += QString("\nSigned inputs: %1/%2   Complete: %3\n")
               .arg(signed_inputs)
               .arg(psgt.tx.vin.size())
               .arg(complete ? "yes" : "no");

    return out;
}

} // namespace

MultisignPSGTDialog::MultisignPSGTDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::MultisignPSGTDialog)
    , model(nullptr)
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

void MultisignPSGTDialog::setStatus(const QString& text, bool error)
{
    ui->statusLabel->setStyleSheet(error ? "QLabel { color: red; }" : "QLabel { color: green; }");
    ui->statusLabel->setText(text);
}

bool MultisignPSGTDialog::syncWorkingFromInput()
{
    std::string b64 = CleanBase64(ui->psgtInEdit->toPlainText());
    if (b64.empty())
    {
        setStatus(tr("Paste a base64 PSGT first."), true);
        return false;
    }

    // Decode into a temporary so a failure never leaves m_working half-updated.
    PartiallySignedTransaction decoded;
    std::string error;
    if (!DecodeRawPSGT(decoded, b64, error))
    {
        setStatus(tr("Could not decode PSGT: %1").arg(QString::fromStdString(error)), true);
        return false;
    }

    m_working = decoded;
    return true;
}

void MultisignPSGTDialog::setWorking(const PartiallySignedTransaction& psgt)
{
    m_working = psgt;

    std::vector<unsigned char> data = SerializePSGT(m_working);
    QString b64 = QString::fromStdString(EncodeBase64(data.data(), data.size()));

    // The input box stays the working PSGT so it can be re-signed/combined/finalized;
    // the result box mirrors it for copy-out. The decoded view is refreshed
    // explicitly by the caller (no hidden re-decode here).
    ui->psgtInEdit->setPlainText(b64);
    ui->resultOutEdit->setPlainText(b64);
}

bool MultisignPSGTDialog::walletHasSignature(const PartiallySignedTransaction& psgt) const
{
    if (!pwalletMain)
        return false;

    for (const PSGTInput& input : psgt.inputs)
    {
        for (const auto& sig : input.partial_sigs)
        {
            if (pwalletMain->HaveKey(sig.first))
                return true;
        }
    }
    return false;
}

void MultisignPSGTDialog::showDecoded(const PartiallySignedTransaction& psgt)
{
    QString text = DescribePSGT(psgt);

    // "Submit to pool" (Phase II, #2910) will gate on this wallet having
    // contributed at least one signature; surface the precondition now.
    if (pwalletMain)
    {
        text += QString("This wallet's signature present: %1\n")
                    .arg(walletHasSignature(psgt) ? tr("yes") : tr("no"));
    }

    ui->decodedView->setPlainText(text);
}

void MultisignPSGTDialog::on_inspectButton_clicked()
{
    if (!syncWorkingFromInput())
        return;

    // Inspect is read-only: it refreshes the decoded view but never rewrites the
    // input box.
    showDecoded(m_working);
    setStatus(tr("Decoded %1 input(s), %2 output(s).")
                  .arg(m_working.tx.vin.size())
                  .arg(m_working.tx.vout.size()),
              false);
}

void MultisignPSGTDialog::on_signButton_clicked()
{
    if (!model || !pwalletMain)
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

    LOCK2(cs_main, pwalletMain->cs_wallet);

    PartiallySignedTransaction psgt = m_working;

    // Fill in non_witness_utxo and redeem_script for inputs that need them
    // (mirrors walletprocesspsgt in src/rpc/psgt.cpp).
    for (unsigned int i = 0; i < psgt.inputs.size(); ++i)
    {
        if (psgt.inputs[i].non_witness_utxo.IsNull())
        {
            const uint256& prevHash = psgt.tx.vin[i].prevout.hash;
            CTransaction prevTx;
            uint256 hashBlock;
            if (GetTransaction(prevHash, prevTx, hashBlock))
                psgt.inputs[i].non_witness_utxo = prevTx;
        }

        if (psgt.inputs[i].redeem_script.empty() && !psgt.inputs[i].non_witness_utxo.IsNull())
        {
            const COutPoint& prevout = psgt.tx.vin[i].prevout;
            if (prevout.n < psgt.inputs[i].non_witness_utxo.vout.size())
            {
                const CScript& scriptPubKey = psgt.inputs[i].non_witness_utxo.vout[prevout.n].scriptPubKey;
                if (scriptPubKey.IsPayToScriptHash())
                {
                    CScriptID scriptID(uint160(std::vector<unsigned char>(
                        scriptPubKey.begin() + 2, scriptPubKey.begin() + 22)));
                    CScript redeemScript;
                    if (pwalletMain->GetCScript(scriptID, redeemScript))
                        psgt.inputs[i].redeem_script = redeemScript;
                }
            }
        }
    }

    // Snapshot per-input signing state so we can report how many inputs this
    // wallet actually signed. We cannot use SignPSGTInput's bool return: for a
    // multisig input it is true whenever *any* co-signer signature is present
    // (psgt.cpp), so it does not mean "this wallet signed this input".
    std::vector<size_t> sigs_before(psgt.inputs.size());
    std::vector<bool> final_before(psgt.inputs.size());
    for (unsigned int i = 0; i < psgt.inputs.size(); ++i)
    {
        sigs_before[i] = psgt.inputs[i].partial_sigs.size();
        final_before[i] = !psgt.inputs[i].final_script_sig.empty();
    }

    // Sign each input with wallet keys.
    for (unsigned int i = 0; i < psgt.inputs.size(); ++i)
    {
        SignPSGTInput(*pwalletMain, psgt, i, SIGHASH_ALL);
    }

    // Count inputs this wallet contributed to this round: a new partial sig, or
    // a newly assembled final scriptSig (single-sig path).
    unsigned int signed_now = 0;
    for (unsigned int i = 0; i < psgt.inputs.size(); ++i)
    {
        const bool final_now = !psgt.inputs[i].final_script_sig.empty();
        if (psgt.inputs[i].partial_sigs.size() > sigs_before[i]
            || (final_now && !final_before[i]))
        {
            ++signed_now;
        }
    }

    // Determine completeness on a copy, then update output metadata.
    PartiallySignedTransaction psgt_copy = psgt;
    bool complete = FinalizePSGT(psgt_copy);

    for (unsigned int i = 0; i < psgt.outputs.size(); ++i)
    {
        UpdatePSGTOutput(*pwalletMain, psgt, i);
    }

    // Distinguish a genuine signing failure from "no key for this input": any
    // key still required (AnalyzePSGT::missing_sigs, which already accounts for
    // the multisig threshold and clears once enough sigs exist) that this wallet
    // actually holds is an input we should have signed but didn't.
    PSGTAnalysis analysis = AnalyzePSGT(psgt);
    unsigned int could_not_sign = 0;
    for (const PSGTInputAnalysis& ia : analysis.inputs)
    {
        for (const CKeyID& keyid : ia.missing_sigs)
        {
            if (pwalletMain->HaveKey(keyid))
            {
                ++could_not_sign;
                break; // count each input at most once
            }
        }
    }

    // The signed PSGT becomes the new working PSGT so it can be combined/finalized.
    setWorking(psgt);
    showDecoded(psgt);

    QString msg = tr("Signed %1 input(s) with this wallet. Complete: %2.")
                      .arg(signed_now)
                      .arg(complete ? tr("yes") : tr("no"));
    if (could_not_sign > 0)
    {
        msg += " " + tr("%n input(s) the wallet holds keys for could not be signed.",
                        nullptr, could_not_sign);
        setStatus(msg, true);
    }
    else
    {
        setStatus(msg, !complete);
    }
}

void MultisignPSGTDialog::on_combineButton_clicked()
{
    if (!syncWorkingFromInput())
        return;

    std::vector<PartiallySignedTransaction> psgts;
    psgts.push_back(m_working);

    // Plain split (no Qt::SkipEmptyParts, which is Qt >= 5.14 only): the loop
    // below skips empty/whitespace-only lines after CleanBase64.
    const QStringList lines = ui->combineInEdit->toPlainText().split('\n');
    for (const QString& line : lines)
    {
        std::string b64 = CleanBase64(line);
        if (b64.empty())
            continue;

        PartiallySignedTransaction other;
        std::string error;
        if (!DecodeRawPSGT(other, b64, error))
        {
            setStatus(tr("Could not decode a co-signer PSGT: %1").arg(QString::fromStdString(error)), true);
            return;
        }
        psgts.push_back(other);
    }

    if (psgts.size() < 2)
    {
        setStatus(tr("Add at least one co-signer PSGT to combine."), true);
        return;
    }

    PartiallySignedTransaction merged;
    if (!CombinePSGTs(merged, psgts))
    {
        setStatus(tr("PSGTs do not refer to the same transaction."), true);
        return;
    }

    setWorking(merged);
    showDecoded(merged);

    setStatus(tr("Combined %1 PSGT(s).").arg(psgts.size()), false);
}

void MultisignPSGTDialog::on_finalizeButton_clicked()
{
    if (!syncWorkingFromInput())
        return;

    // Finalize on a copy so the working PSGT (and the input box) keep the partial
    // form if extraction succeeds; finalize never rewrites the input box.
    PartiallySignedTransaction psgt = m_working;

    CMutableTransaction mtx;
    if (!FinalizeAndExtractPSGT(psgt, mtx))
    {
        showDecoded(m_working);
        setStatus(tr("PSGT is not complete yet; cannot finalize."), true);
        return;
    }

    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << mtx;
    QString hex = QString::fromStdString(HexStr(ssTx));

    ui->resultOutEdit->setPlainText(hex);
    setStatus(tr("Finalized. Raw transaction hex is ready to broadcast (e.g. via sendrawtransaction)."), false);
}

void MultisignPSGTDialog::on_copyResultButton_clicked()
{
    QApplication::clipboard()->setText(ui->resultOutEdit->toPlainText());
}

void MultisignPSGTDialog::on_clearButton_clicked()
{
    m_working = PartiallySignedTransaction();
    ui->psgtInEdit->clear();
    ui->combineInEdit->clear();
    ui->decodedView->clear();
    ui->resultOutEdit->clear();
    ui->statusLabel->clear();
    ui->psgtInEdit->setFocus();
}
