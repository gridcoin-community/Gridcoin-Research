#include "rpcconsole.h"
#include "qt/guilog.h"
#include "ui_rpcconsole.h"

#ifndef Q_MOC_RUN
#include "clientmodel.h"
#include "qt/bantablemodel.h"
#include "qt/decoration.h"
#include "qt/filetransferdialog.h"
#include "guiutil.h"
#include "support/cleanse.h"
#include "support/lockedpool.h"
#include "util.h"
#include "util/strencodings.h"

#include <univalue.h>
#endif

#include <QByteArray>
#include <QThread>
#include <QTextEdit>
#include <QUrl>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QTime>
#include <QTimer>
#include <QStringList>

#include <openssl/crypto.h>

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

// TODO: make it possible to filter out categories (esp debug messages when implemented)
// TODO: receive errors and debug messages through ClientModel
const int CONSOLE_HISTORY = 50;

const QSize ICON_SIZE(24, 24);

const int INITIAL_TRAFFIC_GRAPH_MINS = 30;

const struct {
    const char *url;
    const char *source;
} ICON_MAPPING[] = {
    {"cmd-request", ":/icons/tx_input"},
    {"cmd-reply", ":/icons/tx_output"},
    {"cmd-error", ":/icons/tx_output"},
    {"misc", ":/icons/tx_inout"},
    {nullptr, nullptr}};

/* Object for executing console RPC commands in a separate thread.
*/
class RPCExecutor: public QObject
{
    Q_OBJECT
public:
    explicit RPCExecutor(interfaces::Node& node) : m_node(node) {}
public slots:
    void start();
    void request(const QString &command);
    //! Run \p command again, once. A refusal for the wallet's keys is then
    //! reported as an ordinary error, so it is never asked about twice.
    void retry(const QString &command);
signals:
    void reply(int category, const QString &command);
    //! \p command was refused for the wallet's keys; \p error is its reply.
    //! Sent instead of reply(), so the console can ask for the passphrase.
    void unlockNeeded(const QString &command, const QString &error);
    //! Sent after every request and retry, whatever its outcome.
    void finished();
private:
    interfaces::Node& m_node;

    void run(const QString &command, bool report_unlock_needed);
};

#include "rpcconsole.moc"

void RPCExecutor::start()
{
   // Nothing to do
}



QString toqstring(int o)
{
	std::string pre="";
	pre=strprintf("%d",o);
	QString str1 = QString::fromUtf8(pre.c_str());
	return str1;
}



namespace {
//! Overwrite a string's bytes in place; its size is kept.
void CleanseString(std::string& text)
{
    memory_cleanse(text.data(), text.size());
}

//! CleanseString over every string in a vector, however the scope that owns
//! the vector is left.
class StringsCleanser
{
public:
    explicit StringsCleanser(std::vector<std::string>& strings) : m_strings(strings) {}
    ~StringsCleanser()
    {
        for (std::string& text : m_strings) CleanseString(text);
    }

    StringsCleanser(const StringsCleanser&) = delete;
    StringsCleanser& operator=(const StringsCleanser&) = delete;

private:
    std::vector<std::string>& m_strings;
};

//! The UTF-8 bytes of a console line, the bytes toStdString() gives, overwritten
//! when this goes out of scope: the line can carry a passphrase. A QByteArray,
//! not a SecureString, because the helpers that use it take any QString, and the
//! secure pool refuses a buffer over 256 KiB.
class ConsoleLineBytes
{
public:
    explicit ConsoleLineBytes(const QString& line) : m_utf8(line.toUtf8()) {}
    ~ConsoleLineBytes()
    {
        if (!m_utf8.isEmpty()) memory_cleanse(m_utf8.data(), static_cast<size_t>(m_utf8.size()));
    }

    ConsoleLineBytes(const ConsoleLineBytes&) = delete;
    ConsoleLineBytes& operator=(const ConsoleLineBytes&) = delete;

    std::string_view view() const { return {m_utf8.constData(), static_cast<size_t>(m_utf8.size())}; }

private:
    QByteArray m_utf8;
};

//! \p text in a SecureString whose buffer is reserved before the bytes go in,
//! past the inline capacity of every standard library, so a short text is in
//! secure memory too and moving the string copies nothing.
SecureString ToSecureString(std::string_view text)
{
    SecureString out;
    out.reserve(std::max<size_t>(text.size(), 100));
    out.assign(text.data(), text.size());

    return out;
}

//! A copy of a console line in a std::string, overwritten when this goes out of scope: the line can
//! carry a passphrase.
class CleansedLine
{
public:
    explicit CleansedLine(std::string_view line) { m_text.assign(line.data(), line.size()); }
    ~CleansedLine() { CleanseString(m_text); }

    CleansedLine(const CleansedLine&) = delete;
    CleansedLine& operator=(const CleansedLine&) = delete;

    const std::string& text() const { return m_text; }

private:
    std::string m_text;
};
} // namespace

/**
 * Split shell command line into a list of arguments. Aims to emulate \c bash and friends.
 *
 * - Arguments are delimited with whitespace
 * - Extra whitespace at the beginning and end and between arguments will be ignored
 * - Text can be "double" or 'single' quoted
 * - The backslash \c \ is used as escape character
 *   - Outside quotes, any character can be escaped
 *   - Within double quotes, only escape \c " and backslashes before a \c " or another backslash
 *   - Within single quotes, no escaping is possible and no special interpretation takes place
 *
 * @param[out]   args        Parsed arguments will be appended to this list
 * @param[in]    strCommand  Command line to split
 */
bool parseCommandLine(std::vector<std::string> &args, std::string_view strCommand)
{
    enum CmdParseState
    {
        STATE_EATING_SPACES,
        STATE_ARGUMENT,
        STATE_SINGLEQUOTED,
        STATE_DOUBLEQUOTED,
        STATE_ESCAPE_OUTER,
        STATE_ESCAPE_DOUBLEQUOTED
    } state = STATE_EATING_SPACES;

    // The line can carry a passphrase, so no copy of an argument may be left
    // in memory that is freed without being overwritten. The argument being
    // built has room for the whole line, so it never moves to a larger buffer,
    // and it is overwritten as each argument is taken and on return. args has
    // a slot for every argument the line can hold (each but the last ends at a
    // space, tab or newline), so it never moves its strings either: a short one
    // is held in the vector's own block, and a move would leave it in the freed
    // one.
    std::string curarg;
    curarg.reserve(strCommand.size());
    struct CurargCleanser {
        std::string& text;
        ~CurargCleanser() { CleanseString(text); }
    } curarg_cleanser{curarg};

    const auto is_separator = [](char ch) { return ch == ' ' || ch == '\n' || ch == '\t'; };
    // A count is never negative, so the cast loses nothing.
    args.reserve(args.size() + static_cast<size_t>(std::count_if(strCommand.begin(), strCommand.end(), is_separator)) + 1);

    for (char ch : strCommand) {
        switch(state)
        {
        case STATE_ARGUMENT: // In or after argument
        case STATE_EATING_SPACES: // Handle runs of whitespace
            switch(ch)
            {
            case '"': state = STATE_DOUBLEQUOTED; break;
            case '\'': state = STATE_SINGLEQUOTED; break;
            case '\\': state = STATE_ESCAPE_OUTER; break;
            case ' ': case '\n': case '\t':
                if(state == STATE_ARGUMENT) // Space ends argument
                {
                    args.push_back(curarg);
                    CleanseString(curarg);
                    curarg.clear();
                }
                state = STATE_EATING_SPACES;
                break;
            default: curarg += ch; state = STATE_ARGUMENT;
            }
            break;
        case STATE_SINGLEQUOTED: // Single-quoted string
            switch(ch)
            {
            case '\'': state = STATE_ARGUMENT; break;
            default: curarg += ch;
            }
            break;
        case STATE_DOUBLEQUOTED: // Double-quoted string
            switch(ch)
            {
            case '"': state = STATE_ARGUMENT; break;
            case '\\': state = STATE_ESCAPE_DOUBLEQUOTED; break;
            default: curarg += ch;
            }
            break;
        case STATE_ESCAPE_OUTER: // '\' outside quotes
            curarg += ch; state = STATE_ARGUMENT;
            break;
        case STATE_ESCAPE_DOUBLEQUOTED: // '\' in double-quoted text
            if(ch != '"' && ch != '\\') curarg += '\\'; // keep '\' for everything but the quote and '\' itself
            curarg += ch; state = STATE_DOUBLEQUOTED;
            break;
        }
    }
    switch(state) // final state
    {
    case STATE_EATING_SPACES:
        return true;
    case STATE_ARGUMENT:
        args.push_back(curarg);
        return true;
    default: // ERROR to end in one of the other states
        return false;
    }
}

