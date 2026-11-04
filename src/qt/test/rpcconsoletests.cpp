// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/rpcconsoletests.h"

#include "chainparams.h"
#include "chainparamsbase.h"
#include "crypto/sha256.h"
#include "fs.h"
#include "interfaces/file_transfer.h"
#include "interfaces/node.h"
#include "qt/guiipcinfo.h"
#include "qt/guiipcworker.h"
#include "qt/guiutil.h"
#include "qt/optionsmodel.h"
#include "qt/rpcconsole.h"
#include "qt/test/interfacefakes.h"
#include "qt/walletmodel.h"
#include "support/lockedpool.h"
#include "uint256.h"
#include "util.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QLineEdit>
#include <QMetaObject>
#include <QProgressDialog>
#include <QPushButton>
#include <QSignalSpy>
#include <QString>
#include <QTextEdit>
#include <QTimer>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <future>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifndef WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

using RPCConsoleDetail::DispatchConsoleCommand;
using RPCConsoleDetail::RedactSensitiveArgs;

namespace {
// Placeholder values in the shape claimhtlc takes: htlc_txid, vout,
// preimage_hex, destination_addr.
const QString HTLC_TXID = QStringLiteral("4f1c2b7a9e0d3c5b8a6f2e1d0c9b8a7f6e5d4c3b2a19087f6e5d4c3b2a190876");
const QString PREIMAGE = QStringLiteral("a3b5c7d9e1f30517293b4d5f61728394a5b6c7d8e9fa0b1c2d3e4f5061728394");
const QString DEST_ADDR = QStringLiteral("SBvCq9hD7WtYpRk3mXnJ5fLgZ2aEu8VsQo");
const QString PHRASE_PASSWORD = QStringLiteral("tangerine-otter-41");

using namespace std::chrono_literals;

// "abc" and its SHA-256 as sha256sum prints it, byte by byte from the first.
const std::string ABC{"abc"};
const QString ABC_SHA256 = QStringLiteral("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

std::vector<unsigned char> BytesOf(const std::string& text)
{
    return std::vector<unsigned char>(text.begin(), text.end());
}

uint256 Sha256Of(const std::vector<unsigned char>& data)
{
    uint256 out;
    CSHA256().Write(data.data(), data.size()).Finalize(out.begin());
    return out;
}

std::string ReadAll(const fs::path& path)
{
    std::ifstream in(path.string(), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteAll(const fs::path& path, const std::string& text)
{
    std::ofstream out(path.string(), std::ios::binary);
    out << text;
}

int EntriesIn(const QTemporaryDir& dir)
{
    return static_cast<int>(
        QDir(dir.path()).entryList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System).size());
}

fs::path PathIn(const QTemporaryDir& dir, const char* name)
{
    return GUIUtil::qstringToBoostPath(dir.path()) / name;
}

//! The thread \p worker runs its jobs on, asked first so that the thread is running before the
//! work under test: an id is unique only among threads alive at the same time.
std::thread::id ThreadOf(GuiIpcWorker& worker)
{
    std::thread::id id;
    const std::future<void> done = worker.Post([&id] { id = std::this_thread::get_id(); });
    if (!done.valid() || done.wait_for(10s) != std::future_status::ready) return {};
    return id;
}

//! Enter \p line at the console's input, as a user does. It returns once the console has handled
//! it, a file command's transfer included.
void Enter(RPCConsole& console, const QString& line)
{
    auto* input = console.findChild<QLineEdit*>(QStringLiteral("lineEdit"));
    QVERIFY(input);
    input->setText(line);
    QTest::keyClick(input, Qt::Key_Return);
}

QString PaneText(RPCConsole& console)
{
    auto* pane = console.findChild<QTextEdit*>(QStringLiteral("messagesWidget"));
    return pane ? pane->toPlainText() : QString();
}

//! A console line naming \p path, quoted.
QString Quoted(const fs::path& path)
{
    return QStringLiteral("\"") + GUIUtil::boostPathToQString(path) + QStringLiteral("\"");
}

//! What a test source or sink saw. Written on the thread its calls run on, and read by the test
//! once the command has returned.
struct TransferLog
{
    std::vector<std::thread::id> threads{};
    int reads{0};
    bool cancelled{false};
    bool destroyed{false};
    std::thread::id destroyed_on{};
    //! The wallet's endElevation() count when the source or sink was destroyed.
    int end_elevation_calls_at_destruction{-1};
    // Sinks only.
    int finishes{0};
    std::vector<unsigned char> received{};
    uint64_t finish_size{0};
    uint256 finish_sha{};
};

//! A console source with one file: its name, the size and note it is listed with, and its
//! bytes, in chunks of at most 2 bytes. With \p first_read_error, the first read returns that
//! error, as a dump that could not start does. With a gate, the first read waits until the gate
//! opens (10 seconds at most).
class ConsoleSource : public interfaces::FileSource
{
public:
    ConsoleSource(std::shared_ptr<TransferLog> log, const int* end_elevation_calls, std::string name,
                  std::vector<unsigned char> data, int64_t listed_size, std::string note,
                  std::string first_read_error = {}, const std::atomic<bool>* gate = nullptr,
                  std::atomic<bool>* waiting = nullptr)
        : m_log(std::move(log)), m_end_elevation_calls(end_elevation_calls), m_name(std::move(name)),
          m_data(std::move(data)), m_listed_size(listed_size), m_note(std::move(note)),
          m_first_read_error(std::move(first_read_error)), m_gate(gate), m_waiting(waiting)
    {
    }

    ~ConsoleSource() override
    {
        m_log->destroyed = true;
        m_log->destroyed_on = std::this_thread::get_id();
        m_log->end_elevation_calls_at_destruction = *m_end_elevation_calls;
    }

    std::vector<interfaces::FileEntryInfo> entries() override
    {
        m_log->threads.push_back(std::this_thread::get_id());
        interfaces::FileEntryInfo info;
        info.name = m_name;
        info.size = m_listed_size;
        info.size_hint = m_data.size();
        info.note = m_note;
        return {info};
    }

    interfaces::FileChunk read(uint32_t /*entry*/, uint32_t max_bytes) override
    {
        m_log->threads.push_back(std::this_thread::get_id());
        const int n = ++m_log->reads;

        if (n == 1 && m_gate) {
            if (m_waiting) *m_waiting = true;
            for (int i = 0; i < 1000 && !m_gate->load(); ++i) {
                std::this_thread::sleep_for(10ms);
            }
        }

        interfaces::FileChunk chunk;

        if (n == 1 && !m_first_read_error.empty()) {
            chunk.error = m_first_read_error;
            return chunk;
        }
        if (m_log->cancelled) {
            chunk.error = "the transfer was cancelled";
            return chunk;
        }

        const size_t len = std::min<size_t>({size_t{max_bytes}, size_t{2}, m_data.size() - m_pos});
        chunk.data.assign(m_data.begin() + m_pos, m_data.begin() + m_pos + len);
        m_pos += len;
        chunk.eof = m_pos == m_data.size();
        return chunk;
    }

    interfaces::FileTrailer trailer(uint32_t /*entry*/) override
    {
        m_log->threads.push_back(std::this_thread::get_id());
        interfaces::FileTrailer trailer;
        trailer.ok = true;
        trailer.size = m_data.size();
        trailer.sha256 = Sha256Of(m_data);
        return trailer;
    }

    void cancel() override { m_log->cancelled = true; }

private:
    const std::shared_ptr<TransferLog> m_log;
    const int* const m_end_elevation_calls;
    const std::string m_name;
    const std::vector<unsigned char> m_data;
    const int64_t m_listed_size;
    const std::string m_note;
    const std::string m_first_read_error;
    const std::atomic<bool>* const m_gate;
    std::atomic<bool>* const m_waiting;
    size_t m_pos{0};
};

//! Where a test sink waits for its gate.
enum class SinkGate { None, Write, Finish };

//! A console sink that keeps what it receives and answers finish() with \p output. With a gate,
//! its first write or its finish waits until the gate opens (10 seconds at most).
class ConsoleSink : public interfaces::FileSink
{
public:
    ConsoleSink(std::shared_ptr<TransferLog> log, const int* end_elevation_calls, std::string output,
                SinkGate gate_on = SinkGate::None, const std::atomic<bool>* gate = nullptr,
                std::atomic<bool>* waiting = nullptr)
        : m_log(std::move(log)), m_end_elevation_calls(end_elevation_calls), m_output(std::move(output)),
          m_gate_on(gate_on), m_gate(gate), m_waiting(waiting)
    {
    }

    ~ConsoleSink() override
    {
        m_log->destroyed = true;
        m_log->destroyed_on = std::this_thread::get_id();
        m_log->end_elevation_calls_at_destruction = *m_end_elevation_calls;
    }

    bool write(const std::vector<unsigned char>& data) override
    {
        m_log->threads.push_back(std::this_thread::get_id());
        if (m_gate_on == SinkGate::Write && m_log->received.empty()) Wait();
        m_log->received.insert(m_log->received.end(), data.begin(), data.end());
        return true;
    }

    interfaces::FileSinkResult finish(uint64_t size, const uint256& sha256) override
    {
        m_log->threads.push_back(std::this_thread::get_id());
        ++m_log->finishes;
        if (m_gate_on == SinkGate::Finish) Wait();
        m_log->finish_size = size;
        m_log->finish_sha = sha256;
        interfaces::FileSinkResult result;
        result.ok = true;
        result.output = m_output;
        return result;
    }

    void cancel() override { m_log->cancelled = true; }

private:
    void Wait()
    {
        if (!m_gate) return;
        if (m_waiting) *m_waiting = true;
        for (int i = 0; i < 1000 && !m_gate->load(); ++i) {
            std::this_thread::sleep_for(10ms);
        }
    }

    const std::shared_ptr<TransferLog> m_log;
    const int* const m_end_elevation_calls;
    const std::string m_output;
    const SinkGate m_gate_on;
    const std::atomic<bool>* const m_gate;
    std::atomic<bool>* const m_waiting;
};

//! The progress dialog, if one is showing.
QProgressDialog* VisibleProgressDialog()
{
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        auto* dialog = qobject_cast<QProgressDialog*>(widget);
        if (dialog && dialog->isVisible()) return dialog;
    }
    return nullptr;
}

//! Clicks the progress dialog's Cancel once \p waiting is set, then opens \p gate once the dialog
//! says it is cancelling. Started before the command is entered; \p clicked and \p saw_cancelling
//! record what it did.
void CancelWhenWaiting(QTimer& observer, QObject* context, std::atomic<bool>& waiting, std::atomic<bool>& gate,
                       bool& clicked, bool& saw_cancelling)
{
    QObject::connect(&observer, &QTimer::timeout, context,
                     [w = &waiting, g = &gate, c = &clicked, seen = &saw_cancelling] {
                         QProgressDialog* dialog = VisibleProgressDialog();
                         if (!dialog || !*w) return;

                         auto* button = dialog->findChild<QPushButton*>();

                         if (!*c && button) {
                             button->click();
                             *c = true;
                             return;
                         }

                         if (*c && dialog->labelText() == QStringLiteral("Cancelling...")) {
                             *seen = true;
                             *g = true;
                         }
                     });
    observer.start(5);
}

//! A WalletModel over FakeLockStateWallet, unencrypted unless the test says otherwise. Its
//! passphrase prompt is answered at once: the kind asked for is recorded, a capability is begun
//! for it, and the model is told so, as the prompt does once the passphrase checks out.
struct ConsoleWallet
{
    qt_test::FakeLockStateWallet wallet;
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel options{node, sidestakes};
    qt_test::FakeWalletTxSource tx_source;
    qt_test::FakeCoinSource coin_source;
    std::unique_ptr<WalletModel> model{std::make_unique<WalletModel>(wallet, tx_source, coin_source, &options)};
    std::vector<CapabilityKind> prompts{};

    ConsoleWallet()
    {
        QObject::connect(model.get(), &WalletModel::requireUnlock, model.get(), [this](CapabilityKind kind) {
            prompts.push_back(kind);
            wallet.beginElevation(SecureString(), kind);
            model->setElevationBegun();
        });
    }

    void UnlockedForStakingOnly()
    {
        wallet.m_lock_state.crypted = true;
        wallet.m_lock_state.locked = false;
        wallet.m_lock_state.unlocked_for_staking_only = true;
    }
};

//! The core's own files, with \p datadir as its data directory.
GuiIpcInfo CoreAt(const fs::path& datadir)
{
    GuiIpcInfo info;
    info.active = true;
    info.core_paths.datadir = datadir.string();
    info.core_paths.wallet_file = (datadir / "wallet.dat").string();
    info.core_paths.config_file = (datadir / "gridcoinresearch.conf").string();
    info.core_paths.settings_file = (datadir / "gridcoinsettings.json").string();
    info.core_paths.debug_log = (datadir / "debug.log").string();
    return info;
}
} // namespace

void RPCConsoleTests::initTestCase()
{
    // The options model reads QSettings. Redirected as WalletModelTests does, so a run cannot
    // reach or leave anything in a real store.
    m_prev_organization = QCoreApplication::organizationName();
    m_prev_application = QCoreApplication::applicationName();
    m_prev_format = QSettings::defaultFormat();

    m_settings_root = std::make_unique<QTemporaryDir>();
    QVERIFY(m_settings_root->isValid());

    QCoreApplication::setOrganizationName("GridcoinTest");
    QCoreApplication::setApplicationName("rpcconsoletests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings_root->path());

    // The Qt test main selects no chain, and the data directory (GetDataDir()) is the network's.
    SelectParams(CBaseChainParams::MAIN);
}

void RPCConsoleTests::cleanupTestCase()
{
    QSettings::setDefaultFormat(m_prev_format);
    QCoreApplication::setOrganizationName(m_prev_organization);
    QCoreApplication::setApplicationName(m_prev_application);
    m_settings_root.reset();
}

void RPCConsoleTests::init()
{
    // A temporary data directory, so that a bare file name, and anything the options model
    // reaches, never names the default one on the host.
    m_data_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_data_dir->isValid());
    gArgs.ForceSetArg("-datadir", m_data_dir->path().toStdString());
    gArgs.ClearPathCache();
}

void RPCConsoleTests::cleanup()
{
    QSettings().clear();
    gArgs.ForceSetArg("-datadir", "");
    gArgs.ClearPathCache();
    m_data_dir.reset();
}

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

// The helpers take any QString, so the line's bytes must not go through the secure pool, which
// refuses a buffer over 256 KiB. The console's own input stops far short of that; the helpers are
// called directly here.
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

// A passphrase the secure pool cannot hold is refused before anything is built or sent, and its
// parsed copy is still overwritten. One byte less than an arena is the longest that fits.
void RPCConsoleTests::refusesPassphraseTooLongForSecurePool()
{
    qt_test::FakeNode node;
    const size_t too_long = 300 * 1024;
    std::vector<std::string> args{"walletpassphrase", std::string(too_long, 'p'), "60"};

    const interfaces::RpcConsoleResult result = DispatchConsoleCommand(node, args);

    QVERIFY(!result.ok);
    QCOMPARE(result.output, std::string("Error: walletpassphrase: passphrase too long"));
    QVERIFY(node.m_secret_calls.empty());
    QVERIFY(node.m_console_calls.empty());
    QCOMPARE(args[1], std::string(too_long, '\0'));

    std::vector<std::string> at_limit{"walletpassphrasechange", "old", std::string(LockedPool::ARENA_SIZE - 1, 'n')};
    QVERIFY(DispatchConsoleCommand(node, at_limit).output.empty());
    QCOMPARE(node.m_secret_calls.size(), size_t{1});
    QCOMPARE(node.m_secret_calls.front().passphrases.at(1).size(), size_t{LockedPool::ARENA_SIZE - 1});

    std::vector<std::string> past_limit{"walletpassphrasechange", "old", std::string(LockedPool::ARENA_SIZE, 'n')};
    QCOMPARE(DispatchConsoleCommand(node, past_limit).output,
             std::string("Error: walletpassphrasechange: passphrase too long"));
    QCOMPARE(node.m_secret_calls.size(), size_t{1});
}

// Every method the console routes as a passphrase command must also be redacted, or its passphrase
// would go to the secret path and still be echoed into the pane, the history and the log. The
// methods come from the node's own RPC table.
void RPCConsoleTests::everyPassphraseMethodIsRedacted()
{
    size_t passphrase_methods = 0;

    for (const std::string& method : interfaces::MakeNode()->listRpcCommands()) {
        if (interfaces::RpcConsolePassphraseIndexes(method) == nullptr) continue;
        ++passphrase_methods;

        const QString name = QString::fromStdString(method);
        QCOMPARE(RedactSensitiveArgs(name + QStringLiteral(" ") + PHRASE_PASSWORD + QStringLiteral(" x")),
                 name + QStringLiteral(" (arguments not shown)"));
    }

    // walletpassphrase, walletpassphrasechange and encryptwallet at least.
    QVERIFY(passphrase_methods >= 3);
}

// A mis-cased passphrase command never runs, but its arguments are still kept out of the history,
// the pane and the "Handling Error" log line, which all use RedactSensitiveArgs.
void RPCConsoleTests::redactsMisCasedPassphraseCommand()
{
    QCOMPARE(RedactSensitiveArgs(QStringLiteral("WalletPassphrase ") + PHRASE_PASSWORD + QStringLiteral(" 60")),
             QStringLiteral("WalletPassphrase (arguments not shown)"));

    // Unparseable, so through the leading-token fallback.
    QCOMPARE(RedactSensitiveArgs(QStringLiteral("WALLETPASSPHRASECHANGE old \"") + PHRASE_PASSWORD),
             QStringLiteral("WALLETPASSPHRASECHANGE (arguments not shown)"));

    RPCConsole console;
    auto* input = console.findChild<QLineEdit*>(QStringLiteral("lineEdit"));
    auto* pane = console.findChild<QTextEdit*>(QStringLiteral("messagesWidget"));
    QVERIFY(input);
    QVERIFY(pane);

    input->setText(QStringLiteral("EncryptWallet ") + PHRASE_PASSWORD);
    QTest::keyClick(input, Qt::Key_Return);
    QTest::keyClick(input, Qt::Key_Up);

    QCOMPARE(input->text(), QStringLiteral("EncryptWallet (arguments not shown)"));
    QVERIFY(!pane->toPlainText().contains(PHRASE_PASSWORD));
}

// A bare file name is taken in this process's data directory, as the RPC takes it in the core's.
// A quoted path keeps its spaces and its non-ASCII characters.
void RPCConsoleTests::fileCommandBareNameResolvesToDataDir()
{
    std::string error;

    const std::optional<RPCConsoleDetail::ConsoleFileRequest> bare =
        RPCConsoleDetail::ParseConsoleFileRequest("dumpwallet out.txt", error);
    QVERIFY2(bare, error.c_str());
    QVERIFY(error.empty());
    QCOMPARE(std::string(bare->cmd->method), std::string("dumpwallet"));
    QCOMPARE(bare->local_path, GetDataDir() / "out.txt");
    QVERIFY(bare->core_args.empty());

    const std::optional<RPCConsoleDetail::ConsoleFileRequest> quoted =
        RPCConsoleDetail::ParseConsoleFileRequest("dumpwallet \"/tmp/a b/\xc3\xbc.txt\"", error);
    QVERIFY2(quoted, error.c_str());
    QCOMPARE(quoted->local_path, fs::path("/tmp/a b/\xc3\xbc.txt"));
    QVERIFY(quoted->core_args.empty());
}

// dumpcontracts names its file second; the core is sent the other arguments, in order.
void RPCConsoleTests::dumpcontractsPathIsSecondArgument()
{
    std::string error;

    const std::optional<RPCConsoleDetail::ConsoleFileRequest> request =
        RPCConsoleDetail::ParseConsoleFileRequest("dumpcontracts beacon /tmp/b.txt true", error);
    QVERIFY2(request, error.c_str());
    QCOMPARE(std::string(request->cmd->method), std::string("dumpcontracts"));
    QCOMPARE(request->local_path, fs::path("/tmp/b.txt"));
    QCOMPARE(request->core_args, (std::vector<std::string>{"beacon", "true"}));
}

void RPCConsoleTests::nonFileCommandIsNotIntercepted()
{
    std::string error;

    QVERIFY(!RPCConsoleDetail::ParseConsoleFileRequest("getblockcount", error));
    QVERIFY(error.empty());

    // Help on a file command is not one, and neither is a line that does not parse.
    QVERIFY(!RPCConsoleDetail::ParseConsoleFileRequest("help dumpwallet", error));
    QVERIFY(error.empty());
    QVERIFY(!RPCConsoleDetail::ParseConsoleFileRequest("dumpwallet \"/tmp/x", error));
    QVERIFY(error.empty());

    // At the console, an ordinary command still goes to the executor.
    RPCConsole console;
    QSignalSpy requests(&console, &RPCConsole::cmdRequest);
    Enter(console, QStringLiteral("getblockcount"));
    QCOMPARE(requests.count(), 1);
}

void RPCConsoleTests::fileCommandWithoutPathIsAnError()
{
    std::string error;

    QVERIFY(!RPCConsoleDetail::ParseConsoleFileRequest("importwallet", error));
    QVERIFY2(error.find("usage") != std::string::npos, error.c_str());

    QVERIFY(!RPCConsoleDetail::ParseConsoleFileRequest("dumpcontracts beacon", error));
    QVERIFY2(error.find("usage") != std::string::npos, error.c_str());

    // At the console, the usage is shown and nothing goes to the executor.
    RPCConsole console;
    QSignalSpy requests(&console, &RPCConsole::cmdRequest);
    Enter(console, QStringLiteral("importwallet"));
    QCOMPARE(requests.count(), 0);
    QVERIFY2(PaneText(console).contains(QStringLiteral("usage")), qPrintable(PaneText(console)));
}

// More arguments than the command's RPC takes is a usage error, as the RPC's own count check
// gives: nothing is asked for and nothing reaches the core.
void RPCConsoleTests::fileCommandWithTooManyArgumentsIsAnError()
{
    std::string error;

    QVERIFY(!RPCConsoleDetail::ParseConsoleFileRequest("dumpwallet out.txt extra", error));
    QVERIFY2(error.find("usage") != std::string::npos && error.find("at most 1 argument") != std::string::npos,
             error.c_str());

    QVERIFY(!RPCConsoleDetail::ParseConsoleFileRequest("dumpcontracts beacon /tmp/b.txt true 0 100 7", error));
    QVERIFY2(error.find("at most 5 argument") != std::string::npos, error.c_str());

    // All five dumpcontracts takes are accepted.
    QVERIFY(RPCConsoleDetail::ParseConsoleFileRequest("dumpcontracts beacon /tmp/b.txt true 0 100", error));
    QVERIFY(error.empty());

    GuiIpcWorker worker(GuiIpcWorker::Role::Modal);
    ConsoleWallet fixture;
    fixture.UnlockedForStakingOnly();
    fixture.wallet.m_console_download = [](const std::string&, const std::vector<std::string>&, std::string&) {
        return std::unique_ptr<interfaces::FileSource>();
    };

    RPCConsole console;
    console.setWalletModel(fixture.model.get());
    QSignalSpy requests(&console, &RPCConsole::cmdRequest);
    Enter(console, QStringLiteral("dumpwallet out.txt extra"));

    QCOMPARE(requests.count(), 0);
    QVERIFY(fixture.prompts.empty());
    QVERIFY(fixture.wallet.m_console_begins.empty());
    QVERIFY2(PaneText(console).contains(QStringLiteral("at most 1 argument")), qPrintable(PaneText(console)));
    QVERIFY(!fs::exists(GetDataDir() / "out.txt"));
}

// Without a wallet a file command is refused at the console, and never reaches the node's console,
// which refuses it too.
void RPCConsoleTests::fileCommandNeedsALoadedWallet()
{
    RPCConsole console;
    QSignalSpy requests(&console, &RPCConsole::cmdRequest);

    Enter(console, QStringLiteral("dumpwallet out.txt"));

    QCOMPARE(requests.count(), 0);
    QVERIFY2(PaneText(console).contains(QStringLiteral("File commands need a loaded wallet.")),
             qPrintable(PaneText(console)));
    QVERIFY(!fs::exists(GetDataDir() / "out.txt"));
}

// The command begins, lists, downloads and ends its transfer on the modal worker, never on the
// GUI thread and never through the node's console. The pane says what was written and shows the
// command's reply, which names the local file, not the core's.
void RPCConsoleTests::downloadRunsOnTheWorkerAndShowsWhatItWrote()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::Modal);
    const std::thread::id worker_thread = ThreadOf(worker);
    QVERIFY(worker_thread != std::thread::id{});
    QVERIFY(worker_thread != std::this_thread::get_id());

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const fs::path dest = PathIn(dir, "contracts.txt");

    ConsoleWallet fixture;
    const auto log = std::make_shared<TransferLog>();
    const std::string reply = "{\n  \"contract_type\": \"beacon\",\n  \"number_of_contracts_exported\": 7,\n"
                              "  \"exported_to_file\": \"/core/data/transfers/out-0123456789abcdef.tmp\"\n}";
    fixture.wallet.m_console_download = [&](const std::string&, const std::vector<std::string>&, std::string&) {
        return std::make_unique<ConsoleSource>(log, &fixture.wallet.m_end_elevation_calls, "dumpcontracts.txt",
                                               BytesOf(ABC), 3, reply);
    };

    RPCConsole console;
    console.setWalletModel(fixture.model.get());
    QSignalSpy requests(&console, &RPCConsole::cmdRequest);

    const QString line = QStringLiteral("dumpcontracts beacon ") + Quoted(dest) + QStringLiteral(" true");
    Enter(console, line);

    QCOMPARE(requests.count(), 0);
    QCOMPARE(fixture.wallet.m_console_begins.size(), size_t{1});
    const qt_test::FakeLockStateWallet::ConsoleBegin& begin = fixture.wallet.m_console_begins.front();
    QCOMPARE(begin.method, std::string("dumpcontracts"));
    QCOMPARE(begin.args, (std::vector<std::string>{"beacon", "true"}));
    QVERIFY(begin.thread == worker_thread);

    QVERIFY(!log->threads.empty());
    QVERIFY(std::all_of(log->threads.begin(), log->threads.end(),
                        [&](const std::thread::id& id) { return id == worker_thread; }));
    QVERIFY(log->destroyed);
    QVERIFY(log->destroyed_on == worker_thread);

    QCOMPARE(ReadAll(dest), ABC);
    QCOMPARE(EntriesIn(dir), 1);

    // An unencrypted wallet needs no capability, and asks nothing.
    QVERIFY(fixture.prompts.empty());

    const QString pane = PaneText(console);
    const QString shown_dest = GUIUtil::boostPathToQString(dest);
    QVERIFY2(pane.contains(QStringLiteral("Wrote 3 bytes to %1 (SHA-256 %2)").arg(shown_dest, ABC_SHA256)),
             qPrintable(pane));
    QVERIFY2(pane.contains(QStringLiteral("\"number_of_contracts_exported\": 7")), qPrintable(pane));
    QVERIFY2(pane.contains(QStringLiteral("\"exported_to_file\": \"%1\"").arg(shown_dest)), qPrintable(pane));
    QVERIFY2(!pane.contains(QStringLiteral("/core/data/transfers")), qPrintable(pane));

    // The history keeps the line, as it does every command (RedactSensitiveArgs leaves it as it is).
    auto* input = console.findChild<QLineEdit*>(QStringLiteral("lineEdit"));
    QVERIFY(input);
    QVERIFY(input->isEnabled());
    QTest::keyClick(input, Qt::Key_Up);
    QCOMPARE(input->text(), line);
}

