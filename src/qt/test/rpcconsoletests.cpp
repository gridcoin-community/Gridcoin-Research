// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/rpcconsoletests.h"

#include "qt/rpcconsole.h"

#include <QLineEdit>
#include <QPushButton>
#include <QString>

using RPCConsoleDetail::RedactSensitiveArgs;

namespace {
// Placeholder values in the shape claimhtlc takes: htlc_txid, vout,
// preimage_hex, destination_addr.
const QString HTLC_TXID = QStringLiteral("4f1c2b7a9e0d3c5b8a6f2e1d0c9b8a7f6e5d4c3b2a19087f6e5d4c3b2a190876");
const QString PREIMAGE = QStringLiteral("a3b5c7d9e1f30517293b4d5f61728394a5b6c7d8e9fa0b1c2d3e4f5061728394");
const QString DEST_ADDR = QStringLiteral("SBvCq9hD7WtYpRk3mXnJ5fLgZ2aEu8VsQo");
const QString PHRASE_PASSWORD = QStringLiteral("tangerine-otter-41");
} // namespace

void RPCConsoleTests::redactsListedCommand()
{
    // A command that was already listed, so this pins the form the new
    // entries are expected to produce.
    QCOMPARE(RedactSensitiveArgs(QStringLiteral("walletpassphrase hunter2 60")),
             QStringLiteral("walletpassphrase (arguments not shown)"));
}

void RPCConsoleTests::leavesOrdinaryCommandUnchanged()
{
    QCOMPARE(RedactSensitiveArgs(QStringLiteral("getblockcount")),
             QStringLiteral("getblockcount"));

    const QString getblock = QStringLiteral("getblock ") + HTLC_TXID;
    QCOMPARE(RedactSensitiveArgs(getblock), getblock);
}

void RPCConsoleTests::redactsClaimHtlcPreimage()
{
    const QString cmd = QStringLiteral("claimhtlc ") + HTLC_TXID + QStringLiteral(" 0 ")
                        + PREIMAGE + QStringLiteral(" ") + DEST_ADDR;
    const QString shown = RedactSensitiveArgs(cmd);

    QVERIFY2(!shown.contains(PREIMAGE), qPrintable(shown));
    QCOMPARE(shown, QStringLiteral("claimhtlc (arguments not shown)"));
}

void RPCConsoleTests::redactsUnparseableClaimHtlc()
{
    // The unbalanced quote makes parseCommandLine reject the line, so this
    // goes through the leading-token fallback.
    const QString cmd = QStringLiteral("claimhtlc ") + HTLC_TXID + QStringLiteral(" 0 \"")
                        + PREIMAGE + QStringLiteral(" ") + DEST_ADDR;
    const QString shown = RedactSensitiveArgs(cmd);

    QVERIFY2(!shown.contains(PREIMAGE), qPrintable(shown));
    QCOMPARE(shown, QStringLiteral("claimhtlc (arguments not shown)"));
}

void RPCConsoleTests::redactsMakeSeedPhrasePassword()
{
    const QString cmd = QStringLiteral("makeseedphrase \"") + PHRASE_PASSWORD + QStringLiteral("\"");
    const QString shown = RedactSensitiveArgs(cmd);

    QVERIFY2(!shown.contains(PHRASE_PASSWORD), qPrintable(shown));
    QCOMPARE(shown, QStringLiteral("makeseedphrase (arguments not shown)"));
}

// The console's input must not keep a command it has taken on its undo stack:
// the history keeps a redacted copy, and the input must not keep the original.
// No client model is set, so no executor runs the command.
void RPCConsoleTests::enteredCommandLeavesNoUndoStep()
{
    RPCConsole console;
    auto* input = console.findChild<QLineEdit*>(QStringLiteral("lineEdit"));
    QVERIFY(input);

    QTest::keyClicks(input, QStringLiteral("walletpassphrase ") + PHRASE_PASSWORD + QStringLiteral(" 60"));
    QTest::keyClick(input, Qt::Key_Return);
    QVERIFY(input->text().isEmpty());

    input->undo();
    QVERIFY2(!input->text().contains(PHRASE_PASSWORD), qPrintable(input->text()));
}

void RPCConsoleTests::clearedInputLeavesNoUndoStep()
{
    RPCConsole console;
    auto* input = console.findChild<QLineEdit*>(QStringLiteral("lineEdit"));
    auto* clear_button = console.findChild<QPushButton*>(QStringLiteral("clearButton"));
    QVERIFY(input);
    QVERIFY(clear_button);

    QTest::keyClicks(input, QStringLiteral("walletpassphrase ") + PHRASE_PASSWORD + QStringLiteral(" 60"));
    clear_button->click();
    QVERIFY(input->text().isEmpty());

    input->undo();
    QVERIFY2(!input->text().contains(PHRASE_PASSWORD), qPrintable(input->text()));
}