namespace {
//! Commands whose ARGUMENTS are the secret, so the command line itself must not
//! be echoed, logged or kept in history. Arguments only: dumpprivkey takes a
//! public address and returns a key, which is a separate concern, and
//! importwallet takes a path rather than key material.
//!
//! walletpassphrasechange is the sharpest case. Getting the old passphrase wrong
//! is exactly what makes the call fail, and the failure path is the one that
//! writes to the log -- carrying the new passphrase with it.
const std::set<std::string> SENSITIVE_ARG_COMMANDS = {
    // Carries settings values, and some settings are marked SENSITIVE precisely
    // because they are credentials -- rpcpassword and rpcuser among them. The RPC
    // reply masks those, but that does nothing for the copy the console echoes into
    // the pane and history, or for the verbatim line written to debug.log when the
    // call fails. Listing the whole command hides ordinary setting changes too;
    // that is the intended trade, because deciding per-argument here would mean
    // re-deriving which settings are sensitive in a second place and getting it
    // wrong silently.
    "changesettings",
    // Takes the HTLC preimage, which is meant to stay secret until the claim
    // is broadcast.
    "claimhtlc",
    "encryptwallet",
    "importprivkey",
    // Takes the phrase password, which restores the wallet together with the
    // phrase.
    "makeseedphrase",
    "restoreseedphrase",
    // Both alert commands take the WIF-encoded alert master private key as an
    // argument. The console echoes the line into the pane and the Up-arrow
    // history, and writes it verbatim to debug.log when the call fails -- and a
    // failed call is the common case here, since these are hand-typed by an
    // operator under time pressure.
    "sendalert",
    "sendalert2",
    "sethdseed",
    "signrawtransaction",
    "signrawtransactionwithkey",
    "walletpassphrase",
    "walletpassphrasechange",
};

//! Whether \p name is in SENSITIVE_ARG_COMMANDS, in any letter case. The
//! dispatcher matches a method name exactly, so a mis-cased one never runs, but
//! its arguments are just as secret in the history, the pane and the log.
bool IsSensitiveArgCommand(const std::string& name)
{
    return SENSITIVE_ARG_COMMANDS.count(ToLower(name)) != 0;
}
} // namespace

namespace RPCConsoleDetail {
//! Best-effort command name for text that parseCommandLine REJECTED, where there
//! is no parsed argv to consult. Reads the first token, seeing through one layer
//! of quoting, because a quoted name is how the whitespace-splitting version of
//! this guard was defeated. Used only on the failure path -- when the parser
//! succeeds, its argv is authoritative.
std::string LeadingCommandToken(const QString& command)
{
    const ConsoleLineBytes line(command);
    const std::string_view s = line.view();
    std::string_view::size_type i = 0;

    while (i < s.size() && IsSpace(s[i])) ++i;

    char quote = 0;
    if (i < s.size() && (s[i] == '"' || s[i] == '\'')) {
        quote = s[i];
        ++i;
    }

    const std::string_view::size_type start = i;
    while (i < s.size()) {
        const char c = s[i];
        const bool ends = quote ? (c == quote)
                                : (IsSpace(c) || c == '"' || c == '\'');
        if (ends) break;
        ++i;
    }

    return std::string(s.substr(start, i - start));
}

//! Replace the arguments of a secret-bearing command with a placeholder. The
//! command still executes with its real arguments; only the copies that are
//! displayed, logged or remembered are reduced.
//!
//! When the line parses, the name is taken from parseCommandLine rather than by
//! splitting on whitespace, so it sees the command exactly as the dispatcher
//! will. Splitting naively would leave `"walletpassphrase" secret 60` unredacted
//! while the parser strips the quotes and runs it -- the wrong way round for a
//! guard.
QString RedactSensitiveArgs(const QString& command)
{
    // The line can carry a passphrase, so the arguments parsed out of it are
    // overwritten however this returns, as are the line's bytes.
    std::vector<std::string> args;
    const StringsCleanser args_cleanser(args);

    const ConsoleLineBytes line(command);
    if (parseCommandLine(args, line.view()) && !args.empty()) {
        if (!IsSensitiveArgCommand(args[0])) return command;
        return QString::fromStdString(args[0]) + QStringLiteral(" (arguments not shown)");
    }

    // Unparseable. An earlier revision returned the text verbatim here, reasoning
    // that it would never dispatch and no command name had been resolved, so there
    // was nothing to redact. That inspects the wrong property. `walletpassphrase
    // "secret 60` fails on the unbalanced quote and is returned whole -- writing
    // the passphrase to the console pane and the Up-arrow history, which is the
    // precise disclosure this guard exists to prevent. Whether the command would
    // have run does not matter; the secret is in the text either way.
    //
    // So fall back to recognising the leading token. If it names a sensitive
    // command the arguments are dropped even though they were malformed; the user
    // still sees which command failed. Text whose first token is not recognised is
    // returned unchanged, so an ordinary typo is still visible to correct.
    const std::string token = LeadingCommandToken(command);
    if (!token.empty() && IsSensitiveArgCommand(token)) {
        return QString::fromStdString(token) + QStringLiteral(" (arguments not shown)");
    }

    return command;
}

interfaces::RpcConsoleResult DispatchConsoleCommand(interfaces::Node& node, std::vector<std::string>& args)
{
    if (args.empty()) return {};

    const std::string& method = args[0];
    const std::vector<int>* indexes = interfaces::RpcConsolePassphraseIndexes(method);

    if (indexes == nullptr) {
        return node.executeRpcConsoleCommand(method, std::vector<std::string>(args.begin() + 1, args.end()));
    }

    // The parsed passphrases are overwritten however this returns. In the
    // split build the call below crosses IPC, and a disconnect raises.
    struct PassphraseArgsCleanser {
        std::vector<std::string>& args;
        const std::vector<int>& indexes;

        ~PassphraseArgsCleanser()
        {
            for (const int index : indexes) {
                const size_t position = static_cast<size_t>(index) + 1;
                if (position < args.size()) CleanseString(args[position]);
            }
        }
    } passphrase_cleanser{args, *indexes};

    // A SecureString of n bytes takes n + 1 from one arena of the secure pool,
    // so a longer passphrase cannot be held at all. It is refused here, before
    // anything is built or sent, rather than thrown from the allocator.
    for (const int index : *indexes) {
        const size_t position = static_cast<size_t>(index) + 1;

        if (position < args.size() && args[position].size() > LockedPool::ARENA_SIZE - 1) {
            interfaces::RpcConsoleResult refused;
            refused.output = "Error: " + method + ": passphrase too long";
            return refused;
        }
    }

    // The vector is reserved first and each passphrase is built in a secure
    // buffer of its own, so no move copies one out of a string's inline storage,
    // which nothing cleanses. Fewer passphrases than the method takes go as
    // they are, and the node refuses the count.
    std::vector<SecureString> passphrases;
    passphrases.reserve(indexes->size());
    std::vector<std::string> others;

    for (size_t i = 1; i < args.size(); ++i) {
        const int position = static_cast<int>(i - 1);

        if (std::find(indexes->begin(), indexes->end(), position) == indexes->end()) {
            others.push_back(args[i]);
        } else {
            passphrases.push_back(ToSecureString(args[i]));
        }
    }

    interfaces::RpcConsoleResult result = node.executeRpcConsoleSecretCommand(method, passphrases, others);

    // The command carries its own passphrase, so a refusal for the wallet's
    // keys is not one the console's unlock prompt answers, and the command line
    // is not kept to be run again.
    result.wallet_unlock_needed = false;

    return result;
}

namespace {
//! Why a begin that returned nothing gave no reason.
std::string CouldNotBegin(const ConsoleFileRequest& request)
{
    return QObject::tr("%1 could not begin").arg(QString::fromUtf8(request.cmd->method)).toStdString();
}

//! A sink that passes every call on to \p inner and records whether finish() was called, which is
//! when the core's command runs.
class FinishWatch : public interfaces::FileSink
{
public:
    explicit FinishWatch(interfaces::FileSink& inner) : m_inner(inner) {}

