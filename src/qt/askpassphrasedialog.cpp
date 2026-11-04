#include "askpassphrasedialog.h"
#include "ui_askpassphrasedialog.h"

#include "bitcoingui.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "qt/decoration.h"
#include "qt/guiipcworker.h"
#include "util.h" /* for LogPrintf */
#include "walletmodel.h"

#include <QApplication>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QKeyEvent>

#include <atomic>
#include <exception>
#include <memory>
#include <string>
#include <utility>

struct AskPassphraseDialog::CheckState
{
    //! Set by reject() or the destructor to cancel the check
    //! (PassphraseAttemptCancel).
    std::atomic<bool> cancel{false};

    //! The passphrase was accepted. Written by the check before it reports.
    std::atomic<bool> accepted{false};

    //! Whoever moves this out of QUEUED owns what follows: the check, which
    //! then runs and reports, or the prompt, which then reports for a check
    //! that never ran. A check given up touches nothing of the prompt's when
    //! its turn comes, so the prompt may be gone by then.
    static constexpr int QUEUED{0};
    static constexpr int RUNNING{1};
    static constexpr int GIVEN_UP{2};
    std::atomic<int> phase{QUEUED};

    //! Called by the check when its turn comes. False if it was given up.
    bool Start()
    {
        int expected = QUEUED;
        return phase.compare_exchange_strong(expected, RUNNING);
    }

    //! Called by the prompt. False if the check has already started.
    bool GiveUp()
    {
        int expected = QUEUED;
        return phase.compare_exchange_strong(expected, GIVEN_UP);
    }
};

AskPassphraseDialog::AskPassphraseDialog(Mode mode, QWidget* parent)
                 : QDialog(parent)
                 , ui(new Ui::AskPassphraseDialog)
                 , mode(mode)
                 , model(nullptr)
                 , fCapsLock(false)
{
    ui->setupUi(this);

    resize(GRC::ScaleSize(this, width(), height()));

    ui->oldPassphraseEdit->setMaxLength(MAX_PASSPHRASE_SIZE);
    ui->newPassphraseEdit->setMaxLength(MAX_PASSPHRASE_SIZE);
    ui->repeatNewPassphraseEdit->setMaxLength(MAX_PASSPHRASE_SIZE);

    // Setup Caps Lock detection.
    ui->oldPassphraseEdit->installEventFilter(this);
    ui->newPassphraseEdit->installEventFilter(this);
    ui->repeatNewPassphraseEdit->installEventFilter(this);

    switch(mode)
    {
        case Encrypt: // Ask passphrase x2
            ui->oldPassphraseLabel->hide();
            ui->oldPassphraseEdit->hide();
            ui->warningLabel->setText(tr("Enter the new passphrase to the wallet.<br/>Please use a passphrase of <b>ten or more random characters</b>, or <b>eight or more words</b>."));
            setWindowTitle(tr("Encrypt wallet"));
            break;
        case UnlockStaking:
            ui->stakingCheckBox->setChecked(true);
            ui->stakingCheckBox->show();
            // fallthru
            [[fallthrough]];
        case Elevate: // Ask passphrase
            // Two different promises, so do not make one of them twice.
            //
            // UnlockStaking is the Unlock BUTTON: a directive that outlives this
            // dialog, and the wallet stays as chosen here until it is locked
            // again. Elevate authorises ONE operation and leaves the wallet as
            // it was. Saying "returns to its previous state" for both told a
            // user pressing Unlock that their unlock was temporary, which it is
            // not.
            ui->warningLabel->setText(mode == UnlockStaking
                ? tr("Enter your wallet passphrase to unlock the wallet. It stays unlocked until "
                     "you lock it again.")
                : tr("This operation needs your wallet passphrase. It authorises this operation "
                     "only, and the wallet is left as it was afterwards."));
            ui->newPassphraseLabel->hide();
            ui->newPassphraseEdit->hide();
            ui->repeatNewPassphraseLabel->hide();
            ui->repeatNewPassphraseEdit->hide();
            setWindowTitle(tr("Unlock wallet"));
            break;
        case ChangePass: // Ask old passphrase + new passphrase x2
            setWindowTitle(tr("Change passphrase"));
            ui->warningLabel->setText(tr("Enter the old and new passphrase to the wallet."));
            break;
    }

    textChanged();
    connect(ui->oldPassphraseEdit, &QLineEdit::textChanged, this, &AskPassphraseDialog::textChanged);
    connect(ui->newPassphraseEdit, &QLineEdit::textChanged, this, &AskPassphraseDialog::textChanged);
    connect(ui->repeatNewPassphraseEdit, &QLineEdit::textChanged, this, &AskPassphraseDialog::textChanged);
}

