// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/filetransferdialogtests.h"

#include "crypto/sha256.h"
#include "fs.h"
#include "interfaces/file_transfer.h"
#include "interfaces/node.h"
#include "qt/filetransferdialog.h"
#include "qt/guiipcworker.h"
#include "qt/guiutil.h"
#include "sync.h"
#include "uint256.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QProgressDialog>
#include <QPushButton>
#include <QString>
#include <QTemporaryDir>
#include <QTimer>
#include <QWidget>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <future>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace std::chrono_literals;

std::vector<unsigned char> Bytes(size_t n)
{
    std::vector<unsigned char> v(n);

    for (size_t i = 0; i < n; ++i) {
        v[i] = static_cast<unsigned char>((i * 131 + (i >> 8)) & 0xff);
    }

    return v;
}

uint256 Sha256Of(const std::vector<unsigned char>& v)
{
    uint256 out;
    CSHA256().Write(v.data(), v.size()).Finalize(out.begin());
    return out;
}

std::string ReadAll(const fs::path& p)
{
    std::ifstream in(p.string(), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

//! A one-entry source that sends small chunks, so a transfer takes several reads. Its gated
//! read, if any, waits until the test releases it. Once cancelled, every read is an error, as
//! with the core's snapshot source.
class GatedSource : public interfaces::FileSource
{
public:
    static constexpr size_t CHUNK = 64 * 1024;

    explicit GatedSource(std::vector<unsigned char> data, int gate_on_read = 0)
        : m_data(std::move(data)), m_gate_on_read(gate_on_read)
    {
    }

    std::vector<interfaces::FileEntryInfo> entries() override
    {
        interfaces::FileEntryInfo e;
        e.name = "wallet.dat";
        e.size = static_cast<int64_t>(m_data.size());
        e.size_hint = m_data.size();
        return {e};
    }

    interfaces::FileChunk read(uint32_t, uint32_t max_bytes) override
    {
        const int n = ++m_reads;

        {
            WAIT_LOCK(m_mutex, lock);
            m_read_threads.push_back(std::this_thread::get_id());

            if (n == m_gate_on_read) {
                m_waiting = true;
                m_cv.wait(lock, [this]() EXCLUSIVE_LOCKS_REQUIRED(m_mutex) { return m_released; });
            }
        }

        interfaces::FileChunk c;

        if (m_cancelled) {
            c.error = "the transfer was cancelled";
            return c;
        }

        const size_t len = std::min<size_t>({static_cast<size_t>(max_bytes), CHUNK, m_data.size() - m_pos});
        c.data.assign(m_data.begin() + m_pos, m_data.begin() + m_pos + len);
        m_pos += len;
        c.eof = m_pos == m_data.size();
        return c;
    }

    interfaces::FileTrailer trailer(uint32_t) override
    {
        interfaces::FileTrailer t;
        t.ok = true;
        t.size = m_data.size();
        t.sha256 = Sha256Of(m_data);
        return t;
    }

    void cancel() override { m_cancelled = true; }

    void Release()
    {
        LOCK(m_mutex);
        m_released = true;
        m_cv.notify_all();
    }

    //! The gated read has begun and waits.
    bool Waiting()
    {
        LOCK(m_mutex);
        return m_waiting && !m_released;
    }

    std::vector<std::thread::id> ReadThreads()
    {
        LOCK(m_mutex);
        return m_read_threads;
    }

    int Reads() const { return m_reads; }
    bool Cancelled() const { return m_cancelled; }
    const std::vector<unsigned char>& Data() const { return m_data; }

private:
    const std::vector<unsigned char> m_data;
    const int m_gate_on_read;
    //! Read by the worker thread only.
    size_t m_pos{0};
    std::atomic<int> m_reads{0};
    std::atomic<bool> m_cancelled{false};
    Mutex m_mutex;
    std::condition_variable m_cv;
    bool m_waiting GUARDED_BY(m_mutex){false};
    bool m_released GUARDED_BY(m_mutex){false};
    std::vector<std::thread::id> m_read_threads GUARDED_BY(m_mutex){};
};

class FakeSink : public interfaces::FileSink
{
public:
    bool write(const std::vector<unsigned char>& data) override
    {
        m_received.insert(m_received.end(), data.begin(), data.end());
        return true;
    }

    interfaces::FileSinkResult finish(uint64_t size, const uint256& sha256) override
    {
        m_finish_size = size;
        m_finish_sha = sha256;
        interfaces::FileSinkResult r;
        r.ok = true;
        r.output = "imported";
        return r;
    }

    void cancel() override { m_cancelled = true; }

    // Written by the worker thread, and read by the test once the upload has returned.
    std::vector<unsigned char> m_received{};
    uint64_t m_finish_size{0};
    uint256 m_finish_sha{};
    bool m_cancelled{false};
};

//! A GatedSource that records how often it was read, and on which thread it was destroyed.
class RecordingSource : public GatedSource
{
public:
    RecordingSource(std::vector<unsigned char> data, std::atomic<int>& reads,
                    std::atomic<std::thread::id>& destroyed_on)
        : GatedSource(std::move(data)), m_reads_out(reads), m_destroyed_on(destroyed_on)
    {
    }

    ~RecordingSource() override
    {
        m_reads_out = Reads();
        m_destroyed_on = std::this_thread::get_id();
    }

private:
    std::atomic<int>& m_reads_out;
    std::atomic<std::thread::id>& m_destroyed_on;
};

//! A backup's begin that waits, as the core's does while it copies the wallet, until the test
//! releases it or five seconds have passed, and then hands over a RecordingSource of \p data.
class SlowBegin
{
public:
    explicit SlowBegin(std::vector<unsigned char> data) : m_data(std::move(data)) {}

    BackupBeginFn Fn()
    {
        return [this](std::string&) -> std::unique_ptr<interfaces::FileSource> {
            m_thread = std::this_thread::get_id();
            m_copying = true;
            m_released.wait_for(5s);
            m_copying = false;
            return std::make_unique<RecordingSource>(m_data, m_reads, m_destroyed_on);
        };
    }

    //! GUI thread only.
    void Release()
    {
        if (m_release_set) return;
        m_release_set = true;
        m_release.set_value();
    }

    bool Copying() const { return m_copying; }
    std::thread::id Thread() const { return m_thread; }
    std::thread::id DestroyedOn() const { return m_destroyed_on; }
    int Reads() const { return m_reads; }
    const std::vector<unsigned char>& Data() const { return m_data; }

private:
    const std::vector<unsigned char> m_data;
    std::promise<void> m_release;
    const std::shared_future<void> m_released{m_release.get_future().share()};
    bool m_release_set{false};
    std::atomic<bool> m_copying{false};
    std::atomic<std::thread::id> m_thread{};
    std::atomic<std::thread::id> m_destroyed_on{};
    std::atomic<int> m_reads{-1};
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

//! QCoreApplication::exit() leaves the application marked as quitting, and
//! every later QEventLoop::exec() then returns at once. QCoreApplication::exec()
//! clears the mark; run it once, ended by an exit it has queued for itself.
void ClearApplicationQuit()
{
    QTimer::singleShot(0, qApp, [] { QCoreApplication::exit(0); });
    QCoreApplication::exec();
}

int EntriesIn(const QTemporaryDir& dir)
{
    return static_cast<int>(
        QDir(dir.path()).entryList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System).size());
}

const QString CANCELLING_TEXT{"Cancelling..."};
} // namespace

// Outside a worker's lifetime nothing is read or written.
void FileTransferDialogTests::downloadWithoutWorkerIsALocalError()
{
    QVERIFY(GuiIpcWorker::Current(GuiIpcWorker::Role::Modal) == nullptr);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const fs::path dest = GUIUtil::qstringToBoostPath(dir.path()) / "backup.dat";

    GatedSource src(Bytes(1000));
    const interfaces::TransferOutcome out = RunDownloadWithProgress(nullptr, "Backup Wallet", src, 0, dest);

    QVERIFY(out.status == interfaces::TransferStatus::LocalError);
    QVERIFY(!out.message.empty());
    QCOMPARE(src.Reads(), 0);

    {
        std::ofstream file((GUIUtil::qstringToBoostPath(dir.path()) / "up.bin").string(), std::ios::binary);
        file << "data";
    }

    FakeSink sink;
    const interfaces::FileSinkResult up =
        RunUploadWithProgress(nullptr, "Import", sink, GUIUtil::qstringToBoostPath(dir.path()) / "up.bin");

    QVERIFY(!up.ok);
    QVERIFY(!up.error.empty());
    QVERIFY(sink.m_received.empty());
    QCOMPARE(EntriesIn(dir), 1);
}

// The download runs on the worker while the dialog shows its title and the bytes done of the
// total, and the file arrives whole.
void FileTransferDialogTests::downloadShowsProgressAndSavesTheFile()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::Modal);

    // Asked first, so that the worker's thread is running before the transfer
    // starts: an id is unique only among threads alive at the same time.
    std::thread::id worker_thread;
    const std::future<void> asked = worker.Post([&worker_thread] { worker_thread = std::this_thread::get_id(); });
    QVERIFY(asked.valid() && asked.wait_for(10s) == std::future_status::ready);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const fs::path dest = GUIUtil::qstringToBoostPath(dir.path()) / "backup.dat";

    GatedSource src(Bytes(4 * GatedSource::CHUNK), /*gate_on_read=*/2);
    const QString total = QString::number(4 * GatedSource::CHUNK);
    QString seen_label;
    QString seen_title;

    QTimer observer;
    connect(&observer, &QTimer::timeout, this, [&] {
        QProgressDialog* dialog = VisibleProgressDialog();
        if (dialog && src.Waiting() && dialog->labelText().contains(total)) {
            seen_label = dialog->labelText();
            seen_title = dialog->windowTitle();
            src.Release();
        }
    });
    observer.start(5);

    // So a regression fails rather than hangs.
    QTimer backstop;
    backstop.setSingleShot(true);
    connect(&backstop, &QTimer::timeout, this, [&src] { src.Release(); });
    backstop.start(10000);

    const interfaces::TransferOutcome out = RunDownloadWithProgress(nullptr, "Backup Wallet", src, 0, dest);
    observer.stop();
    backstop.stop();

    QVERIFY2(out.status == interfaces::TransferStatus::Ok, out.message.c_str());
    QVERIFY(out.bytes == 4 * GatedSource::CHUNK);
    QVERIFY(out.sha256 == Sha256Of(src.Data()));
    QVERIFY(ReadAll(dest) == std::string(src.Data().begin(), src.Data().end()));
    QCOMPARE(EntriesIn(dir), 1);

    QCOMPARE(seen_title, QString("Backup Wallet"));
    QVERIFY2(seen_label.contains(QString("%1 of %2 bytes").arg(GatedSource::CHUNK).arg(total)), qPrintable(seen_label));
    QVERIFY(!VisibleProgressDialog());

    // Every read ran on the modal worker's thread.
    const std::vector<std::thread::id> threads = src.ReadThreads();
    QVERIFY(!threads.empty());
    QVERIFY(threads[0] != std::this_thread::get_id());
    QVERIFY(std::all_of(threads.begin(), threads.end(), [&](const std::thread::id& id) { return id == worker_thread; }));
}