// On a wallet unlocked for staking only, each file command asks for the passphrase once and begins
// a FileTransfer capability for itself alone. The capability lives until the source has been
// destroyed, and ends then. A wallet that needs none asks nothing.
void RPCConsoleTests::downloadHoldsItsOwnCapabilityUntilTheSourceIsGone()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::Modal);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    ConsoleWallet fixture;
    fixture.UnlockedForStakingOnly();
    std::vector<std::shared_ptr<TransferLog>> logs;
    fixture.wallet.m_console_download = [&](const std::string&, const std::vector<std::string>&, std::string&) {
        logs.push_back(std::make_shared<TransferLog>());
        return std::make_unique<ConsoleSource>(logs.back(), &fixture.wallet.m_end_elevation_calls, "wallet-dump.txt",
                                               BytesOf(ABC), -1, "");
    };

    RPCConsole console;
    console.setWalletModel(fixture.model.get());

    Enter(console, QStringLiteral("dumpwallet ") + Quoted(PathIn(dir, "one.txt")));

    QCOMPARE(fixture.prompts, (std::vector<CapabilityKind>{CapabilityKind::FileTransfer}));
    QCOMPARE(fixture.wallet.m_console_begins.size(), size_t{1});
    QCOMPARE(fixture.wallet.m_console_begins[0].begin_elevation_calls, 1);
    QCOMPARE(fixture.wallet.m_console_begins[0].end_elevation_calls, 0);
    QCOMPARE(logs.size(), size_t{1});
    QVERIFY(logs[0]->destroyed);
    QCOMPARE(logs[0]->end_elevation_calls_at_destruction, 0);
    QCOMPARE(fixture.wallet.m_end_elevation_calls, 1);
    QCOMPARE(ReadAll(PathIn(dir, "one.txt")), ABC);

    // The next command asks again, for a capability of its own.
    Enter(console, QStringLiteral("dumpwallet ") + Quoted(PathIn(dir, "two.txt")));

    QCOMPARE(fixture.prompts.size(), size_t{2});
    QCOMPARE(fixture.wallet.m_begin_elevation_calls, 2);
    QCOMPARE(fixture.wallet.m_console_begins[1].end_elevation_calls, 1);
    QCOMPARE(logs[1]->end_elevation_calls_at_destruction, 1);
    QCOMPARE(fixture.wallet.m_end_elevation_calls, 2);
    QCOMPARE(ReadAll(PathIn(dir, "two.txt")), ABC);

    // A fully unlocked wallet needs none.
    fixture.wallet.m_lock_state.unlocked_for_staking_only = false;
    Enter(console, QStringLiteral("dumpwallet ") + Quoted(PathIn(dir, "three.txt")));

    QCOMPARE(fixture.prompts.size(), size_t{2});
    QCOMPARE(fixture.wallet.m_begin_elevation_calls, 2);
    QCOMPARE(fixture.wallet.m_end_elevation_calls, 2);
    QCOMPARE(ReadAll(PathIn(dir, "three.txt")), ABC);
}