AskPassphraseDialog::~AskPassphraseDialog()
{
    // A check still running holds this dialog's address, so it must finish
    // first. The prompt stays open until its check reports, cancelled or not,
    // so that is reached only when the application quits under an open
    // prompt. Cancelled first, so that a check waiting for its turn in the
    // wallet gives up rather than holding the quit up. A check still waiting
    // for its turn on the worker never runs once given up, here or by a
    // Cancel whose report has not been delivered yet, so only one that is
    // running is waited for.
    if (m_check_in_flight) {
        m_check->cancel = true;

        if (!m_check->GiveUp() && m_check->phase == CheckState::RUNNING && m_check_done.valid()) {
            m_check_done.wait();
        }
    }

    // The check began a capability, and the report that would have handed it
    // to the waiting operation never ran. End it rather than leave it to its
    // deadline.
    if (!m_check_reported && m_check && m_check->accepted && mode == Elevate && model) {
        try {
            model->wallet().endElevation();
        } catch (const std::exception& e) {
            LogPrintf("WARN: %s: could not end an unclaimed capability: %s", __func__, e.what());
        }
    }

    secureClearPassFields();
    delete ui;
}

void AskPassphraseDialog::setModel(WalletModel *model)
{
    this->model = model;

    m_opened_staking_only = model && mode == Elevate
        && model->getEncryptionStatus() == WalletModel::UnlockedForStakingOnly;

    // The box is no longer seeded from a remembered preference. There is no such
    // preference any more -- the restriction belongs to the unlock -- and the
    // only mode that shows the box, UnlockStaking, ticks it in the constructor
    // because staking-only is the conservative default for a directive.
    //
    // Dropping the seeding also removes a lockout. The box is invisible in the
    // Elevate mode the operation-driven prompt uses, so a user whose last unlock
    // was staking-only used to get the hidden box pre-ticked, unlock
    // staking-only again, and fail the very operation that raised the prompt.
    // Do not restore "stickiness" without solving that.
}

namespace {
//! Balances setOverrideCursor/restoreOverrideCursor even if the call it wraps
//! throws.
//!
//! Its user, the encryption, crosses IPC in the split build, where a disconnect
//! mid-call raises, and an override cursor that is set and never restored
//! persists for the life of the application -- the whole GUI keeps a wait
//! cursor with nothing running.
struct BusyCursor {
    BusyCursor() { QApplication::setOverrideCursor(Qt::WaitCursor); }
    ~BusyCursor() { QApplication::restoreOverrideCursor(); }

    BusyCursor(const BusyCursor&) = delete;
    BusyCursor& operator=(const BusyCursor&) = delete;
};
} // namespace