    bool write(const std::vector<unsigned char>& data) override { return m_inner.write(data); }

    interfaces::FileSinkResult finish(uint64_t size, const uint256& sha256) override
    {
        m_finished = true;
        return m_inner.finish(size, sha256);
    }

    void cancel() override { m_inner.cancel(); }

    bool Finished() const { return m_finished; }

private:
    interfaces::FileSink& m_inner;
    bool m_finished{false};
};
} // namespace

std::optional<ConsoleFileRequest> ParseConsoleFileRequest(const std::string& line, std::string& error)
{
    error.clear();

    // Every line entered is parsed here, a passphrase command's too, so the arguments are
    // overwritten however this returns. A file command's arguments are no secret, and are copied
    // out.
    std::vector<std::string> args;
    const StringsCleanser args_cleanser(args);

    if (!parseCommandLine(args, line) || args.empty()) return std::nullopt;

    const interfaces::ConsoleFileCommand* cmd = interfaces::FindConsoleFileCommand(args[0]);
    if (!cmd) return std::nullopt;

    const size_t path_position = 1 + cmd->path_index;

    if (path_position >= args.size() || args[path_position].empty()) {
        error = QObject::tr("usage: %1 takes the file as its argument %2, as in help %1")
                    .arg(QString::fromStdString(args[0]), QString::number(static_cast<qulonglong>(path_position)))
                    .toStdString();
        return std::nullopt;
    }

    // As the RPC's own count check refuses more than it takes.
    if (args.size() - 1 > cmd->max_args) {
        error = QObject::tr("usage: %1 takes at most %2 argument(s), its file included, as in help %1")
                    .arg(QString::fromStdString(args[0]), QString::number(static_cast<qulonglong>(cmd->max_args)))
                    .toStdString();
        return std::nullopt;
    }

    ConsoleFileRequest request;
    request.cmd = cmd;

    // The argument is UTF-8, as every console argument is.
    fs::path path = GUIUtil::qstringToBoostPath(QString::fromStdString(args[path_position]));

    // A bare name goes in the data directory, as the RPC puts it in the core's; anything else is
    // where it says, from this process's working directory.
    if (!path.has_parent_path()) {
        path = GetDataDir() / path;
    } else if (!path.is_absolute()) {
        path = fs::absolute(path);
    }

    request.local_path = path;

    for (size_t i = 1; i < args.size(); ++i) {
        if (i != path_position) request.core_args.push_back(args[i]);
    }

    return request;
}

ConsoleFileResult RunConsoleDownload(interfaces::Wallet& wallet, const ConsoleFileRequest& request,
                                     const interfaces::ProgressFn& progress, const std::atomic<bool>& cancel)
{
    ConsoleFileResult result;

    // dumpcontracts runs its command in the core inside this call, and dumpwallet begins its dump
    // under the capability the caller holds. A call that throws (a lost connection) left no source.
    std::unique_ptr<interfaces::FileSource> source;

    try {
        source = wallet.beginConsoleDownload(request.cmd->method, request.core_args, result.error);
    } catch (const std::exception& e) {
        result.error = e.what();
        return result;
    }

    if (!source) {
        if (result.error.empty()) result.error = CouldNotBegin(request);
        return result;
    }

    // The source goes as this returns, on this thread. Destroying it ends the transfer in the core,
    // which deletes what it has not sent and ends the capability the source holds.
    std::vector<interfaces::FileEntryInfo> infos;

    try {
        infos = source->entries();
    } catch (const std::exception& e) {
        result.error = e.what();
        return result;
    }

    if (infos.size() != 1) {
        result.error = QObject::tr("the core listed %1 files for %2, which writes one")
                           .arg(QString::number(static_cast<qulonglong>(infos.size())),
                                QString::fromUtf8(request.cmd->method))
                           .toStdString();
        return result;
    }

    // The note is the command's reply, shown with the file once it has arrived. The file is
    // downloaded whatever its size: an entry with nothing to send refuses its first read, with its
    // reason, and that is what is shown.
    result.note = infos[0].note;
    result.outcome = interfaces::DownloadEntry(*source, 0, request.local_path, progress, cancel);

    return result;
}

ConsoleFileResult RunConsoleUpload(interfaces::Wallet& wallet, const ConsoleFileRequest& request, uint64_t size,
                                   const interfaces::ProgressFn& progress, const std::atomic<bool>& cancel)
{
    ConsoleFileResult result;
    result.size = size;

    std::unique_ptr<interfaces::FileSink> sink;

    try {
        sink = wallet.beginConsoleUpload(request.cmd->method, request.core_args, size, result.error);
    } catch (const std::exception& e) {
        result.error = e.what();
        return result;
    }

    if (!sink) {
        if (result.error.empty()) result.error = CouldNotBegin(request);
        return result;
    }

    // finish() runs the command in the core: importwallet imports the keys and rescans. A cancel
    // stops the upload only before then; once the command runs, its result is what is shown. The
    // sink goes as this returns, on this thread, and with it the capability it holds.
    FinishWatch watch(*sink);
    result.sink_result = interfaces::UploadFile(watch, request.local_path, progress, cancel);
    result.cancelled = !watch.Finished() && cancel.load();

    return result;
}

std::string ConsoleNoteForDestination(const std::string& note, const fs::path& dest)
{
    // dumpcontracts' reply names the file it wrote, which is the core's temporary one: that name is
    // never shown. The reply's other fields keep their order.
    static const std::string FILE_KEY{"exported_to_file"};

    UniValue reply;
    if (!reply.read(note) || !reply.isObject() || !reply.exists(FILE_KEY)) return note;

    UniValue shown(UniValue::VOBJ);
    const std::vector<std::string>& keys = reply.getKeys();
    const std::vector<UniValue>& values = reply.getValues();

    for (size_t i = 0; i < keys.size(); ++i) {
        if (keys[i] == FILE_KEY) {
            shown.pushKV(keys[i], GUIUtil::boostPathToQString(dest).toStdString());
        } else {
            shown.pushKV(keys[i], values[i]);
        }
    }

    return shown.write(2);
}

QString ConsoleFileResultText(const ConsoleFileRequest& request, const ConsoleFileResult& result, bool& failed)
{
    failed = true;
    const QString path = GUIUtil::boostPathToQString(request.local_path);

    if (!result.error.empty()) return QString::fromStdString(result.error);

    if (request.cmd->direction == interfaces::TransferDirection::Upload) {
        if (result.cancelled) {
            failed = false;
            return QObject::tr("Cancelled; %1 was not run with %2.").arg(QString::fromUtf8(request.cmd->method), path);
        }

        if (!result.sink_result.ok) return QString::fromStdString(result.sink_result.error);

        failed = false;
        QString text = QObject::tr("Sent %1 bytes from %2.")
                           .arg(QString::number(static_cast<qulonglong>(result.size)), path);
        if (!result.sink_result.output.empty()) text += "\n" + QString::fromStdString(result.sink_result.output);
        return text;
    }

    switch (result.outcome.status) {
    case interfaces::TransferStatus::Ok:
        break;
    case interfaces::TransferStatus::Cancelled:
        failed = false;
        return QObject::tr("Cancelled; nothing was written to %1.").arg(path);
    case interfaces::TransferStatus::SourceError:
    case interfaces::TransferStatus::LocalError:
    case interfaces::TransferStatus::VerifyFailed:
    case interfaces::TransferStatus::NoSpace:
        return QObject::tr("Nothing was written to %1: %2").arg(path, QString::fromStdString(result.outcome.message));
    }

    failed = false;

    // The SHA-256 in the order sha256sum prints it: the digest's bytes from the first.
    QString text = QObject::tr("Wrote %1 bytes to %2 (SHA-256 %3)")
                       .arg(QString::number(static_cast<qulonglong>(result.outcome.bytes)), path,
                            QString::fromStdString(HexStr(result.outcome.sha256)));

    if (!result.note.empty()) {
        text += "\n" + QString::fromStdString(ConsoleNoteForDestination(result.note, request.local_path));
    }

    return text;
}
} // namespace RPCConsoleDetail

