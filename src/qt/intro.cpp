// Copyright (c) 2011-2020 The Bitcoin Core developers
// Copyright (c) 2021 The Gridcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/gridcoin-config.h>
#endif

#include <chainparams.h>
#include <fs.h>
#include <qt/intro.h>
#include <qt/forms/ui_intro.h>

#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>

//#include <interfaces/node.h>
#include <util.h>
#include <util/dir_permissions.h>

#include <QFileDialog>
#include <QSettings>
#include <QMessageBox>

#include <cmath>

/* Check free space asynchronously to prevent hanging the UI thread.

   Up to one request to check a path is in flight to this thread; when the check()
   function runs, the current path is requested from the associated Intro object.
   The reply is sent back through a signal.

   This ensures that no queue of checking requests is built up while the user is
   still entering the path, and that always the most recently entered path is checked as
   soon as the thread becomes available.
*/
class FreespaceChecker : public QObject
{
    Q_OBJECT

public:
    explicit FreespaceChecker(Intro *intro);

    enum Status {
        ST_OK,
        ST_ERROR
    };

public Q_SLOTS:
    void check();

Q_SIGNALS:
    void reply(int status, const QString &message, quint64 available);

private:
    Intro *intro;
};

#include <qt/intro.moc>

FreespaceChecker::FreespaceChecker(Intro *_intro)
{
    this->intro = _intro;
}

void FreespaceChecker::check()
{
    QString dataDirStr = intro->getPathToCheck();
    fs::path dataDir = GUIUtil::qstringToBoostPath(dataDirStr);
    uint64_t freeBytesAvailable = 0;
    int replyStatus = ST_OK;
    QString replyMessage = tr("A new data directory will be created.");

    /* Find first parent that exists, so that fs::space does not fail */
    fs::path parentDir = dataDir;
    fs::path parentDirOld = fs::path();
    while(parentDir.has_parent_path() && !fs::exists(parentDir))
    {
        parentDir = parentDir.parent_path();

        /* Check if we make any progress, break if not to prevent an infinite loop here */
        if (parentDirOld == parentDir)
            break;

        parentDirOld = parentDir;
    }

    try {
        freeBytesAvailable = fs::space(parentDir).available;
        if(fs::exists(dataDir))
        {
            if(fs::is_directory(dataDir))
            {
                replyStatus = ST_OK;
                replyMessage = tr("Directory already exists. If this directory contains valid data, it will be used.");
            } else {
                replyStatus = ST_ERROR;
                replyMessage = tr("Path already exists, and is not a directory.");
            }
        }

        // A directory on tmpfs or ramfs takes every write and loses the lot when
        // the program exits. Report it as an error so OK stays disabled. This is
        // worth surfacing here rather than only at startup because the free-space
        // figure shown just above is entirely healthy-looking on tmpfs, which is
        // most of why the sandbox case was so hard to diagnose from the outside.
        if (replyStatus == ST_OK && fsbridge::IsVolatileFilesystem(fs::exists(dataDir) ? dataDir : parentDir))
        {
            replyStatus = ST_ERROR;
            replyMessage = tr("This location is on a temporary filesystem. Anything stored here, "
                              "including the wallet, is lost when the program exits.");
        }
    } catch (const fs::filesystem_error&)
    {
        /* Parent directory does not exist or is not accessible */
        replyStatus = ST_ERROR;
        replyMessage = tr("Cannot create data directory here.");
    }
    Q_EMIT reply(replyStatus, replyMessage, freeBytesAvailable);
}

Intro::Intro(QWidget *parent, int64_t blockchain_size_gb) :
    QDialog(parent),
    ui(new Ui::Intro),
    thread(nullptr),
    signalled(false),
    m_blockchain_size_gb(blockchain_size_gb)
{

    ui->setupUi(this);

    ui->welcomeLabel->setText(ui->welcomeLabel->text().arg(PACKAGE_NAME));
    ui->storageLabel->setText(ui->storageLabel->text().arg(PACKAGE_NAME));

    ui->lblExplanation1->setText(ui->lblExplanation1->text()
        .arg(PACKAGE_NAME)
        .arg(m_blockchain_size_gb)
        .arg(2014)
        .arg(tr("Gridcoin"))
    );

    ui->lblExplanation2->setText(ui->lblExplanation2->text().arg(PACKAGE_NAME));

    // This is necessary to deal with various dpi displays.
    this->adjustSize();

    startThread();
}

Intro::~Intro()
{
    delete ui;
    /* Ensure thread is finished before it is deleted */
    thread->quit();
    thread->wait();
}

QString Intro::getDataDirectory()
{
    return ui->dataDirectory->text();
}