void AskPassphraseDialog::accept()
{
    // The OK button is disabled while a check runs; this covers any other
    // route to accept().
    if (m_check_in_flight)
        return;

    if(!model)
        return;

    // No plain copy of a passphrase is left behind by the conversion, and each
    // one is in a secure buffer of its own whatever its length, so moving it
    // into the check below moves that buffer.
    SecureString oldpass = GUIUtil::SecureStringFromQString(ui->oldPassphraseEdit->text());
    SecureString newpass1 = GUIUtil::SecureStringFromQString(ui->newPassphraseEdit->text());
    SecureString newpass2 = GUIUtil::SecureStringFromQString(ui->repeatNewPassphraseEdit->text());

    secureClearPassFields();

    switch(mode)
    {
    case Encrypt: {
        if(newpass1.empty() || newpass2.empty())
        {
            // Cannot encrypt with empty passphrase
            break;
        }
        if(newpass1 != newpass2)
        {
            // Checked before the confirmation, while this dialog is still up:
            // the passphrase is retyped here, so the dialog has to stay. Asking
            // "are you sure" first and reporting the mismatch only afterwards
            // had the user confirm an encryption that could not happen.
            QMessageBox::critical(this, tr("Wallet encryption failed"),
                                 tr("The supplied passphrases do not match."));
            break;
        }

        // Take this dialog down BEFORE the confirmation, not after it. accept()
        // cleared the passphrase fields on entry, so leaving it up shows an
        // empty prompt behind the confirmation -- two prompts at once -- and
        // then an empty, frozen one for the seconds setWalletEncrypted() blocks
        // this thread rewriting the wallet database and regenerating the
        // keypool.
        //
        // hide(), not accept(): the message boxes below parent to this dialog,
        // and the accept() or reject() at the end of this branch is what ends
        // exec() with the right result.
        hide();

        QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm wallet encryption"),
                 tr("Warning: If you encrypt your wallet and lose your passphrase, you will <b>LOSE ALL OF YOUR COINS</b>!") + "<br><br>" + tr("Are you sure you wish to encrypt your wallet?"),
                 QMessageBox::Yes|QMessageBox::Cancel,
                 QMessageBox::Cancel);
        if(retval == QMessageBox::Yes) {
            // Nothing else on screen says the application is busy once the
            // dialog is gone, and the whole GUI thread is blocked here.
            bool encrypted = false;
            {
                BusyCursor busy;
                encrypted = model->setWalletEncrypted(newpass1);
            }

            if(encrypted) {
                // Earlier backups of the pre-encryption wallet are not merely useless
                // once the new wallet is in use -- they remain a security risk, because
                // they still contain the UNENCRYPTED private keys. This includes the
                // automatic backups written to the walletbackups folder. Shown in both
                // the monolithic and multiprocess cases (most users do not know this).
                const QString backupWarning = "<b>" +
                                              tr("IMPORTANT: Any earlier backups of your wallet file are not just "
                                                 "useless once you use the new encrypted wallet; they are a security "
                                                 "risk. They still contain your UNENCRYPTED private keys, so anyone who "
                                                 "obtains one can take your coins even after the live wallet is encrypted. "
                                                 "This includes the automatic backups Gridcoin writes to the wallet backups "
                                                 "directory (\"walletbackups\" by default, or the directory set by -backupdir). "
                                                 "After making a fresh backup of the new encrypted wallet, securely delete "
                                                 "every backup taken before encryption, including those in that directory.") +
                                              "</b>";

                if (!m_multiprocess) {
                    // Monolithic: node + wallet + GUI are one process, so restarting the
                    // application performs the clean reload that finishes the encryption.
                    QMessageBox::warning(this, tr("Wallet encrypted"),
                                         "<qt>" +
                                             tr("Gridcoin will close now to finish the encryption process. "
                                                "Remember that encrypting your wallet cannot fully protect "
                                                "your coins from being stolen by malware infecting your computer.") +
                                             "<br><br>" + backupWarning + "</qt>");
                    // The wallet must restart to finish encryption; route the
                    // shutdown through BitcoinGUI::requestQuit() so it can't be
                    // vetoed by minimize-on-close on Qt6 (issue #2995).
                    BitcoinGUI::requestQuit();
                } else {
                    // Multiprocess: the wallet lives in the separate core process. The
                    // core stops when this connection closes, to finish the encryption
                    // (as the RPC encryptwallet does), so the GUI quits after the user
                    // has read this. The core also stops by itself a while after the
                    // encryption if the connection is still open then, and the GUI asks
                    // for a quit when it sees the connection drop. That quit waits until
                    // the user has closed the message (QuitHold); a quit from a
                    // termination signal does not.
                    const BitcoinGUI::QuitHold hold;

                    QMessageBox::warning(this, tr("Wallet encrypted"),
                                         "<qt>" +
                                             tr("Your wallet is now encrypted and locked. The Gridcoin core stops "
                                                "to finish the encryption when you close this message, or after a "
                                                "minute if it is left open. If it runs as a service it starts again "
                                                "by itself; otherwise start it again, then reopen the wallet.") +
                                             "<br><br>" + backupWarning + "</qt>");
                    BitcoinGUI::requestQuit();
                }
            }
            else {
                QMessageBox::critical(this, tr("Wallet encryption failed"),
                                     tr("Wallet encryption failed due to an internal error. Your wallet was not encrypted."));
            }
            QDialog::accept(); // Success
        } else {
            QDialog::reject(); // Cancelled
        }
    } break;
    case UnlockStaking:
    case Elevate: {
        // Two different questions, so do not read one answer for both.
        //
        // UnlockStaking is the Unlock BUTTON: a directive that leaves the
        // wallet in a state the user chose and that outlives this dialog. The
        // checkbox is that choice, and it opens ticked because staking-only is
        // the conservative option.
        //
        // Elevate is raised by an operation that needs the wallet's keys. It
        // begins a capability for that operation's kind and nothing else: the
        // wallet is not widened, so a cancelled or mistyped prompt changes
        // nothing, and the deadline of a timed unlock is never touched. A
        // locked wallet is unlocked for staking only until the capability
        // ends, which WalletModel::UnlockContext does when the operation
        // finishes.
        //
        // Both run the check off the GUI thread; checkFinished() reports.
        startCheck(std::move(oldpass));
    } break;
    case ChangePass:
        if(newpass1 == newpass2)
        {
            // The same off-thread check as an unlock. The change waits its turn
            // behind other passphrase attempts, waits out a delay of up to 30 s
            // on a wrong old passphrase, and re-derives the key several times
            // to recalibrate the iteration count; none of that may freeze the
            // GUI. The dialog stays up with its inputs disabled, and
            // checkFinished() reports. The node leaves the unlock as it is,
            // whether or not the change succeeds, so a wallet unlocked for
            // staking is still unlocked for staking afterwards.
            startCheck(std::move(oldpass), std::move(newpass1));
        }
        else
        {
            QMessageBox::critical(this, tr("Wallet encryption failed"),
                                 tr("The supplied passphrases do not match."));
        }
        break;
    }
}