void RPCConsoleTests::downloadNoteNamesTheLocalFile()
{
    const fs::path dest("/home/user/contracts.txt");

    // The core's file is named by the local one; the other fields keep their order.
    const std::string reply = "{\"timestamp\":5,\"exported_to_file\":\"/core/transfers/out-1.tmp\",\"low_height\":0}";
    QCOMPARE(RPCConsoleDetail::ConsoleNoteForDestination(reply, dest),
             std::string("{\n  \"timestamp\": 5,\n  \"exported_to_file\": \"/home/user/contracts.txt\",\n"
                         "  \"low_height\": 0\n}"));

    // Any other note is shown as it is.
    QCOMPARE(RPCConsoleDetail::ConsoleNoteForDestination("plain text", dest), std::string("plain text"));
    QCOMPARE(RPCConsoleDetail::ConsoleNoteForDestination("{\"a\":1}", dest), std::string("{\"a\":1}"));
    QCOMPARE(RPCConsoleDetail::ConsoleNoteForDestination("", dest), std::string());
}

// A file the core lists with nothing in it and a note, and refuses to read with that note (an
// entry with no file), is read, and its note is shown. Nothing is written.
void RPCConsoleTests::downloadWithNothingToSendIsReportedNotWritten()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::Modal);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const fs::path dest = PathIn(dir, "out.txt");

    ConsoleWallet fixture;
    const auto log = std::make_shared<TransferLog>();
    const std::string note = "the command wrote nothing to send";
    fixture.wallet.m_console_download = [&](const std::string&, const std::vector<std::string>&, std::string&) {
        return std::make_unique<ConsoleSource>(log, &fixture.wallet.m_end_elevation_calls, "dumpcontracts.txt",
                                               std::vector<unsigned char>{}, 0, note, note);
    };

    RPCConsole console;
    console.setWalletModel(fixture.model.get());
    Enter(console, QStringLiteral("dumpcontracts beacon ") + Quoted(dest));

    QCOMPARE(log->reads, 1);
    QVERIFY(log->destroyed);
    QCOMPARE(EntriesIn(dir), 0);
    QVERIFY2(PaneText(console).contains(QStringLiteral("Nothing was written to %1: %2")
                                            .arg(GUIUtil::boostPathToQString(dest), QString::fromStdString(note))),
             qPrintable(PaneText(console)));
    QVERIFY(!PaneText(console).contains(QStringLiteral("Wrote ")));
}