void RPCExecutor::request(const QString &command)
{
    run(command, /*report_unlock_needed=*/true);
    emit finished();
}

void RPCExecutor::retry(const QString &command)
{
    run(command, /*report_unlock_needed=*/false);
    emit finished();
}

void RPCExecutor::run(const QString &command, bool report_unlock_needed)
{
    // The line can carry a passphrase, so every argument parsed out of it is
    // overwritten however this returns, and the line's bytes once it is parsed.
    // DispatchConsoleCommand sends the passphrases on as SecureStrings.
    std::vector<std::string> args;
    const StringsCleanser args_cleanser(args);

    bool parsed = false;
    {
        const ConsoleLineBytes line(command);
        parsed = parseCommandLine(args, line.view());
    }
    if(!parsed)
    {
        emit reply(RPCConsole::CMD_ERROR, QString("Parse error: unbalanced ' or \""));
        return;
    }
    if(args.empty())
        return; // Nothing to do

    // The dispatch, UniValue conversion and reply/error formatting all happen
    // node-side; the GUI receives the already-formatted text. This runs on the
    // executor thread, so a slow command does not block the GUI.
    const interfaces::RpcConsoleResult result = RPCConsoleDetail::DispatchConsoleCommand(m_node, args);

    if (result.ok)
    {
        emit reply(RPCConsole::CMD_REPLY, QString::fromStdString(result.output));
    }
    else
    {
        // This line persists: GUILog::LogPrintf reaches LogPrintStr, i.e.
        // debug.log. A failed command is the common case for the very commands
        // that carry a secret, so log the name without its arguments.
        GUILogPrintf("gridcoinresearch:  Handling Error [Request %s]...",
                     RPCConsoleDetail::RedactSensitiveArgs(command).toStdString());

        if (report_unlock_needed && result.wallet_unlock_needed) {
            emit unlockNeeded(command, QString::fromStdString(result.output));
        } else {
            emit reply(RPCConsole::CMD_ERROR, QString::fromStdString(result.output));
        }
    }
}

RPCConsole::RPCConsole(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RPCConsole),
    historyPtr(0)
{
    ui->setupUi(this);

    resize(GRC::ScaleSize(this, width(), height()));
    GRC::ScaleFontPointSize(ui->banHeading, 12);

#ifndef Q_OS_MACOS
    ui->openDebugLogfileButton->setIcon(QIcon(":/icons/export"));
    ui->showCLOptionsButton->setIcon(QIcon(":/icons/options"));
#endif

    // Install event filter for up and down arrow
    ui->lineEdit->installEventFilter(this);
    ui->messagesWidget->installEventFilter(this);

    connect(ui->clearButton, &QPushButton::clicked, this, &RPCConsole::clear);

    // set OpenSSL version label
    ui->openSSLVersion->setText(OpenSSL_version(OPENSSL_VERSION));

    // set Qt version label
    ui->qtVersion->setText("Qt " + QString::fromLocal8Bit(qVersion()) + " (built against " + QString::fromStdString(QT_VERSION_STR) + ")");

	setTrafficGraphRange(INITIAL_TRAFFIC_GRAPH_MINS);
    clear();


}

RPCConsole::~RPCConsole()
{
    // Normally the executor was already stopped on the setClientModel(nullptr)
    // teardown path; this is an idempotent backstop.
    stopExecutorThread();
    delete ui;
}

bool RPCConsole::eventFilter(QObject* obj, QEvent *event)
{
    if(event->type() == QEvent::KeyPress) // Special key handling
    {
        QKeyEvent *keyevt = static_cast<QKeyEvent*>(event);
        int key = keyevt->key();
        Qt::KeyboardModifiers mod = keyevt->modifiers();
        switch(key)
        {
        case Qt::Key_Up: if(obj == ui->lineEdit) { browseHistory(-1); return true; } break;
        case Qt::Key_Down: if(obj == ui->lineEdit) { browseHistory(1); return true; } break;
        case Qt::Key_PageUp: /* pass paging keys to messages widget */
        case Qt::Key_PageDown:
            if(obj == ui->lineEdit)
            {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                QApplication::postEvent(ui->messagesWidget, keyevt->clone());
#else
                QApplication::postEvent(ui->messagesWidget, new QKeyEvent(*keyevt));
#endif
                return true;
            }
            break;
        default:
            // Typing in messages widget brings focus to line edit, and redirects key there
            // Exclude most combinations and keys that emit no text, except paste shortcuts
            if(obj == ui->messagesWidget && (
                  (!mod && !keyevt->text().isEmpty() && key != Qt::Key_Tab) ||
                  ((mod & Qt::ControlModifier) && key == Qt::Key_V) ||
                  ((mod & Qt::ShiftModifier) && key == Qt::Key_Insert)))
            {
                ui->lineEdit->setFocus();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                QApplication::postEvent(ui->lineEdit, keyevt->clone());
#else
                QApplication::postEvent(ui->lineEdit, new QKeyEvent(*keyevt));
#endif
                return true;
            }
        }
    }
    return QDialog::eventFilter(obj, event);
}