// Cancel keeps the dialog up, saying so with Cancel disabled, until the transfer has stopped; then
// nothing is left at the destination, and the source was told to stop.
void FileTransferDialogTests::cancelStopsTheDownloadAndLeavesNothing()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::Modal);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const fs::path dest = GUIUtil::qstringToBoostPath(dir.path()) / "backup.dat";

    GatedSource src(Bytes(4 * GatedSource::CHUNK), /*gate_on_read=*/2);
    bool clicked = false;
    bool saw_cancelling = false;
    bool cancel_disabled = false;

    QTimer observer;
    connect(&observer, &QTimer::timeout, this, [&] {
        QProgressDialog* dialog = VisibleProgressDialog();
        if (!dialog) return;

        auto* button = dialog->findChild<QPushButton*>();

        if (!clicked && src.Waiting() && button) {
            button->click();
            clicked = true;
            return;
        }

        // Shown again after the cancel hid it, while the read still waits.
        if (clicked && src.Waiting() && dialog->labelText() == CANCELLING_TEXT) {
            saw_cancelling = true;
            cancel_disabled = button && !button->isEnabled();
            src.Release();
        }
    });
    observer.start(5);

    QTimer backstop;
    backstop.setSingleShot(true);
    connect(&backstop, &QTimer::timeout, this, [&src] { src.Release(); });
    backstop.start(10000);

    const interfaces::TransferOutcome out = RunDownloadWithProgress(nullptr, "Backup Wallet", src, 0, dest);
    observer.stop();
    backstop.stop();

    QVERIFY(clicked);
    QVERIFY(saw_cancelling);
    QVERIFY(cancel_disabled);
    QVERIFY(out.status == interfaces::TransferStatus::Cancelled);
    QVERIFY(src.Cancelled());
    QVERIFY(!fs::exists(dest));
    QCOMPARE(EntriesIn(dir), 0);
    QVERIFY(!VisibleProgressDialog());
}