// A file with nothing in it that the core does send is written, empty, and its reply is shown with
// the local file named, as for any other.
void RPCConsoleTests::downloadOfAnEmptyFileWritesItAndShowsTheReply()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::Modal);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const fs::path dest = PathIn(dir, "empty.txt");

    ConsoleWallet fixture;
    const auto log = std::make_shared<TransferLog>();
    const std::string reply = "{\"number_of_contracts_exported\": 0, "
                              "\"exported_to_file\": \"/core/data/transfers/out-0123456789abcdef.tmp\"}";
    fixture.wallet.m_console_download = [&](const std::string&, const std::vector<std::string>&, std::string&) {
        return std::make_unique<ConsoleSource>(log, &fixture.wallet.m_end_elevation_calls, "dumpcontracts.txt",
                                               std::vector<unsigned char>{}, 0, reply);
    };

    RPCConsole console;
    console.setWalletModel(fixture.model.get());
    Enter(console, QStringLiteral("dumpcontracts beacon ") + Quoted(dest));

    QVERIFY(fs::exists(dest));
    QCOMPARE(ReadAll(dest), std::string());
    QCOMPARE(EntriesIn(dir), 1);

    const QString pane = PaneText(console);
    const QString shown_dest = GUIUtil::boostPathToQString(dest);
    QVERIFY2(pane.contains(QStringLiteral("Wrote 0 bytes to %1 (SHA-256 "
                                          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)")
                               .arg(shown_dest)),
             qPrintable(pane));
    QVERIFY2(pane.contains(QStringLiteral("\"exported_to_file\": \"%1\"").arg(shown_dest)), qPrintable(pane));
    QVERIFY2(!pane.contains(QStringLiteral("/core/data/transfers")), qPrintable(pane));
}

