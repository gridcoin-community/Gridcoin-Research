// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/filetransferdialogtests.h"

#include "crypto/sha256.h"
#include "fs.h"
#include "interfaces/file_transfer.h"
#include "interfaces/node.h"
#include "qt/filetransferdialog.h"
#include "qt/guiutil.h"
#include "sync.h"
#include "uint256.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
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

// Every job runs on one thread, not the GUI's, in the order posted, and one that throws does not
// end that thread.
void FileTransferDialogTests::workerRunsJobsInOrderOnOneThread()
{
    const std::thread::id gui_thread = std::this_thread::get_id();
    QVERIFY(FileTransferWorker::Current() == nullptr);

    {
        FileTransferWorker worker;
        QVERIFY(FileTransferWorker::Current() == &worker);

        Mutex mutex;
        std::vector<int> order;
        std::vector<std::thread::id> threads;
        std::promise<void> last;
        std::future<void> done = last.get_future();

        for (int i = 0; i < 3; ++i) {
            QVERIFY(worker.Post([&, i] {
                LOCK(mutex);
                order.push_back(i);
                threads.push_back(std::this_thread::get_id());
            }));
        }

        QVERIFY(worker.Post([] { throw std::runtime_error("a job that throws"); }));
        QVERIFY(worker.Post([&] {
            {
                LOCK(mutex);
                threads.push_back(std::this_thread::get_id());
            }
            last.set_value();
        }));

        QVERIFY(done.wait_for(10s) == std::future_status::ready);

        LOCK(mutex);
        QVERIFY(order == (std::vector<int>{0, 1, 2}));
        QVERIFY(threads.size() == 4);
        QVERIFY(threads[0] != gui_thread);
        QVERIFY(std::all_of(threads.begin(), threads.end(), [&](const std::thread::id& id) { return id == threads[0]; }));
    }

    QVERIFY(FileTransferWorker::Current() == nullptr);
}

// The worker is joined by its destructor, which runs the jobs still queued first.
void FileTransferDialogTests::workerRunsQueuedJobsBeforeItGoes()
{
    std::atomic<int> ran{0};

    {
        FileTransferWorker worker;
        QVERIFY(worker.Post([] { std::this_thread::sleep_for(50ms); }));

        for (int i = 0; i < 5; ++i) {
            QVERIFY(worker.Post([&ran] { ++ran; }));
        }
    }

    QCOMPARE(ran.load(), 5);
}

// Outside a worker's lifetime nothing is read or written.
void FileTransferDialogTests::downloadWithoutWorkerIsALocalError()
{
    QVERIFY(FileTransferWorker::Current() == nullptr);

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
    FileTransferWorker worker;
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

    const std::vector<std::thread::id> threads = src.ReadThreads();
    QVERIFY(!threads.empty());
    QVERIFY(threads[0] != std::this_thread::get_id());
    QVERIFY(std::all_of(threads.begin(), threads.end(), [&](const std::thread::id& id) { return id == threads[0]; }));
}

// Cancel keeps the dialog up, saying so with Cancel disabled, until the transfer has stopped; then
// nothing is left at the destination, and the source was told to stop.
void FileTransferDialogTests::cancelStopsTheDownloadAndLeavesNothing()
{
    FileTransferWorker worker;
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
    FileTransferWorker worker;
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
                     "not enough free space in /backups"));
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
    FileTransferWorker worker;
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