void FileTransferDialogTests::uploadSendsTheFile()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::Modal);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const fs::path local = GUIUtil::qstringToBoostPath(dir.path()) / "import.txt";
    const std::vector<unsigned char> data = Bytes(100'000);

    {
        std::ofstream file(local.string(), std::ios::binary);
        file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }

    FakeSink sink;
    const interfaces::FileSinkResult r = RunUploadWithProgress(nullptr, "Import", sink, local);

    QVERIFY2(r.ok, r.error.c_str());
    QVERIFY(r.output == "imported");
    QVERIFY(sink.m_received == data);
    QVERIFY(sink.m_finish_size == 100'000);
    QVERIFY(sink.m_finish_sha == Sha256Of(data));
    QVERIFY(!sink.m_cancelled);
}

// A job that is more than one transfer (a console file command) runs on the modal worker. Until it
// reports bytes the dialog shows what it does alone, then the bytes as a transfer's do; Cancel sets
// the job's flag and the dialog waits for the job to return. A job that throws is reported.
void FileTransferDialogTests::jobShowsItsBytesOnceReportedAndIsToldOfCancel()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::Modal);

    std::thread::id worker_thread;
    const std::future<void> asked = worker.Post([&worker_thread] { worker_thread = std::this_thread::get_id(); });
    QVERIFY(asked.valid() && asked.wait_for(10s) == std::future_status::ready);

    const QString what = "dumpcontracts to /backups/c.txt";
    std::atomic<int> stage{0};
    std::atomic<bool> go_on{false};
    std::atomic<bool> saw_cancel{false};
    std::thread::id job_thread;
    QString label_before;
    QString label_after;
    bool clicked = false;
    QElapsedTimer shown;

    // Waits until the test lets it go on, 10 seconds at most.
    const auto wait_for_go = [&go_on] {
        for (int i = 0; i < 1000 && !go_on.exchange(false); ++i) {
            std::this_thread::sleep_for(10ms);
        }
    };

    QTimer observer;
    connect(&observer, &QTimer::timeout, this, [&] {
        QProgressDialog* dialog = VisibleProgressDialog();
        if (!dialog) return;

        // Stage 1: the job waits before it reports anything. Read once the dialog has had time for
        // several of its own updates.
        if (stage == 1 && label_before.isEmpty()) {
            if (!shown.isValid()) shown.start();
            if (shown.elapsed() >= 300) {
                label_before = dialog->labelText();
                go_on = true;
            }
        }

        // Stage 2: the job has reported 5 of 10 bytes and waits again.
        if (stage == 2 && dialog->labelText().contains("5 of 10")) {
            label_after = dialog->labelText();
            if (!clicked) {
                if (auto* button = dialog->findChild<QPushButton*>()) {
                    button->click();
                    clicked = true;
                }
            }
        }
    });
    observer.start(5);

    QTimer backstop;
    backstop.setSingleShot(true);
    connect(&backstop, &QTimer::timeout, this, [&go_on] { go_on = true; });
    backstop.start(10000);

    std::string error;
    const bool ran = RunJobWithProgress(
        nullptr, "dumpcontracts", what,
        [&](const interfaces::ProgressFn& progress, const std::atomic<bool>& cancel) {
            job_thread = std::this_thread::get_id();
            stage = 1;
            wait_for_go();
            progress(5, 10);
            stage = 2;
            for (int i = 0; i < 1000 && !cancel; ++i) {
                std::this_thread::sleep_for(10ms);
            }
            saw_cancel = cancel.load();
        },
        error);
    observer.stop();
    backstop.stop();

    QVERIFY2(ran, error.c_str());
    QVERIFY(job_thread == worker_thread);
    QCOMPARE(label_before, what);
    QCOMPARE(label_after, what + "\n" + "5 of 10 bytes");
    QVERIFY(clicked);
    QVERIFY(saw_cancel);
    QVERIFY(!VisibleProgressDialog());

    const bool threw = RunJobWithProgress(
        nullptr, "dumpcontracts", what,
        [](const interfaces::ProgressFn&, const std::atomic<bool>&) { throw std::runtime_error("connection lost"); },
        error);
    QVERIFY(!threw);
    QCOMPARE(error, std::string("connection lost"));
}