void RPCConsole::setClientModel(ClientModel *model)
{

	clientModel = model;
	ui->trafficGraph->setClientModel(model);

    if (!model)
    {
        // Client model (and the interfaces::Node it exposes) is going away on
        // the shutdown teardown path: stop the executor thread now, before the
        // node is destroyed, so an in-flight/queued command cannot dereference
        // it. Safe no-op if the executor was never started.
        stopExecutorThread();
        return;
    }

    if(model && clientModel->getPeerTableModel() && clientModel->getBanTableModel())
    {
        // Subscribe to information, replies, messages, errors
        connect(model, &ClientModel::numConnectionsChanged, this, &RPCConsole::setNumConnections);
        connect(model, &ClientModel::numBlocksChanged, this, &RPCConsole::setNumBlocks);
        connect(model, &ClientModel::updateScraperLog, this, &RPCConsole::displayScraperLogMessage);

        updateTrafficStats(model->getTotalBytesRecv(), model->getTotalBytesSent());
        connect(model, &ClientModel::bytesChanged, this, &RPCConsole::updateTrafficStats);

        // set up peer table
        ui->peerWidget->setModel(model->getPeerTableModel());
        ui->peerWidget->verticalHeader()->hide();
        ui->peerWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui->peerWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
        ui->peerWidget->setContextMenuPolicy(Qt::CustomContextMenu);

        // Scale column widths by the logical DPI over 96.0 to deal with hires displays.
        ui->peerWidget->setColumnWidth(PeerTableModel::NetNodeId, GRC::ScalePx(this, NETNODEID_COLUMN_WIDTH));
        ui->peerWidget->setColumnWidth(PeerTableModel::Address, GRC::ScalePx(this, ADDRESS_COLUMN_WIDTH));
        ui->peerWidget->setColumnWidth(PeerTableModel::Ping, GRC::ScalePx(this, PING_COLUMN_WIDTH));
        ui->peerWidget->setColumnWidth(PeerTableModel::Sent, GRC::ScalePx(this, SENT_COLUMN_WIDTH));
        ui->peerWidget->setColumnWidth(PeerTableModel::Received, GRC::ScalePx(this, RECEIVED_COLUMN_WIDTH));
        ui->peerWidget->setColumnWidth(PeerTableModel::Subversion, GRC::ScalePx(this, SUBVERSION_COLUMN_WIDTH));
        ui->peerWidget->horizontalHeader()->setStretchLastSection(true);

        // Hide peerDetailWidget as initial state
        ui->peerDetailWidget->hide();

        // create peer table context menu actions
        QAction* disconnectAction = new QAction(tr("&Disconnect"), this);
        QAction* banAction1h      = new QAction(tr("Ban for") + " " + tr("1 &hour"), this);
        QAction* banAction24h     = new QAction(tr("Ban for") + " " + tr("1 &day"), this);
        QAction* banAction7d      = new QAction(tr("Ban for") + " " + tr("1 &week"), this);
        QAction* banAction365d    = new QAction(tr("Ban for") + " " + tr("1 &year"), this);

        // create peer table context menu
        peersTableContextMenu = new QMenu(this);
        peersTableContextMenu->addAction(disconnectAction);
        peersTableContextMenu->addAction(banAction1h);
        peersTableContextMenu->addAction(banAction24h);
        peersTableContextMenu->addAction(banAction7d);
        peersTableContextMenu->addAction(banAction365d);

        connect(banAction1h, &QAction::triggered, [this] { banSelectedNode(60 * 60); });
        connect(banAction24h, &QAction::triggered, [this] { banSelectedNode(60 * 60 * 24); });
        connect(banAction7d, &QAction::triggered, [this] { banSelectedNode(60 * 60 * 24 * 7); });
        connect(banAction365d, &QAction::triggered, [this] { banSelectedNode(60 * 60 * 24 * 365); });

        // peer table context menu signals
        connect(ui->peerWidget, &QTableView::customContextMenuRequested, this, &RPCConsole::showPeersTableContextMenu);
        connect(disconnectAction, &QAction::triggered, this, &RPCConsole::disconnectSelectedNode);

        // peer table signal handling - update peer details when selecting new node
        connect(ui->peerWidget->selectionModel(), &QItemSelectionModel::selectionChanged, this, &RPCConsole::peerSelected);
        // peer table signal handling - update peer details when new nodes are added to the model
        connect(model->getPeerTableModel(), &PeerTableModel::layoutChanged, this, &RPCConsole::peerLayoutChanged);
        // peer table signal handling - cache selected node ids
        connect(model->getPeerTableModel(), &PeerTableModel::layoutAboutToBeChanged, this, &RPCConsole::peerLayoutAboutToChange);

        // set up ban table
        ui->banlistWidget->setModel(model->getBanTableModel());
        ui->banlistWidget->verticalHeader()->hide();
        ui->banlistWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui->banlistWidget->setSelectionMode(QAbstractItemView::SingleSelection);
        ui->banlistWidget->setContextMenuPolicy(Qt::CustomContextMenu);
        ui->banlistWidget->setColumnWidth(BanTableModel::Address, BANSUBNET_COLUMN_WIDTH);
        ui->banlistWidget->setColumnWidth(BanTableModel::Bantime, BANTIME_COLUMN_WIDTH);
        ui->banlistWidget->horizontalHeader()->setStretchLastSection(true);

        // create ban table context menu action
        QAction* unbanAction = new QAction(tr("&Unban"), this);

        // create ban table context menu
        banTableContextMenu = new QMenu(this);
        banTableContextMenu->addAction(unbanAction);

        // ban table context menu signals
        connect(ui->banlistWidget, &QTableView::customContextMenuRequested, this, &RPCConsole::showBanTableContextMenu);
        connect(unbanAction, &QAction::triggered, this, &RPCConsole::unbanSelectedNode);

        // ban table signal handling - clear peer details when clicking a peer in the ban table
        connect(ui->banlistWidget, &QTableView::clicked, this, &RPCConsole::clearSelectedNode);
        // ban table signal handling - ensure ban table is shown or hidden (if empty)
        connect(model->getBanTableModel(), &BanTableModel::layoutChanged, this, &RPCConsole::showOrHideBanTableIfRequired);
        showOrHideBanTableIfRequired();

        // Provide initial values
		QString cvi = model->formatFullVersion() + "-" + toqstring(CLIENT_VERSION_MINOR);

	    ui->clientVersion->setText(cvi);
        ui->clientName->setText(model->clientName());
        ui->startupTime->setText(model->formatClientStartupTime());

        setNumConnections(model->getNumConnections());
        ui->isTestNet->setText(model->isTestNet() ? tr("Yes") : tr("No"));

        setNumBlocks(model->getNumBlocks(), model->getNumBlocksOfPeers());

		ui->boostVersion->setText(model->formatBoostVersion());

        //Setup autocomplete and attach it
        QStringList wordList;
        std::vector<std::string> commandList = model->node().listRpcCommands();
        for (size_t i = 0; i < commandList.size(); ++i)
        {
            wordList << commandList[i].c_str();
        }

        autoCompleter = new QCompleter(wordList, this);
        ui->lineEdit->setCompleter(autoCompleter);

        // Start the RPC executor now that we have a node to run commands
        // against (the console is unusable without a model). Guarded so a
        // second setClientModel(model) call cannot spawn a duplicate thread.
        if (!m_executorThread)
            startExecutor();
    }
}

static QString categoryClass(int category)
{
    switch(category)
    {
    case RPCConsole::CMD_REQUEST:  return "cmd-request"; break;
    case RPCConsole::CMD_REPLY:    return "cmd-reply"; break;
    case RPCConsole::CMD_ERROR:    return "cmd-error"; break;
    default:                       return "misc";
    }
}