void Intro::setDataDirectory(const QString &dataDir)
{
    ui->dataDirectory->setText(dataDir);
    if(dataDir == GUIUtil::getDefaultDataDirectory())
    {
        ui->dataDirDefault->setChecked(true);
        ui->dataDirectory->setEnabled(false);
        ui->ellipsisButton->setEnabled(false);
    } else {
        ui->dataDirCustom->setChecked(true);
        ui->dataDirectory->setEnabled(true);
        ui->ellipsisButton->setEnabled(true);
    }
}

bool Intro::showIfNeeded(bool& did_show_intro)
{
    QSettings settings;
    // If data directory provided on command line AND -choosedatadir is not specified AND the directory exists,
    // no need to look at settings or show a picking dialog. The -choosedatadir test is required because
    // showIfNeeded is called after the initialization of the optionsModel, which will populate the datadir
    // from the GUI settings if present. Without it, you would then not be able to get the chooser dialog to
    // come up again once a datadir is stored in the GUI settings config file.
    //
    // If the stored path does not exist (e.g. moved, deleted, or corrupted by a Unicode encoding issue),
    // fall through to the directory chooser dialog so the user can reselect.
    std::string datadir_arg = gArgs.GetArg("-datadir", "");
    // Use the non-throwing fs::exists() overload. A path corrupted by the Unicode encoding bug
    // (see #2736) may be ill-formed and cause boost::filesystem::filesystem_error. Treating
    // errors as "does not exist" lets us fall through to the directory chooser safely.
    boost::system::error_code ec;
    if (!datadir_arg.empty()
            && !gArgs.GetBoolArg("-choosedatadir", DEFAULT_CHOOSE_DATADIR)
            && fs::exists(datadir_arg, ec)) {
        return true;
    }
    /* 1) Default data directory for operating system */
    QString dataDir = GUIUtil::getDefaultDataDirectory();
    /* 2) Allow QSettings to override default dir */
    dataDir = settings.value("dataDir", dataDir).toString();

    // This is necessary to handle the case where a user has changed the name of the data directory from a non-default
    // back to the default, and wants to use the chooser via -choosedatadir to rechoose the data directory and have it
    // properly respected without having to restart after the choosing.
    bool originally_not_default_datadir = (dataDir != GUIUtil::getDefaultDataDirectory());

    // Non-throwing overload here for the same reason as the -datadir check above:
    // this call is not inside the try block that starts below, and its caller in
    // bitcoin.cpp does not catch either, so a path that is ill-formed or whose
    // parent denies access would terminate the process with no dialog at all --
    // the exact outcome the comment twelve lines up exists to prevent.
    if (!fs::exists(GUIUtil::qstringToBoostPath(dataDir), ec)
            || gArgs.GetBoolArg("-choosedatadir", DEFAULT_CHOOSE_DATADIR)
            || settings.value("fReset", false).toBool()
            || gArgs.GetBoolArg("-resetguisettings", false))
    {
        /* Use selectParams here to guarantee Params() can be used by node interface when we implement it from
           Bitcoin. */
        try {
            SelectParams(gArgs.GetChainName());
        } catch (const std::exception&) {
            return false;
        }

        /* If current default data directory does not exist, let the user choose one */
        Intro intro(nullptr, Params().AssumedBlockchainSize());
        intro.setDataDirectory(dataDir);
        intro.setWindowIcon(QIcon(":/images/gridcoin"));
        did_show_intro = true;

        while (true)
        {
            if (!intro.exec())
            {
                /* Cancel clicked */
                return false;
            }
            dataDir = intro.getDataDirectory();
            try {
                // Create it restricted to this account (0700 / owner+SYSTEM DACL).
                // This is the monolithic build's creation point; the daemon has its
                // own in GetDataDirPath(), and both go through the same helper so a
                // data directory is protected no matter which one made it.
                // Create-only: an existing directory is left as the user set it up.
                std::string perm_error;
                if (!util::CreateOwnerOnlyDirectory(GUIUtil::qstringToBoostPath(dataDir), perm_error)) {
                    // Fatal to this choice, both of the two ways it can fail.
                    //
                    // An earlier revision downgraded "created but could not be locked
                    // down" to a warning and kept the selection. That was the worse
                    // half of the two: it put the wallet in a directory already known
                    // to be readable by other accounts, and because the helper leaves
                    // a pre-existing directory alone, re-selecting the same path on
                    // the next run would succeed silently and never warn again. The
                    // helper now withdraws a directory it could not protect, so
                    // returning the user to the chooser is a real retry rather than a
                    // loop that passes the second time for the wrong reason.
                    QMessageBox::critical(nullptr, PACKAGE_NAME,
                        tr("Error: The data directory \"%1\" could not be created and secured.\n\n%2\n\n"
                           "Please choose a different location.")
                            .arg(dataDir, QString::fromStdString(perm_error)));
                    continue; /* back to the choosing screen */
                }
                break;
            } catch (const fs::filesystem_error&) {
                QMessageBox::critical(nullptr, PACKAGE_NAME,
                    tr("Error: Specified data directory \"%1\" cannot be created.").arg(dataDir));
                /* fall through, back to choosing screen */
            }
        }

        settings.setValue("dataDir", dataDir);
        settings.setValue("fReset", false);
    }
    /* Only override -datadir if different from the default, to make it possible to
     * override -datadir in the bitcoin.conf file in the default data directory
     * (to be consistent with bitcoind behavior)
     */
    if (dataDir != GUIUtil::getDefaultDataDirectory() || originally_not_default_datadir) {
        // This must be a ForceSetArg, because the optionsModel has already loaded the datadir argument if it exists in
        // the Qt settings file prior to this.
        // Use ShortPathString to get an 8.3 short path on Windows when the path contains characters
        // outside the system code page. gArgs is a narrow-string store and fs::path::string() would
        // corrupt non-codepage characters via code page narrowing.
        gArgs.ForceSetArg("-datadir", fsbridge::ShortPathString(GUIUtil::qstringToBoostPath(dataDir)));
    }

    return true;
}