// A source that fails on its first read (the dump could not start) shows its error in the pane,
// and nothing is left at the destination.
void RPCConsoleTests::downloadFirstReadErrorIsShown()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::Modal);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const fs::path dest = PathIn(dir, "dump.txt");

    ConsoleWallet fixture;
    const auto log = std::make_shared<TransferLog>();
    fixture.wallet.m_console_download = [&](const std::string&, const std::vector<std::string>&, std::string&) {
        return std::make_unique<ConsoleSource>(log, &fixture.wallet.m_end_elevation_calls, "wallet-dump.txt",
                                               BytesOf(ABC), -1, "", "dumpwallet: cannot read the key for SAddr");
    };

    RPCConsole console;
    console.setWalletModel(fixture.model.get());
    Enter(console, QStringLiteral("dumpwallet ") + Quoted(dest));

    QCOMPARE(log->reads, 1);
    QVERIFY(!fs::exists(dest));
    QCOMPARE(EntriesIn(dir), 0);
    QVERIFY2(PaneText(console).contains(QStringLiteral("dumpwallet: cannot read the key for SAddr")),
             qPrintable(PaneText(console)));
}

// A destination that is one of the core's own files is refused before the prompt and before the
// core is asked anything. So is a bare name that lands on one, when this process's data directory
// is the core's (same account).
void RPCConsoleTests::downloadToACoreFileIsRefusedBeforeThePrompt()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::Modal);

    ConsoleWallet fixture;
    fixture.UnlockedForStakingOnly();
    fixture.wallet.m_console_download = [](const std::string&, const std::vector<std::string>&, std::string&) {
        return std::unique_ptr<interfaces::FileSource>();
    };

    RPCConsole console;
    console.setIpcInfo(CoreAt(GetDataDir()));
    console.setWalletModel(fixture.model.get());

    Enter(console, QStringLiteral("dumpwallet ") + Quoted(GetDataDir() / "wallet.dat"));
    Enter(console, QStringLiteral("dumpwallet wallet.dat"));
    Enter(console, QStringLiteral("dumpcontracts beacon ") + Quoted(GetDataDir() / "database" / "log.0000000001"));

    QVERIFY(fixture.prompts.empty());
    QVERIFY(fixture.wallet.m_console_begins.empty());
    const QString pane = PaneText(console);
    QCOMPARE(pane.count(QStringLiteral("is one of the core's own files")), 2);
    QVERIFY2(pane.contains(QStringLiteral("is inside the core's")), qPrintable(pane));
}