void AskPassphraseDialog::reject()
{
    // Cancels the check rather than waiting for it here, and the prompt stays
    // open, saying so, until the check reports: checkFinished() then ends it.
    // It must not be hidden or closed now. Hiding ends exec(), and the dialog's
    // owner would then destroy it, which waits for the check on the GUI thread. In
    // the monolithic build a check still waiting for its turn sees the cancel
    // and gives up at once. One already deriving the key finishes first, and
    // so, in the split build, does one waiting in the node's process, which
    // the cancel cannot reach; the event loop runs meanwhile. A check still
    // waiting for its turn on the worker is given up, and reported as one that
    // saw the cancel would be.
    if (m_check_in_flight) {
        m_check->cancel = true;
        m_check_cancelled = true;
        ui->statusLabel->setText(tr("Cancelling..."));

        if (m_check->GiveUp()) {
            QMetaObject::invokeMethod(this, &AskPassphraseDialog::checkFinished, Qt::QueuedConnection);
        }

        return;
    }

    QDialog::reject();
}

void AskPassphraseDialog::startCheck(SecureString passphrase, SecureString new_passphrase)
{
    m_check_had_null = passphrase.find('\0') != std::string::npos;
    m_check_reported = false;
    m_check_cancelled = false;
    m_check_in_flight = true;

    const auto check = std::make_shared<CheckState>();
    m_check = check;

    setChecking(true);

    // The check calls the wallet interface only: no Qt object is touched off
    // the GUI thread. It writes its outcome into the shared state, and posts
    // its report to this dialog, which outlives a check that has started (its
    // destructor waits for one); the report is dropped if the dialog is
    // destroyed before it runs. The passphrases move into the job and are
    // cleansed when it is destroyed.
    //
    // It runs on the process's modal worker, a thread that outlives the
    // connection to the core: a thread that has made IPC calls must not exit
    // while its connection may be destroyed (see GuiIpcWorker), and a
    // disconnect mid-check ends the call just as the connection is destroyed.
    interfaces::Wallet& wallet = model->wallet();
    const Mode check_mode = mode;
    const CapabilityKind kind = m_kind;
    const bool staking_only = (mode == UnlockStaking && ui->stakingCheckBox->isChecked());

    GuiIpcWorker* const worker = GuiIpcWorker::Current(GuiIpcWorker::Role::Modal);

    if (worker) {
        m_check_done = worker->Post([this, &wallet, check, check_mode, kind, staking_only,
                                     passphrase = std::move(passphrase),
                                     new_passphrase = std::move(new_passphrase)] {
            // Given up while it waited for its turn: the prompt may be gone.
            if (!check->Start()) return;

            bool accepted = false;

            // The attempt watches the cancel while it waits for its turn in
            // the wallet. Only in the monolithic build, where the wallet runs
            // on this thread: across IPC the attempt runs in the node's
            // process, which cannot see it.
            const PassphraseAttemptCancel attempt(check->cancel);

            // Nothing may escape a job. In the split build the call crosses
            // IPC, and a disconnect mid-call raises.
            try {
                switch (check_mode) {
                case Elevate:
                    accepted = wallet.beginElevation(passphrase, kind);
                    break;
                case UnlockStaking:
                    accepted = wallet.unlockWallet(passphrase, staking_only);
                    break;
                case ChangePass:
                    accepted = wallet.changeWalletPassphrase(passphrase, new_passphrase);
                    break;
                case Encrypt:
                    // Encryption checks no passphrase and does not come here.
                    break;
                }
            } catch (const std::exception& e) {
                LogPrintf("WARN: AskPassphraseDialog: the passphrase check failed: %s", e.what());
            } catch (...) {
                LogPrintf("WARN: AskPassphraseDialog: the passphrase check failed");
            }

            check->accepted = accepted;

            // The last use of this dialog.
            QMetaObject::invokeMethod(this, &AskPassphraseDialog::checkFinished, Qt::QueuedConnection);
        });
    } else {
        m_check_done = std::future<void>();
    }

    // No worker, or it could not take the job: the check reports as one that
    // failed, with nothing tried.
    if (!m_check_done.valid()) {
        LogPrintf("ERROR: AskPassphraseDialog: no thread is running to check the passphrase");
        check->GiveUp();
        QMetaObject::invokeMethod(this, &AskPassphraseDialog::checkFinished, Qt::QueuedConnection);
    }
}