void RPCConsole::clear()
{
    ui->messagesWidget->clear();
    // setText, not clear(): setText also empties the undo history, so the
    // cleared line cannot be brought back with Ctrl+Z.
    ui->lineEdit->setText(QString());
    ui->lineEdit->setFocus();

    // Add smoothly scaled icon images.
    // (when using width/height on an img, Qt uses nearest instead of linear interpolation)
    for(int i=0; ICON_MAPPING[i].url; ++i)
    {
        ui->messagesWidget->document()->addResource(
                    QTextDocument::ImageResource,
                    QUrl(ICON_MAPPING[i].url),
                    QImage(ICON_MAPPING[i].source).scaled(ICON_SIZE, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }

    // Set default style sheet
    ui->messagesWidget->document()->setDefaultStyleSheet(
                "table { }"
                "td.time { color: #808080; valign: bottom;} "
                "td.message { valign: bottom; } "
                "td.cmd-request { color: #006060; } "
                "td.cmd-error { color: red; } "
                "b { color: #006060; } "
                );

    message(CMD_REPLY, (tr("Welcome to the Gridcoin RPC console! ") + "<br>" +
                        tr("Use up and down arrows to navigate history, and <b>Ctrl-L</b> to clear screen.") + "<br>" +
                        tr("Type <b>help</b> for an overview of available commands.")), true);
}

void RPCConsole::message(int category, const QString &message, bool html)
{
    QTime time = QTime::currentTime();
    QString timeString = time.toString();
    QString out;
    out += "<table><tr><td class=\"time\" width=\"65\">" + timeString + "</td>";
    out += "<td class=\"icon\" width=\"32\"><img src=\"" + categoryClass(category) + "\"></td>";
    out += "<td class=\"message " + categoryClass(category) + "\" valign=\"middle\">";
    if(html)
        out += message;
    else
        out += GUIUtil::HtmlEscape(message, true);
    out += "</td></tr></table>";
    ui->messagesWidget->append(out);
}

void RPCConsole::setNumConnections(int count)
{
    ui->numberOfConnections->setText(QString::number(count));
}

void RPCConsole::setNumBlocks(int count, int countOfPeers)
{
    ui->numberOfBlocks->setText(QString::number(count));
    ui->totalBlocks->setText(QString::number(countOfPeers));
    if(clientModel)
    {
        // If there is no current number available display N/A instead of 0, which can't ever be true
        ui->totalBlocks->setText(clientModel->getNumBlocksOfPeers() == 0 ? tr("N/A") : QString::number(clientModel->getNumBlocksOfPeers()));
        ui->lastBlockTime->setText(clientModel->getLastBlockDate().toString());
        ui->diff->setText(QString::number(clientModel->getDifficulty(), 'f', 3));
    }
}

void RPCConsole::displayScraperLogMessage(const QString& string)
{
    // GUILogPrintf("INFO: RPCConsole::displayScraperLogMessage: %s", string.toStdString());

    ui->scraper_log->appendPlainText(string);
}

void RPCConsole::on_lineEdit_returnPressed()
{
    QString cmd = ui->lineEdit->text();
    // As in clear(): the history keeps a redacted copy of a secret-bearing
    // command, so the input must not keep the original on its undo stack.
    ui->lineEdit->setText(QString());

    if(!cmd.isEmpty())
    {
        // The command is dispatched in full, but the pane echo and the history
        // keep only the redacted form -- the pane ends up in screenshots pasted
        // into bug reports, and the history is one Up-arrow away for anyone at
        // the keyboard. This matches how upstream feeds its filtered command to
        // both sinks.
        const QString display_cmd = RPCConsoleDetail::RedactSensitiveArgs(cmd);
        message(CMD_REQUEST, display_cmd);
        // Remove command, if already in history
        history.removeOne(display_cmd);
        // Append command to history
        history.append(display_cmd);
        // Enforce maximum history size
        while(history.size() > CONSOLE_HISTORY)
            history.removeFirst();
        // Set pointer to end of history
        historyPtr = history.size();

        // A command whose file is on this machine runs here, through the file transfer, and never
        // reaches the node's console, which refuses it.
        std::string file_error;
        std::optional<RPCConsoleDetail::ConsoleFileRequest> file_request;
        {
            const ConsoleLineBytes bytes(cmd);
            const CleansedLine line(bytes.view());
            file_request = RPCConsoleDetail::ParseConsoleFileRequest(line.text(), file_error);
        }

        if (file_request) {
            runFileCommand(*file_request);
        } else if (!file_error.empty()) {
            message(CMD_ERROR, QString::fromStdString(file_error));
        } else {
            if (m_executorThread) {
                ++m_commands_in_flight;
            }
            emit cmdRequest(cmd);
        }

        // Scroll console view to end
        scrollToEnd();
    }
}

void RPCConsole::browseHistory(int offset)
{
    historyPtr += offset;
    if(historyPtr < 0)
        historyPtr = 0;
    if(historyPtr > history.size())
        historyPtr = history.size();
    QString cmd;
    if(historyPtr < history.size())
        cmd = history.at(historyPtr);
    ui->lineEdit->setText(cmd);
}

void RPCConsole::startExecutor()
{
    // Long-lived like a GuiIpcWorker: it ends only at teardown (stopExecutorThread), while the connection to the
    // core is open, or after a drop already destroyed it on the loop thread. Its one exposure: a drop whose
    // destruction of the connection is still running on the loop thread as teardown stops this thread.
    QThread* thread = new QThread;
    m_executorThread = thread;
    RPCExecutor *executor = new RPCExecutor(clientModel->node());
    executor->moveToThread(thread);

    // Notify executor when thread started (in executor thread)
    connect(thread, &QThread::started, executor, &RPCExecutor::start);
    // Replies from executor object must go to this object
    connect(executor, &RPCExecutor::reply, this, [this](int cat, const QString &msg){this->message(cat, msg);});
    // Requests from this object must go to executor
    connect(this, &RPCConsole::cmdRequest, executor, &RPCExecutor::request);
    connect(this, &RPCConsole::cmdRetry, executor, &RPCExecutor::retry);
    // A refusal for the wallet's keys, and the end of every request, come back
    // in the order sent: the refusal first.
    connect(executor, &RPCExecutor::unlockNeeded, this, &RPCConsole::commandNeedsUnlock);
    connect(executor, &RPCExecutor::finished, this, &RPCConsole::executorFinished);
    // On stopExecutor signal
    // - queue executor for deletion (in execution thread)
    // - quit the Qt event loop in the execution thread
    connect(this, &RPCConsole::stopExecutor, executor, &RPCExecutor::deleteLater);
    connect(this, &RPCConsole::stopExecutor, thread, &QThread::quit);
    // Queue the thread for deletion (in this thread) when it is finished
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    // Default implementation of QThread::run() simply spins up an event loop in the thread,
    // which is what we want.
    thread->start();
}

void RPCConsole::runFileCommand(const RPCConsoleDetail::ConsoleFileRequest& request)
{
    if (!walletModel) {
        message(CMD_ERROR, tr("File commands need a loaded wallet."));
        return;
    }

    const QString method = QString::fromUtf8(request.cmd->method);
    const QString path = GUIUtil::boostPathToQString(request.local_path);
    const bool download = request.cmd->direction == interfaces::TransferDirection::Download;
    uint64_t size = 0;

    // Everything that can refuse here does so before the passphrase is asked for.
    if (download) {
        // The core's own files, which a download must not replace. In the split build they were
        // read when the GUI connected, so nothing crosses IPC on this thread. The monolith filled
        // them before its wallet was loaded, and asks its node in this process now.
        const interfaces::CorePaths core_paths =
            (m_ipc_info.active || !clientModel) ? m_ipc_info.core_paths : clientModel->node().getCorePaths();
        std::string error;

        if (!interfaces::CheckDestination(request.local_path, BackupGuardFiles(core_paths),
                                          fs::path(core_paths.datadir), error)) {
            message(CMD_ERROR, tr("Nothing was written to %1: %2").arg(path, QString::fromStdString(error)));
            return;
        }

        // A download never replaces what is there: a bare name lands in the data directory, among
        // files the check above does not list. A link counts, also one that leads nowhere, and so
        // does anything that cannot be examined.
        boost::system::error_code ec;
        if (fs::symlink_status(request.local_path, ec).type() != fs::file_not_found) {
            message(CMD_ERROR, tr("File already exists at location. Please delete or rename old file first."));
            return;
        }
    } else {
        boost::system::error_code ec;
        const bool regular = fs::is_regular_file(request.local_path, ec);
        const uintmax_t file_size = regular ? fs::file_size(request.local_path, ec) : 0;
        const bool readable = regular && !ec && fsbridge::ifstream(request.local_path, std::ios::binary).is_open();

        if (!readable) {
            message(CMD_ERROR, tr("Cannot send %1: it is not a file that can be read.").arg(path));
            return;
        }

        size = file_size;

        // The core refuses it too; this spares the prompt.
        if (size > request.cmd->max_upload_bytes) {
            message(CMD_ERROR, tr("%1: the file is too large (%2 bytes; at most %3)")
                                   .arg(method, QString::number(static_cast<qulonglong>(size)),
                                        QString::number(static_cast<qulonglong>(request.cmd->max_upload_bytes))));
            return;
        }
    }

    // No other command is entered, and no other prompt raised, while this one runs.
    ui->lineEdit->setEnabled(false);
    m_file_command_running = true;

    struct Running {
        RPCConsole& console;
        ~Running()
        {
            console.m_file_command_running = false;
            console.ui->lineEdit->setEnabled(true);
            console.ui->lineEdit->setFocus();
        }
    } running{*this};

    RPCConsoleDetail::ConsoleFileResult result;
    std::string run_error;
    bool ran = false;

    {
        // One FileTransfer capability for this command alone: a wallet that needs one asks for the
        // passphrase, an unencrypted or fully unlocked one asks nothing and needs none. The context
        // lives until the job below has destroyed the source or sink, since ending the capability
        // ends the transfer; it ends the capability as it goes.
        const WalletModel::UnlockContext ctx(walletModel->requestUnlock(CapabilityKind::FileTransfer));

        if (!ctx.isValid()) {
            message(CMD_ERROR, tr("%1 was not run: the wallet was not unlocked for it.").arg(method));
            return;
        }

        // Every call into the core this command makes runs on the modal worker, under the progress
        // dialog: dumpcontracts runs its command when the transfer begins, and importwallet imports
        // and rescans when it finishes.
        interfaces::Wallet& wallet = walletModel->wallet();
        const QString what = download ? tr("%1 to %2").arg(method, path) : tr("%1 from %2").arg(method, path);

        ran = RunJobWithProgress(
            this, method, what,
            [&](const interfaces::ProgressFn& progress, const std::atomic<bool>& cancel) {
                result = download ? RPCConsoleDetail::RunConsoleDownload(wallet, request, progress, cancel)
                                  : RPCConsoleDetail::RunConsoleUpload(wallet, request, size, progress, cancel);
            },
            run_error);
    }

    if (!ran) {
        message(CMD_ERROR, QString::fromStdString(run_error));
        return;
    }

    bool failed = false;
    const QString text = RPCConsoleDetail::ConsoleFileResultText(request, result, failed);
    message(failed ? CMD_ERROR : CMD_REPLY, text);
}

void RPCConsole::setWalletModel(WalletModel *model)
{
    if (!model) {
        // End a capability still held for a command run again, through the
        // model it was begun with, which is still alive here.
        m_retry_unlock.reset();
        m_unlock_pending = false;
        ui->lineEdit->setEnabled(true);
    }

    walletModel = model;
}

void RPCConsole::commandNeedsUnlock(const QString &command, const QString &error)
{
    // Acted on in executorFinished(), once this request is known to be the only
    // one in flight.
    m_unlock_pending = true;
    m_unlock_command = command;
    m_unlock_error = error;
}

void RPCConsole::executorFinished()
{
    if (m_commands_in_flight > 0) {
        --m_commands_in_flight;
    }

    // The command run again has finished, so its capability ends now.
    if (m_retry_unlock) {
        m_retry_unlock.reset();
        ui->lineEdit->setEnabled(true);
        ui->lineEdit->setFocus();
        return;
    }

    if (!m_unlock_pending) {
        return;
    }

    m_unlock_pending = false;
    const QString command = std::exchange(m_unlock_command, QString());
    const QString error = std::exchange(m_unlock_error, QString());

    // A command refused for the wallet's keys on a wallet unlocked for staking
    // only gets one prompt. The passphrase begins an RpcRequest capability, the
    // same command runs once more under it, and the capability ends when it has.
    // The console never asks before a command runs, only after one is refused.
    //
    // Asked only when nothing else is queued behind it: every command the
    // executor runs while the capability is held runs under it, and those were
    // not what the user entered the passphrase for. On a locked wallet the
    // refusal stands; walletpassphrase is the console's way to unlock it.
    //
    // Nor while another operation's prompt is open. This runs as a queued
    // event, so it can run inside that prompt's event loop, and a second
    // prompt would be refused there anyway.
    //
    // Nor while a file command runs (inside its progress dialog's event loop):
    // the IPC slot holds one capability, and beginning this one would end the
    // transfer's.
    if (m_commands_in_flight != 0 || !walletModel || m_file_command_running
            || walletModel->getEncryptionStatus() != WalletModel::UnlockedForStakingOnly
            || walletModel->isUnlockPromptOpen()) {
        message(CMD_ERROR, error);
        return;
    }

    // Nothing else may be entered while the capability is held.
    ui->lineEdit->setEnabled(false);

    auto unlock = std::make_unique<WalletModel::UnlockContext>(
        walletModel->requestUnlock(CapabilityKind::RpcRequest));

    if (!unlock->isValid()) {
        ui->lineEdit->setEnabled(true);
        ui->lineEdit->setFocus();
        message(CMD_ERROR, error);
        return;
    }

    m_retry_unlock = std::move(unlock);
    ++m_commands_in_flight;
    emit cmdRetry(command);
}

void RPCConsole::stopExecutorThread()
{
    if (!m_executorThread)
        return;

    // stopExecutor quits the worker's event loop and queues the executor for
    // deletion; wait() blocks until run() has returned so no further request()
    // can dereference the interfaces::Node. If a command is in flight this
    // drains it first, mirroring how the other GUI workers are quiesced on the
    // model-teardown path. The thread object self-deletes via finished->
    // deleteLater; drop our pointer so this is idempotent.
    emit stopExecutor();
    m_executorThread->wait();
    m_executorThread = nullptr;

    // Requests still queued were dropped with the executor.
    m_commands_in_flight = 0;
    m_unlock_pending = false;
}


void RPCConsole::on_graphRangeSlider_valueChanged(int value)
{
    const int multiplier = 5; // each position on the slider represents 5 min
    int mins = value * multiplier;
    setTrafficGraphRange(mins);
}

QString RPCConsole::FormatBytes(quint64 bytes)
{
    if(bytes < 1024)
        return QString(tr("%1 B")).arg(bytes);
    if(bytes < 1024 * 1024)
        return QString(tr("%1 KB")).arg(bytes / 1024);
    if(bytes < 1024 * 1024 * 1024)
        return QString(tr("%1 MB")).arg(bytes / 1024 / 1024);

    return QString(tr("%1 GB")).arg(bytes / 1024 / 1024 / 1024);
}

void RPCConsole::setTrafficGraphRange(int mins)
{
    ui->trafficGraph->setGraphRangeMins(mins);
    if(mins < 60) {
        ui->graphRangeLabel->setText(QString(tr("%1 m")).arg(mins));
    } else {
        int hours = mins / 60;
        int minsLeft = mins % 60;
        if(minsLeft == 0) {
            ui->graphRangeLabel->setText(QString(tr("%1 h")).arg(hours));
        } else {
            ui->graphRangeLabel->setText(QString(tr("%1 h %2 m")).arg(hours).arg(minsLeft));
        }
    }
}

void RPCConsole::updateTrafficStats(quint64 totalBytesIn, quint64 totalBytesOut)
{
    ui->bytesInLabel->setText(FormatBytes(totalBytesIn));
    ui->bytesOutLabel->setText(FormatBytes(totalBytesOut));
}

void RPCConsole::on_clearTrafficGraphButton_clicked()
{
    ui->trafficGraph->clear();
}

void RPCConsole::on_tabWidget_currentChanged(int index)
{
    if(ui->tabWidget->widget(index) == ui->tab_console)
    {
        ui->lineEdit->setFocus();
    }
}

void RPCConsole::setIpcInfo(const GuiIpcInfo& info)
{
    m_ipc_info = info;
}

void RPCConsole::on_openDebugLogfileButton_clicked()
{
    GUIUtil::openDebugLogfile(m_ipc_info, this);
}

void RPCConsole::scrollToEnd()
{
    QScrollBar *scrollbar = ui->messagesWidget->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}

void RPCConsole::on_showCLOptionsButton_clicked()
{
    GUIUtil::HelpMessageBox help;
    help.exec();
}

void RPCConsole::peerSelected(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(deselected);

    if (!clientModel || !clientModel->getPeerTableModel() || selected.indexes().isEmpty())
        return;

    const interfaces::PeerInfo *stats = clientModel->getPeerTableModel()->getNodeStats(selected.indexes().first().row());
    if (stats)
        updateNodeDetail(stats);
}

void RPCConsole::peerLayoutAboutToChange()
{
    QModelIndexList selected = ui->peerWidget->selectionModel()->selectedIndexes();
    cachedNodeids.clear();
    for(int i = 0; i < selected.size(); i++)
    {
        const interfaces::PeerInfo *stats = clientModel->getPeerTableModel()->getNodeStats(selected.at(i).row());
        cachedNodeids.append(stats->id);
    }
}

void RPCConsole::peerLayoutChanged()
{
    if (!clientModel || !clientModel->getPeerTableModel())
        return;

    const interfaces::PeerInfo *stats = nullptr;
    bool fUnselect = false;
    bool fReselect = false;

    if (cachedNodeids.empty()) // no node selected yet
        return;

    // find the currently selected row
    int selectedRow = -1;
    QModelIndexList selectedModelIndex = ui->peerWidget->selectionModel()->selectedIndexes();
    if (!selectedModelIndex.isEmpty()) {
        selectedRow = selectedModelIndex.first().row();
    }

    // check if our detail node has a row in the table (it may not necessarily
    // be at selectedRow since its position can change after a layout change)
    int detailNodeRow = clientModel->getPeerTableModel()->getRowByNodeId(cachedNodeids.first());

    if (detailNodeRow < 0)
    {
        // detail node disappeared from table (node disconnected)
        fUnselect = true;
    }
    else
    {
        if (detailNodeRow != selectedRow)
        {
            // detail node moved position
            fUnselect = true;
            fReselect = true;
        }

        // get fresh stats on the detail node.
        stats = clientModel->getPeerTableModel()->getNodeStats(detailNodeRow);
    }

    if (fUnselect && selectedRow >= 0) {
        clearSelectedNode();
    }

    if (fReselect)
    {
        for(int i = 0; i < cachedNodeids.size(); i++)
        {
            ui->peerWidget->selectRow(clientModel->getPeerTableModel()->getRowByNodeId(cachedNodeids.at(i)));
        }
    }

    if (stats)
        updateNodeDetail(stats);
}

void RPCConsole::updateNodeDetail(const interfaces::PeerInfo *stats)
{
    // update the detail ui with latest node information
    QString peerAddrDetails(QString::fromStdString(stats->addr_name) + " ");
    peerAddrDetails += tr("(node id: %1)").arg(QString::number(stats->id));
    if (!stats->addr_local.empty())
        peerAddrDetails += "<br />" + tr("via %1").arg(QString::fromStdString(stats->addr_local));
    ui->peerHeading->setText(peerAddrDetails);
    ui->peerServices->setText(GUIUtil::formatServicesStr(stats->services));
    ui->peerLastSend->setText(stats->last_send ? GUIUtil::formatDurationStr(GetTimeSeconds() - stats->last_send) : tr("never"));
    ui->peerLastRecv->setText(stats->last_recv ? GUIUtil::formatDurationStr(GetTimeSeconds() - stats->last_recv) : tr("never"));
    ui->peerBytesSent->setText(GUIUtil::formatBytes(stats->send_bytes));
    ui->peerBytesRecv->setText(GUIUtil::formatBytes(stats->recv_bytes));
    ui->peerConnTime->setText(GUIUtil::formatDurationStr(GetTimeSeconds() - stats->time_connected));
    ui->peerPingTime->setText(GUIUtil::formatPingTime(stats->ping_time));
    ui->peerPingWait->setText(GUIUtil::formatPingTime(stats->ping_wait));
    ui->peerMinPing->setText(GUIUtil::formatPingTime(stats->min_ping));
    ui->timeoffset->setText(GUIUtil::formatTimeOffset(stats->time_offset));
    ui->peerVersion->setText(QString("%1").arg(QString::number(stats->version)));
    if (!stats->subversion.empty()) {
        // remove leading and trailing slash
        ui->peerSubversion->setText(QString::fromStdString(stats->subversion.substr(1, stats->subversion.length() - 2)));
    } else {
        ui->peerSubversion->clear();
    }
    ui->peerDirection->setText(stats->inbound ? tr("Inbound") : tr("Outbound"));
    ui->peerHeight->setText(QString("%1").arg(QString::number(stats->starting_height)));
    ui->peerBanScore->setText(QString("%1").arg(stats->misbehavior));

    // Sync/common height are not tracked yet (there is no CNodeStateStats), so
    // the peerSyncHeight/peerCommonHeight labels keep their designer default and
    // are intentionally left untouched here.

    ui->peerDetailWidget->show();
}

void RPCConsole::showPeersTab()
{
    show();
    ui->tabWidget->setCurrentWidget(ui->tab_peers);
}


void RPCConsole::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void RPCConsole::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    if (!clientModel || !clientModel->getPeerTableModel())
        return;

    // start PeerTableModel auto refresh
    clientModel->getPeerTableModel()->startAutoRefresh();
}

void RPCConsole::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);

    if (!clientModel || !clientModel->getPeerTableModel())
        return;

    // stop PeerTableModel auto refresh
    clientModel->getPeerTableModel()->stopAutoRefresh();
}