// A download never replaces what is already at its destination: a file, a bare name that names
// one in this process's data directory, or a link, also one that leads nowhere. It is refused
// before the prompt and before the core is asked anything, and what is there is left as it is.
void RPCConsoleTests::downloadKeepsAnExistingFile()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::Modal);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const fs::path old_contracts = PathIn(dir, "contracts.txt");
    const fs::path old_dump = PathIn(dir, "dump.txt");
    const fs::path peers = GetDataDir() / "peers.dat";
    WriteAll(old_contracts, "old contracts");
    WriteAll(old_dump, "old dump");
    WriteAll(peers, "peers");

    ConsoleWallet fixture;
    fixture.UnlockedForStakingOnly();
    const auto log = std::make_shared<TransferLog>();
    fixture.wallet.m_console_download = [&](const std::string&, const std::vector<std::string>&, std::string&) {
        return std::make_unique<ConsoleSource>(log, &fixture.wallet.m_end_elevation_calls, "dumpcontracts.txt",
                                               BytesOf(ABC), 3, "{}");
    };

    RPCConsole console;
    console.setWalletModel(fixture.model.get());
    Enter(console, QStringLiteral("dumpcontracts beacon ") + Quoted(old_contracts));
    Enter(console, QStringLiteral("dumpwallet ") + Quoted(old_dump));
    Enter(console, QStringLiteral("dumpwallet peers.dat"));
    int refusals = 3;

#ifndef WIN32
    const fs::path dangling = PathIn(dir, "dangling.txt");
    fs::create_symlink(PathIn(dir, "nowhere.txt"), dangling);
    Enter(console, QStringLiteral("dumpwallet ") + Quoted(dangling));
    ++refusals;
#endif

    QVERIFY(fixture.prompts.empty());
    QCOMPARE(fixture.wallet.m_begin_elevation_calls, 0);
    QVERIFY(fixture.wallet.m_console_begins.empty());
    QCOMPARE(ReadAll(old_contracts), std::string("old contracts"));
    QCOMPARE(ReadAll(old_dump), std::string("old dump"));
    QCOMPARE(ReadAll(peers), std::string("peers"));
#ifndef WIN32
    QVERIFY(fs::is_symlink(dangling));
    QVERIFY(!fs::exists(PathIn(dir, "nowhere.txt")));
#endif

    const QString pane = PaneText(console);
    QCOMPARE(pane.count(QStringLiteral("File already exists at location. Please delete or rename old file first.")),
             refusals);
}

// A destination that cannot be examined, here under a directory this account may not search, is
// refused before the prompt as one that exists is, and the pane says why it was refused rather than
// that a file is there.
void RPCConsoleTests::downloadToADestinationThatCannotBeCheckedSaysSo()
{
#ifdef WIN32
    QSKIP("A directory that cannot be searched is made with POSIX modes.");
#else
    if (::geteuid() == 0) QSKIP("Running as root, which may search any directory.");

    GuiIpcWorker worker(GuiIpcWorker::Role::Modal);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const fs::path closed = PathIn(dir, "closed");
    QVERIFY(fs::create_directory(closed));
    const fs::path dest = closed / "dump.txt";

    // Searchable again however the case ends, so the temporary directory can be removed.
    struct Reopen {
        fs::path path;
        ~Reopen() { ::chmod(path.c_str(), 0700); }
    } reopen{closed};
    QCOMPARE(::chmod(closed.c_str(), 0), 0);

    ConsoleWallet fixture;
    fixture.UnlockedForStakingOnly();
    fixture.wallet.m_console_download = [](const std::string&, const std::vector<std::string>&, std::string&) {
        return std::unique_ptr<interfaces::FileSource>();
    };

    RPCConsole console;
    console.setWalletModel(fixture.model.get());
    Enter(console, QStringLiteral("dumpwallet ") + Quoted(dest));

    QVERIFY(fixture.prompts.empty());
    QCOMPARE(fixture.wallet.m_begin_elevation_calls, 0);
    QVERIFY(fixture.wallet.m_console_begins.empty());

    const QString pane = PaneText(console);
    QVERIFY2(pane.contains(QStringLiteral("Cannot check the destination %1: ").arg(GUIUtil::boostPathToQString(dest))),
             qPrintable(pane));
    QVERIFY2(!pane.contains(QStringLiteral("File already exists")), qPrintable(pane));
#endif
}