// The backup's begin, in which the core copies the wallet, runs on the modal worker under the
// dialog, which says "Preparing the backup..." while the GUI thread keeps running its event loop.
// The entry is saved after it, and the source is destroyed on the worker too.
void FileTransferDialogTests::backupBeginsOnTheWorkerWhileTheWindowRuns()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::Modal);

    std::thread::id worker_thread;
    const std::future<void> asked = worker.Post([&worker_thread] { worker_thread = std::this_thread::get_id(); });
    QVERIFY(asked.valid() && asked.wait_for(10s) == std::future_status::ready);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const fs::path dest = GUIUtil::qstringToBoostPath(dir.path()) / "wallet.dat";

    SlowBegin begin(Bytes(3 * GatedSource::CHUNK + 5));
    int ticks_while_copying = 0;
    QString label_while_copying;

    QTimer observer;
    connect(&observer, &QTimer::timeout, this, [&] {
        if (!begin.Copying()) return;
        ++ticks_while_copying;
        if (QProgressDialog* dialog = VisibleProgressDialog()) label_while_copying = dialog->labelText();
        if (ticks_while_copying >= 5 && !label_while_copying.isEmpty()) begin.Release();
    });
    observer.start(5);

    std::vector<interfaces::BackupEntryResult> results;
    std::string error;
    const bool begun = RunBackupWithProgress(nullptr, "Backup Wallet", begin.Fn(), {dest}, results, error);
    observer.stop();
    begin.Release();

    QVERIFY2(begun, error.c_str());
    QVERIFY(begin.Thread() == worker_thread);
    QVERIFY2(ticks_while_copying >= 5, "the GUI thread's event loop did not run while the core copied the wallet");
    QCOMPARE(label_while_copying, QString("Preparing the backup..."));
    QCOMPARE(results.size(), size_t{1});
    QVERIFY2(results[0].outcome.status == interfaces::TransferStatus::Ok, results[0].outcome.message.c_str());
    QVERIFY(ReadAll(dest) == std::string(begin.Data().begin(), begin.Data().end()));
    QVERIFY(begin.DestroyedOn() == worker_thread);
    QVERIFY(!VisibleProgressDialog());
}

