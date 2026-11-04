// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/rpcconsoletests.h"

#include "qt/rpcconsole.h"
#include "qt/test/interfacefakes.h"

#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QTextEdit>

#include <string>
#include <vector>

using RPCConsoleDetail::DispatchConsoleCommand;
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

// The pane and the Up-arrow history keep the redacted form of each passphrase command. No client
// model is set, so no executor runs the command.
void RPCConsoleTests::passphraseCommandsKeepNoSecretInHistoryOrPane()
{
    const std::vector<QString> commands{
        QStringLiteral("walletpassphrase ") + PHRASE_PASSWORD + QStringLiteral(" 60 true"),
        QStringLiteral("walletpassphrasechange old-") + PHRASE_PASSWORD + QStringLiteral(" new-") + PHRASE_PASSWORD,
        QStringLiteral("encryptwallet ") + PHRASE_PASSWORD,
    };

    for (const QString& command : commands) {
        RPCConsole console;
        auto* input = console.findChild<QLineEdit*>(QStringLiteral("lineEdit"));
        auto* pane = console.findChild<QTextEdit*>(QStringLiteral("messagesWidget"));
        QVERIFY(input);
        QVERIFY(pane);

        input->setText(command);
        QTest::keyClick(input, Qt::Key_Return);
        QTest::keyClick(input, Qt::Key_Up);

        QVERIFY2(input->text().endsWith(QStringLiteral(" (arguments not shown)")), qPrintable(input->text()));
        QVERIFY2(!input->text().contains(PHRASE_PASSWORD), qPrintable(input->text()));
        QVERIFY(!pane->toPlainText().contains(PHRASE_PASSWORD));
    }
}

// The console line is cleansed without going through the secure pool, which refuses a buffer over
// 256 KiB: a pasted raw transaction can be that long.
void RPCConsoleTests::handlesLineLongerThanSecurePool()
{
    const QString hex(300 * 1024, QLatin1Char('a'));

    const QString parsed = QStringLiteral("sendrawtransaction ") + hex;
    QCOMPARE(RedactSensitiveArgs(parsed), parsed);

    const QString unparsed = QStringLiteral("sendrawtransaction \"") + hex;
    QCOMPARE(RPCConsoleDetail::LeadingCommandToken(unparsed), std::string("sendrawtransaction"));
    QCOMPARE(RedactSensitiveArgs(unparsed), unparsed);
}

void RPCConsoleTests::routesPassphraseCommandsOverSecretPath()
{
    qt_test::FakeNode node;
    std::vector<std::string> args{"walletpassphrase", "hunter2", "60", "true"};

    DispatchConsoleCommand(node, args);

    QVERIFY(node.m_console_calls.empty());
    QCOMPARE(node.m_secret_calls.size(), size_t{1});

    const qt_test::FakeNode::SecretCall& call = node.m_secret_calls.front();
    QCOMPARE(call.method, std::string("walletpassphrase"));
    QCOMPARE(call.passphrases, (std::vector<std::string>{"hunter2"}));
    QCOMPARE(call.args, (std::vector<std::string>{"60", "true"}));

    // The parsed copy is overwritten in place, and a short passphrase still went over in the
    // SecureString's secure buffer rather than its inline storage.
    QCOMPARE(args[1], std::string(7, '\0'));
    QCOMPARE(call.passphrase_capacities.size(), size_t{1});
    QVERIFY(call.passphrase_capacities[0] > SecureString().capacity());
}

void RPCConsoleTests::routesPassphraseChangeWithTwoSecrets()
{
    qt_test::FakeNode node;
    std::vector<std::string> args{"walletpassphrasechange", "old pw", "new pw"};

    DispatchConsoleCommand(node, args);

    QVERIFY(node.m_console_calls.empty());
    QCOMPARE(node.m_secret_calls.size(), size_t{1});

    const qt_test::FakeNode::SecretCall& call = node.m_secret_calls.front();
    QCOMPARE(call.method, std::string("walletpassphrasechange"));
    QCOMPARE(call.passphrases, (std::vector<std::string>{"old pw", "new pw"}));
    QVERIFY(call.args.empty());

    QCOMPARE(args[1], std::string(6, '\0'));
    QCOMPARE(args[2], std::string(6, '\0'));
}

void RPCConsoleTests::routesEncryptWalletOverSecretPath()
{
    qt_test::FakeNode node;
    std::vector<std::string> args{"encryptwallet", "p\xc3\xa4 ss"};

    DispatchConsoleCommand(node, args);

    QVERIFY(node.m_console_calls.empty());
    QCOMPARE(node.m_secret_calls.size(), size_t{1});
    QCOMPARE(node.m_secret_calls.front().passphrases, (std::vector<std::string>{"p\xc3\xa4 ss"}));
    QVERIFY(node.m_secret_calls.front().args.empty());
    QCOMPARE(args[1], std::string(6, '\0'));
}

void RPCConsoleTests::routesOrdinaryCommandOverPlainPath()
{
    qt_test::FakeNode node;
    node.m_console_result.ok = true;
    node.m_console_result.output = "42";
    std::vector<std::string> args{"getblockcount"};

    const interfaces::RpcConsoleResult result = DispatchConsoleCommand(node, args);

    QVERIFY(node.m_secret_calls.empty());
    QCOMPARE(node.m_console_calls.size(), size_t{1});
    QCOMPARE(node.m_console_calls.front().first, std::string("getblockcount"));
    QVERIFY(node.m_console_calls.front().second.empty());
    QVERIFY(result.ok);
    QCOMPARE(result.output, std::string("42"));
}

// The node refuses the count; the GUI must not fall back to the plain path.
void RPCConsoleTests::passphraseCommandWithoutArgsStaysOnSecretPath()
{
    qt_test::FakeNode node;
    std::vector<std::string> args{"walletpassphrase"};

    DispatchConsoleCommand(node, args);

    QVERIFY(node.m_console_calls.empty());
    QCOMPARE(node.m_secret_calls.size(), size_t{1});
    QCOMPARE(node.m_secret_calls.front().method, std::string("walletpassphrase"));
    QVERIFY(node.m_secret_calls.front().passphrases.empty());
    QVERIFY(node.m_secret_calls.front().args.empty());
}

// A passphrase command carries its own passphrase, so a refusal for the wallet's keys is not one the
// console's unlock prompt answers, and the command line is not kept to run again. An ordinary
// command's refusal still reaches the console.
void RPCConsoleTests::passphraseCommandIsNeverRunAgainUnderUnlock()
{
    qt_test::FakeNode node;
    node.m_secret_result.wallet_unlock_needed = true;
    node.m_console_result.wallet_unlock_needed = true;

    std::vector<std::string> secret_args{"walletpassphrase", "hunter2", "60"};
    QVERIFY(!DispatchConsoleCommand(node, secret_args).wallet_unlock_needed);

    std::vector<std::string> plain_args{"sendtoaddress", "SBvCq9hD7WtYpRk3mXnJ5fLgZ2aEu8VsQo", "1"};
    QVERIFY(DispatchConsoleCommand(node, plain_args).wallet_unlock_needed);
}