void AskPassphraseDialog::setChecking(bool checking)
{
    ui->oldPassphraseEdit->setEnabled(!checking);
    ui->newPassphraseEdit->setEnabled(!checking);
    ui->repeatNewPassphraseEdit->setEnabled(!checking);
    ui->stakingCheckBox->setEnabled(!checking);

    // Of the buttons only OK is disabled: Cancel stays available while the
    // check runs, and cancels it (reject()). Afterwards OK follows the fields
    // again.
    if (checking) {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        ui->statusLabel->setText(mode == ChangePass ? tr("Changing the passphrase...")
                                                    : tr("Checking the passphrase..."));
        setCursor(Qt::BusyCursor);
    } else {
        textChanged();
        ui->statusLabel->clear();
        unsetCursor();
        ui->oldPassphraseEdit->setFocus();
    }
}

void AskPassphraseDialog::checkFinished()
{
    // Posted by the check as its last use of this dialog, or by the prompt for
    // a check that never ran.
    m_check_in_flight = false;
    m_check_reported = true;
    setChecking(false);

    const bool accepted = m_check->accepted;

    // Cancelled while the check ran: nothing waits for its outcome. A
    // capability it began has no operation to use it, so it is ended here. A
    // passphrase it changed did change, which the user must be told. Anything
    // else is let go, an unlock included: the user asked for it when they
    // pressed OK.
    if (m_check_cancelled) {
        if (accepted && mode == Elevate && model) {
            try {
                model->wallet().endElevation();
            } catch (const std::exception& e) {
                LogPrintf("WARN: %s: could not end a cancelled prompt's capability: %s", __func__, e.what());
            }
        }

        if (accepted && mode == ChangePass) {
            QMessageBox::information(this, tr("Wallet encrypted"),
                                     tr("Wallet passphrase was successfully changed."));
        }

        QDialog::reject();
        return;
    }

    if (mode == ChangePass) {
        if (accepted) {
            QMessageBox::information(this, tr("Wallet encrypted"),
                                     tr("Wallet passphrase was successfully changed."));
            QDialog::accept(); // Success
        } else if (!m_check_had_null) {
            // The dialog stays up: it is where the passphrase is retyped.
            QMessageBox::critical(this, tr("Passphrase change failed"),
                                  tr("The passphrase entered for the wallet decryption was incorrect."));
        } else {
            QMessageBox::critical(this, tr("Passphrase change failed"),
                                  tr("The old passphrase entered for the wallet decryption is incorrect. "
                                     "It contains a null character (ie - a zero byte). "
                                     "If the passphrase was set with a version of this software prior to 5.4.6, "
                                     "please try again with only the characters up to — but not including — "
                                     "the first null character."));
        }

        return;
    }

    if (accepted) {
        // The operation waiting in WalletModel::requestUnlock() takes the
        // capability, and its context ends it.
        if (mode == Elevate) {
            model->setElevationBegun();
        }

        QDialog::accept(); // Success
        return;
    }

    // A capability is begun on a locked wallet too, so a wallet that locked
    // itself while the prompt was open is no reason to refuse. It is worth
    // saying, though: the unlock the user had asked for ran out, so the node
    // is no longer staking.
    if (mode == Elevate && m_opened_staking_only
            && model->getEncryptionStatus() == WalletModel::Locked) {
        QMessageBox::critical(this, tr("Wallet unlock failed"),
                              tr("The passphrase was not accepted. The wallet also locked itself "
                                 "while you were entering it, because the unlock you had asked "
                                 "for ran out, so it is no longer unlocked for staking."));
    } else if (!m_check_had_null) {
        QMessageBox::critical(this, tr("Wallet unlock failed"),
                              tr("The passphrase entered for the wallet decryption was incorrect."));
    } else {
        QMessageBox::critical(this, tr("Wallet unlock failed"),
                              tr("The passphrase entered for the wallet decryption is incorrect. "
                                 "It contains a null character (ie - a zero byte). "
                                 "If the passphrase was set with a version of this software prior to 5.4.6, "
                                 "please try again with only the characters up to — but not including — "
                                 "the first null character. If this is successful, please set a new "
                                 "passphrase to avoid this issue in the future."));
    }
}