// Cancel while the core makes its copies takes effect when the begin returns: the dialog says
// "Cancelling..." meanwhile, the source is never read and nothing is written to the destination,
// and the source is destroyed, which deletes the core's copies.
void FileTransferDialogTests::cancelWhileTheBackupBeginsSavesNothing()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::Modal);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const fs::path dest = GUIUtil::qstringToBoostPath(dir.path()) / "wallet.dat";

    SlowBegin begin(Bytes(1000));
    bool clicked = false;
    bool saw_cancelling = false;

    QTimer observer;
    connect(&observer, &QTimer::timeout, this, [&] {
        if (!begin.Copying()) return;
        QProgressDialog* dialog = VisibleProgressDialog();
        if (!dialog) return;

        if (!clicked) {
            if (auto* button = dialog->findChild<QPushButton*>()) {
                button->click();
                clicked = true;
            }
            return;
        }

        if (dialog->labelText() == CANCELLING_TEXT) {
            saw_cancelling = true;
            begin.Release();
        }
    });
    observer.start(5);

    std::vector<interfaces::BackupEntryResult> results;
    std::string error;
    const bool begun = RunBackupWithProgress(nullptr, "Backup Wallet", begin.Fn(), {dest}, results, error);
    observer.stop();
    begin.Release();

    QVERIFY(clicked);
    QVERIFY(saw_cancelling);
    QVERIFY2(begun, error.c_str());
    QCOMPARE(results.size(), size_t{1});
    QVERIFY(results[0].outcome.status == interfaces::TransferStatus::Cancelled);
    QCOMPARE(begin.Reads(), 0);
    QVERIFY(begin.DestroyedOn() != std::thread::id{});
    QVERIFY(!fs::exists(dest));
    QCOMPARE(EntriesIn(dir), 0);
    QVERIFY(!VisibleProgressDialog());
}

void FileTransferDialogTests::guardFilesComeFromTheCorePaths()
{
    interfaces::CorePaths paths;
    paths.datadir = "/core";
    paths.wallet_file = "/core/wallet.dat";
    paths.config_file = "/etc/gridcoinresearch.conf";
    paths.settings_file = "/core/gridcoinsettings.json";
    paths.debug_log = "/core/debug.log";

    std::vector<fs::path> files = BackupGuardFiles(paths);
    QVERIFY(files == (std::vector<fs::path>{"/core/wallet.dat", "/etc/gridcoinresearch.conf",
                                            "/core/gridcoinsettings.json", "/core/debug.log"}));

    // A path the core did not report (-nosettings, no wallet loaded) is left out.
    paths.wallet_file.clear();
    paths.settings_file.clear();
    files = BackupGuardFiles(paths);
    QVERIFY(files == (std::vector<fs::path>{"/etc/gridcoinresearch.conf", "/core/debug.log"}));
}