void RPCConsole::showPeersTableContextMenu(const QPoint& point)
{
    QModelIndex index = ui->peerWidget->indexAt(point);
    if (index.isValid())
        peersTableContextMenu->exec(QCursor::pos());
}

void RPCConsole::showBanTableContextMenu(const QPoint& point)
{
    QModelIndex index = ui->banlistWidget->indexAt(point);
    if (index.isValid())
        banTableContextMenu->exec(QCursor::pos());
}

void RPCConsole::banSelectedNode(int bantime)
{
    if (!clientModel)
        return;

    // Ban (and disconnect) each selected peer by its connection id. The node
    // side looks the peer's address up and applies the ban + disconnect.
    QList<QModelIndex> nodes = GUIUtil::getEntryData(ui->peerWidget, PeerTableModel::NetNodeId);
    for(int i = 0; i < nodes.count(); i++)
    {
        const int64_t id = nodes.at(i).data().toLongLong();
        clientModel->node().banNode(id, bantime);
    }
    clearSelectedNode();
    clientModel->getBanTableModel()->refresh();
}

void RPCConsole::unbanSelectedNode()
{
    if (!clientModel)
        return;

    // Get selected ban addresses
    QList<QModelIndex> nodes = GUIUtil::getEntryData(ui->banlistWidget, BanTableModel::Address);
    for(int i = 0; i < nodes.count(); i++)
    {
        // Get currently selected ban address
        QString strNode = nodes.at(i).data().toString();

        if (clientModel->node().unban(strNode.toStdString()))
        {
            clientModel->getBanTableModel()->refresh();
        }
    }
}

void RPCConsole::clearSelectedNode()
{
    ui->peerWidget->selectionModel()->clearSelection();
    cachedNodeids.clear();
    ui->peerDetailWidget->hide();
    ui->peerHeading->setText(tr("Select a peer to view detailed information."));
}

void RPCConsole::showOrHideBanTableIfRequired()
{
    if (!clientModel)
        return;

    bool visible = clientModel->getBanTableModel()->shouldShow();
    ui->banlistWidget->setVisible(visible);
    ui->banHeading->setVisible(visible);
}

void RPCConsole::disconnectSelectedNode()
{
    if (!clientModel)
        return;

    // Disconnect each selected peer by its connection id, node-side.
    QList<QModelIndex> nodes = GUIUtil::getEntryData(ui->peerWidget, PeerTableModel::NetNodeId);
    for(int i = 0; i < nodes.count(); i++)
    {
        const int64_t id = nodes.at(i).data().toLongLong();
        if (clientModel->node().disconnectNode(id))
            clearSelectedNode();
    }
}