// Cancel during a download stops it: nothing is at the destination, no partial file is left, and
// the pane says so.
void RPCConsoleTests::downloadCancelLeavesNothing()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::Modal);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const fs::path dest = PathIn(dir, "dump.txt");

    ConsoleWallet fixture;
    const auto log = std::make_shared<TransferLog>();
    std::atomic<bool> gate{false};
    std::atomic<bool> waiting{false};
    fixture.wallet.m_console_download = [&](const std::string&, const std::vector<std::string>&, std::string&) {
        return std::make_unique<ConsoleSource>(log, &fixture.wallet.m_end_elevation_calls, "wallet-dump.txt",
                                               BytesOf(ABC), -1, "", "", &gate, &waiting);
    };

    RPCConsole console;
    console.setWalletModel(fixture.model.get());

    bool clicked = false;
    bool saw_cancelling = false;
    QTimer observer;
    CancelWhenWaiting(observer, this, waiting, gate, clicked, saw_cancelling);

    // So a regression fails rather than hangs.
    QTimer backstop;
    backstop.setSingleShot(true);
    connect(&backstop, &QTimer::timeout, this, [&gate] { gate = true; });
    backstop.start(10000);

    Enter(console, QStringLiteral("dumpwallet ") + Quoted(dest));
    observer.stop();
    backstop.stop();

    QVERIFY(clicked);
    QVERIFY(saw_cancelling);
    QVERIFY(!fs::exists(dest));
    QCOMPARE(EntriesIn(dir), 0);
    QVERIFY(log->destroyed);
    QVERIFY2(PaneText(console).contains(
                 QStringLiteral("Cancelled; nothing was written to %1.").arg(GUIUtil::boostPathToQString(dest))),
             qPrintable(PaneText(console)));
}

// A begin that throws, as a call across IPC does when the core drops the connection, shows its
// error, and the input can be used again.
void RPCConsoleTests::beginThatThrowsIsShown()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::Modal);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const fs::path dest = PathIn(dir, "dump.txt");

    ConsoleWallet fixture;
    fixture.wallet.m_console_download = [](const std::string&, const std::vector<std::string>&,
                                           std::string&) -> std::unique_ptr<interfaces::FileSource> {
        throw std::runtime_error("IPC client method call interrupted by disconnect.");
    };

    RPCConsole console;
    console.setWalletModel(fixture.model.get());
    Enter(console, QStringLiteral("dumpwallet ") + Quoted(dest));

    QCOMPARE(fixture.wallet.m_console_begins.size(), size_t{1});
    QVERIFY(!fs::exists(dest));
    QVERIFY2(PaneText(console).contains(QStringLiteral("IPC client method call interrupted by disconnect.")),
             qPrintable(PaneText(console)));

    auto* input = console.findChild<QLineEdit*>(QStringLiteral("lineEdit"));
    QVERIFY(input);
    QVERIFY(input->isEnabled());
}

// The upload asks for its own capability, begins with the file's size and the other arguments,
// sends the file on the modal worker, and shows what the core's command made of it. The
// capability lives until the sink has been destroyed.
void RPCConsoleTests::uploadSendsTheFileAndShowsTheOutput()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::Modal);
    const std::thread::id worker_thread = ThreadOf(worker);
    QVERIFY(worker_thread != std::thread::id{});

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const fs::path local = PathIn(dir, "dump file.txt");
    const std::string content = "# Wallet dump\nkey line\n";
    WriteAll(local, content);

    ConsoleWallet fixture;
    fixture.UnlockedForStakingOnly();
    const auto log = std::make_shared<TransferLog>();
    fixture.wallet.m_console_upload = [&](const std::string&, const std::vector<std::string>&, uint64_t,
                                          std::string&) {
        return std::make_unique<ConsoleSink>(log, &fixture.wallet.m_end_elevation_calls, "imported 1 key");
    };

    RPCConsole console;
    console.setWalletModel(fixture.model.get());
    QSignalSpy requests(&console, &RPCConsole::cmdRequest);
    Enter(console, QStringLiteral("importwallet ") + Quoted(local));

    QCOMPARE(requests.count(), 0);
    QCOMPARE(fixture.prompts, (std::vector<CapabilityKind>{CapabilityKind::FileTransfer}));
    QCOMPARE(fixture.wallet.m_console_begins.size(), size_t{1});
    const qt_test::FakeLockStateWallet::ConsoleBegin& begin = fixture.wallet.m_console_begins.front();
    QCOMPARE(begin.method, std::string("importwallet"));
    QVERIFY(begin.args.empty());
    QCOMPARE(begin.size, uint64_t{content.size()});
    QVERIFY(begin.thread == worker_thread);

    QVERIFY(log->received == BytesOf(content));
    QCOMPARE(log->finish_size, uint64_t{content.size()});
    QVERIFY(log->finish_sha == Sha256Of(BytesOf(content)));
    QVERIFY(std::all_of(log->threads.begin(), log->threads.end(),
                        [&](const std::thread::id& id) { return id == worker_thread; }));
    QVERIFY(log->destroyed_on == worker_thread);
    QCOMPARE(log->end_elevation_calls_at_destruction, 0);
    QCOMPARE(fixture.wallet.m_end_elevation_calls, 1);

    const QString pane = PaneText(console);
    QVERIFY2(pane.contains(QStringLiteral("Sent %1 bytes from %2.")
                               .arg(content.size())
                               .arg(GUIUtil::boostPathToQString(local))),
             qPrintable(pane));
    QVERIFY2(pane.contains(QStringLiteral("imported 1 key")), qPrintable(pane));
}