void AskPassphraseDialog::textChanged()
{
    // Validate input, set Ok button to enabled when acceptable
    bool acceptable = false;
    switch(mode)
    {
    case Encrypt: // New passphrase x2
        acceptable = !ui->newPassphraseEdit->text().isEmpty() && !ui->repeatNewPassphraseEdit->text().isEmpty();
        break;
    case UnlockStaking:
    case Elevate: // Old passphrase x1
        acceptable = !ui->oldPassphraseEdit->text().isEmpty();
        break;
    case ChangePass: // Old passphrase x1, new passphrase x2
        acceptable = !ui->oldPassphraseEdit->text().isEmpty() && !ui->newPassphraseEdit->text().isEmpty() && !ui->repeatNewPassphraseEdit->text().isEmpty();
        break;
    }
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(acceptable);
}

bool AskPassphraseDialog::event(QEvent *event)
{
    // Detect Caps Lock key press.
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_CapsLock) {
            fCapsLock = !fCapsLock;
        }
        if (fCapsLock) {
            ui->capsLabel->setText(tr("Warning: The Caps Lock key is on!"));
        } else {
            ui->capsLabel->clear();
        }
    }
    return QWidget::event(event);
}

bool AskPassphraseDialog::eventFilter(QObject *object, QEvent *event)
{
    /* Detect Caps Lock.
     * There is no good OS-independent way to check a key state in Qt, but we
     * can detect Caps Lock by checking for the following condition:
     * Shift key is down and the result is a lower case character, or
     * Shift key is not down and the result is an upper case character.
     */
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        QString str = ke->text();
        if (str.length() != 0) {
            const QChar *psz = str.unicode();
            bool fShift = (ke->modifiers() & Qt::ShiftModifier) != 0;
            if ((fShift && psz->isLower()) || (!fShift && psz->isUpper())) {
                fCapsLock = true;
                ui->capsLabel->setText(tr("Warning: The Caps Lock key is on!"));
            } else if (psz->isLetter()) {
                fCapsLock = false;
                ui->capsLabel->clear();
            }
        }
    }
    return QDialog::eventFilter(object, event);
}

void AskPassphraseDialog::secureClearPassFields()
{
    // Attempt to overwrite text so that they do not linger around in memory
    ui->oldPassphraseEdit->setText(QString(" ").repeated(ui->oldPassphraseEdit->text().size()));
    ui->newPassphraseEdit->setText(QString(" ").repeated(ui->newPassphraseEdit->text().size()));
    ui->repeatNewPassphraseEdit->setText(QString(" ").repeated(ui->repeatNewPassphraseEdit->text().size()));

    ui->oldPassphraseEdit->clear();
    ui->newPassphraseEdit->clear();
    ui->repeatNewPassphraseEdit->clear();
}