void Intro::setStatus(int status, const QString &message, quint64 bytesAvailable)
{
    switch(status)
    {
    case FreespaceChecker::ST_OK:
        ui->errorMessage->setText(message);
        ui->errorMessage->setStyleSheet("");
        break;
    case FreespaceChecker::ST_ERROR:
        ui->errorMessage->setText(tr("Error") + ": " + message);
        ui->errorMessage->setStyleSheet("QLabel { color: #800000 }");
        break;
    }
    /* Indicate number of bytes available */
    if(status == FreespaceChecker::ST_ERROR)
    {
        ui->freeSpace->setText("");
    } else {
        m_bytes_available = bytesAvailable;
        UpdateFreeSpaceLabel();
    }
    /* Don't allow confirm in ERROR state */
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(status != FreespaceChecker::ST_ERROR);
}

void Intro::UpdateFreeSpaceLabel()
{
    QString freeString = tr("%n GB of free space available", "", m_bytes_available / GB_BYTES);
    if (m_bytes_available < m_required_space_gb * GB_BYTES) {
        freeString += " " + tr("(of %n GB needed)", "", m_required_space_gb);
        ui->freeSpace->setStyleSheet("QLabel { color: #800000 }");
    } else if (m_bytes_available / GB_BYTES - m_required_space_gb < 10) {
        freeString += " " + tr("(%n GB needed for full chain)", "", m_required_space_gb);
        ui->freeSpace->setStyleSheet("QLabel { color: #999900 }");
    } else {
        ui->freeSpace->setStyleSheet("");
    }
    ui->freeSpace->setText(freeString + ".");
}

void Intro::on_dataDirectory_textChanged(const QString &dataDirStr)
{
    /* Disable OK button until check result comes in */
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    checkPath(dataDirStr);
}

void Intro::on_ellipsisButton_clicked()
{
    QString dir = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(nullptr, "Choose data directory", ui->dataDirectory->text()));
    if(!dir.isEmpty())
        ui->dataDirectory->setText(dir);
}

void Intro::on_dataDirDefault_clicked()
{
    setDataDirectory(GUIUtil::getDefaultDataDirectory());
}

void Intro::on_dataDirCustom_clicked()
{
    ui->dataDirectory->setEnabled(true);
    ui->ellipsisButton->setEnabled(true);
}

void Intro::startThread()
{
    thread = new QThread(this);
    FreespaceChecker *executor = new FreespaceChecker(this);
    executor->moveToThread(thread);

    connect(executor, &FreespaceChecker::reply, this, &Intro::setStatus);
    connect(this, &Intro::requestCheck, executor, &FreespaceChecker::check);
    // make sure executor object is deleted in its own thread
    connect(thread, &QThread::finished, executor, &QObject::deleteLater);

    thread->start();
}

void Intro::checkPath(const QString &dataDir)
{
    mutex.lock();
    pathToCheck = dataDir;
    if(!signalled)
    {
        signalled = true;
        Q_EMIT requestCheck();
    }
    mutex.unlock();
}

QString Intro::getPathToCheck()
{
    QString retval;
    mutex.lock();
    retval = pathToCheck;
    signalled = false; /* new request can be queued now */
    mutex.unlock();
    return retval;
}