void FileTransferDialogTests::backupResultText()
{
    interfaces::BackupEntryResult wallet;
    wallet.name = "wallet.dat";
    wallet.dest = "/backups/wallet.dat";
    wallet.outcome.status = interfaces::TransferStatus::Ok;
    wallet.outcome.bytes = 123456;

    interfaces::BackupEntryResult conf;
    conf.name = "gridcoinresearch.conf";
    conf.dest = "/backups/gridcoinresearch.conf";
    conf.skipped = true;
    conf.note = "no configuration file";

    interfaces::BackupEntryResult settings;
    settings.name = "gridcoinsettings.json";
    settings.dest = "/backups/gridcoinsettings.json";
    settings.outcome.status = interfaces::TransferStatus::NoSpace;
    settings.outcome.message = "not enough free space in /backups";

    bool failed = true;
    QCOMPARE(BackupResultText({wallet}, failed), QString("Saved 123456 bytes to /backups/wallet.dat."));
    QVERIFY(!failed);

    QCOMPARE(BackupResultText({wallet, conf}, failed),
             QString("Saved 123456 bytes to /backups/wallet.dat.\n"
                     "gridcoinresearch.conf was not backed up: no configuration file"));
    QVERIFY(!failed);

    QCOMPARE(BackupResultText({wallet, settings}, failed),
             QString("Saved 123456 bytes to /backups/wallet.dat.\n"
                     "gridcoinsettings.json was not saved: not enough free space in /backups"));
    QVERIFY(failed);

    interfaces::BackupEntryResult cancelled = settings;
    cancelled.outcome.status = interfaces::TransferStatus::Cancelled;
    cancelled.outcome.message = "cancelled";
    QCOMPARE(BackupResultText({wallet, cancelled}, failed),
             QString("Saved 123456 bytes to /backups/wallet.dat.\n"
                     "The backup was cancelled; nothing was saved to /backups/gridcoinsettings.json."));
    QVERIFY(!failed);

    // A source that could not list its entries.
    interfaces::BackupEntryResult lost;
    lost.outcome.status = interfaces::TransferStatus::SourceError;
    lost.outcome.message = "IPC client method call interrupted by disconnect.";
    QCOMPARE(BackupResultText({lost}, failed), QString("IPC client method call interrupted by disconnect."));
    QVERIFY(failed);
}

// The application quitting while a transfer runs cancels it, and RunDownloadWithProgress returns
// only once it has stopped, with events still processed meanwhile.
void FileTransferDialogTests::loopExitCancelsAndWaitsForTheTransfer()
{
    GuiIpcWorker worker(GuiIpcWorker::Role::Modal);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const fs::path dest = GUIUtil::qstringToBoostPath(dir.path()) / "backup.dat";

    GatedSource src(Bytes(4 * GatedSource::CHUNK), /*gate_on_read=*/2);
    bool exited = false;
    int ticks_after_exit = 0;

    QTimer observer;
    connect(&observer, &QTimer::timeout, this, [&] {
        if (!exited) {
            if (src.Waiting()) {
                QCoreApplication::exit(0);
                exited = true;
            }
            return;
        }

        if (++ticks_after_exit >= 3) src.Release();
    });
    observer.start(5);

    QTimer backstop;
    backstop.setSingleShot(true);
    connect(&backstop, &QTimer::timeout, this, [&src] { src.Release(); });
    backstop.start(10000);

    const interfaces::TransferOutcome out = RunDownloadWithProgress(nullptr, "Backup Wallet", src, 0, dest);
    observer.stop();
    backstop.stop();

    // Before any check can return early.
    ClearApplicationQuit();

    QVERIFY(exited);
    QVERIFY2(ticks_after_exit >= 3, qPrintable(QString("ticks after exit: %1").arg(ticks_after_exit)));
    QVERIFY(out.status == interfaces::TransferStatus::Cancelled);
    QVERIFY(src.Cancelled());
    QVERIFY(!fs::exists(dest));
    QCOMPARE(EntriesIn(dir), 0);
    QVERIFY(!VisibleProgressDialog());
}