// A file that is not there, a directory, a file over the command's upload cap, and one this account
// may not read are refused before the prompt and before the core is asked anything.
void RPCConsoleTests::uploadOfNoRegularFileIsRefusedBeforeThePrompt()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::Modal);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Sparse: nothing is written but its size.
    const fs::path big = PathIn(dir, "big.txt");
    WriteAll(big, "");
    fs::resize_file(big, interfaces::FindConsoleFileCommand("importwallet")->max_upload_bytes + 1);

    ConsoleWallet fixture;
    fixture.UnlockedForStakingOnly();
    fixture.wallet.m_console_upload = [](const std::string&, const std::vector<std::string>&, uint64_t,
                                         std::string&) { return std::unique_ptr<interfaces::FileSink>(); };

    RPCConsole console;
    console.setWalletModel(fixture.model.get());

    Enter(console, QStringLiteral("importwallet ") + Quoted(PathIn(dir, "missing.txt")));
    Enter(console, QStringLiteral("parseaccrualsnapshotfile ") + Quoted(GUIUtil::qstringToBoostPath(dir.path())));
    Enter(console, QStringLiteral("importwallet ") + Quoted(big));

#ifndef WIN32
    // A file this account may not read. Root reads it anyway, so the case needs another account.
    const fs::path unreadable = PathIn(dir, "unreadable.txt");
    WriteAll(unreadable, "# Wallet dump\n");
    QCOMPARE(::chmod(unreadable.string().c_str(), 0), 0);
    if (::geteuid() != 0) Enter(console, QStringLiteral("importwallet ") + Quoted(unreadable));
#endif

    QVERIFY(fixture.prompts.empty());
    QVERIFY(fixture.wallet.m_console_begins.empty());
    const QString pane = PaneText(console);
    QVERIFY2(pane.contains(QStringLiteral("Cannot send %1:").arg(GUIUtil::boostPathToQString(PathIn(dir, "missing.txt")))),
             qPrintable(pane));
    QVERIFY2(pane.contains(QStringLiteral("Cannot send %1:").arg(dir.path())), qPrintable(pane));
    QVERIFY2(pane.contains(QStringLiteral("importwallet: the file is too large (%1 bytes; at most %2)")
                               .arg(64 * 1024 * 1024 + 1)
                               .arg(64 * 1024 * 1024)),
             qPrintable(pane));
#ifndef WIN32
    if (::geteuid() != 0) {
        QVERIFY2(pane.contains(QStringLiteral("Cannot send %1: it is not a file that can be read.")
                                   .arg(GUIUtil::boostPathToQString(unreadable))),
                 qPrintable(pane));
    }
#endif
}

// A cancel before the core's command runs stops the upload, and the pane says the command was not
// run. A cancel that comes while the command runs cannot stop it, and the pane shows its result.
void RPCConsoleTests::uploadCancelIsReportedUnlessTheCommandRan()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::Modal);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const fs::path local = PathIn(dir, "dump.txt");
    const std::string content = "# Wallet dump\n";
    WriteAll(local, content);

    for (const SinkGate gate_on : {SinkGate::Write, SinkGate::Finish}) {
        ConsoleWallet fixture;
        const auto log = std::make_shared<TransferLog>();
        std::atomic<bool> gate{false};
        std::atomic<bool> waiting{false};
        fixture.wallet.m_console_upload = [&](const std::string&, const std::vector<std::string>&, uint64_t,
                                              std::string&) {
            return std::make_unique<ConsoleSink>(log, &fixture.wallet.m_end_elevation_calls, "imported 1 key",
                                                 gate_on, &gate, &waiting);
        };

        RPCConsole console;
        console.setWalletModel(fixture.model.get());

        bool clicked = false;
        bool saw_cancelling = false;
        QTimer observer;
        CancelWhenWaiting(observer, this, waiting, gate, clicked, saw_cancelling);

        QTimer backstop;
        backstop.setSingleShot(true);
        connect(&backstop, &QTimer::timeout, this, [&gate] { gate = true; });
        backstop.start(10000);

        Enter(console, QStringLiteral("importwallet ") + Quoted(local));
        observer.stop();
        backstop.stop();

        QVERIFY(clicked);
        QVERIFY(saw_cancelling);

        const QString pane = PaneText(console);
        const QString shown = GUIUtil::boostPathToQString(local);

        if (gate_on == SinkGate::Write) {
            QCOMPARE(log->finishes, 0);
            QVERIFY(log->cancelled);
            QVERIFY2(pane.contains(QStringLiteral("Cancelled; importwallet was not run with %1.").arg(shown)),
                     qPrintable(pane));
        } else {
            QCOMPARE(log->finishes, 1);
            QVERIFY2(pane.contains(QStringLiteral("Sent %1 bytes from %2.").arg(content.size()).arg(shown)),
                     qPrintable(pane));
            QVERIFY2(pane.contains(QStringLiteral("imported 1 key")), qPrintable(pane));
        }

    }
}

// While a file command holds its capability, a command refused for the wallet's keys is not asked
// about: a second capability would displace the transfer's. Its refusal is shown as it is.
void RPCConsoleTests::noPromptWhileAFileCommandHoldsItsCapability()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::Modal);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const fs::path dest = PathIn(dir, "dump.txt");

    ConsoleWallet fixture;
    fixture.UnlockedForStakingOnly();
    const auto log = std::make_shared<TransferLog>();
    std::atomic<bool> gate{false};
    std::atomic<bool> waiting{false};
    fixture.wallet.m_console_download = [&](const std::string&, const std::vector<std::string>&, std::string&) {
        return std::make_unique<ConsoleSource>(log, &fixture.wallet.m_end_elevation_calls, "wallet-dump.txt",
                                               BytesOf(ABC), -1, "", "", &gate, &waiting);
    };

    RPCConsole console;
    console.setWalletModel(fixture.model.get());

    // While the download waits, the executor reports a command refused for the keys, as it can
    // inside the progress dialog's event loop.
    bool reported = false;
    QTimer observer;
    connect(&observer, &QTimer::timeout, this, [&] {
        if (reported || !waiting) return;
        reported = true;
        QMetaObject::invokeMethod(&console, "commandNeedsUnlock", Qt::DirectConnection,
                                  Q_ARG(QString, QStringLiteral("sendtoaddress SAddr 1")),
                                  Q_ARG(QString, QStringLiteral("Error: the keys are not available")));
        QMetaObject::invokeMethod(&console, "executorFinished", Qt::DirectConnection);
        gate = true;
    });
    observer.start(5);

    // So a regression fails rather than hangs.
    QTimer backstop;
    backstop.setSingleShot(true);
    connect(&backstop, &QTimer::timeout, this, [&gate] { gate = true; });
    backstop.start(10000);

    Enter(console, QStringLiteral("dumpwallet ") + Quoted(dest));
    observer.stop();
    backstop.stop();

    QVERIFY(reported);
    QCOMPARE(fixture.prompts, (std::vector<CapabilityKind>{CapabilityKind::FileTransfer}));
    QCOMPARE(fixture.wallet.m_begin_elevation_calls, 1);
    QCOMPARE(ReadAll(dest), ABC);
    QVERIFY2(PaneText(console).contains(QStringLiteral("Error: the keys are not available")),
             qPrintable(PaneText(console)));

    auto* input = console.findChild<QLineEdit*>(QStringLiteral("lineEdit"));
    QVERIFY(input);
    QVERIFY(input->isEnabled());
}
