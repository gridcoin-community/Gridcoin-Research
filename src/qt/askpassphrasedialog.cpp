#include "askpassphrasedialog.h"
#include "ui_askpassphrasedialog.h"

#include "bitcoingui.h"
#include "guiconstants.h"
#include "qt/decoration.h"
#include "walletmodel.h"

#include <QMessageBox>
#include <QPushButton>
#include <QKeyEvent>

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
        case Unlock: // Ask passphrase
            ui->warningLabel->setText(tr("This operation needs your wallet passphrase to unlock the wallet."));
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
    secureClearPassFields();
    delete ui;
}

void AskPassphraseDialog::setModel(WalletModel *model)
{
    this->model = model;

    // Seed the staking-only checkbox from the persisted unlock preference —
    // done here rather than in the constructor, which runs before a model is
    // attached (the preference lives behind the wallet interface now, not a
    // core global). UnlockStaking mode forces the box checked in the
    // constructor and stays forced.
    if (model && mode != UnlockStaking) {
        ui->stakingCheckBox->setChecked(model->wallet().getUnlockStakingOnlyFlag());
    }
}

void AskPassphraseDialog::accept()
{
    SecureString oldpass, newpass1, newpass2;
    if(!model)
        return;
    oldpass.reserve(MAX_PASSPHRASE_SIZE);
    newpass1.reserve(MAX_PASSPHRASE_SIZE);
    newpass2.reserve(MAX_PASSPHRASE_SIZE);

    oldpass.assign(std::string_view{ui->oldPassphraseEdit->text().toStdString()});
    newpass1.assign(std::string_view{ui->newPassphraseEdit->text().toStdString()});
    newpass2.assign(std::string_view{ui->repeatNewPassphraseEdit->text().toStdString()});

    secureClearPassFields();

    switch(mode)
    {
    case Encrypt: {
        if(newpass1.empty() || newpass2.empty())
        {
            // Cannot encrypt with empty passphrase
            break;
        }
        QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm wallet encryption"),
                 tr("Warning: If you encrypt your wallet and lose your passphrase, you will <b>LOSE ALL OF YOUR COINS</b>!") + "<br><br>" + tr("Are you sure you wish to encrypt your wallet?"),
                 QMessageBox::Yes|QMessageBox::Cancel,
                 QMessageBox::Cancel);
        if(retval == QMessageBox::Yes) {
            if(newpass1 == newpass2) {
                if(model->setWalletEncrypted(newpass1)) {
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
                        // Multiprocess: the wallet lives in the separate core process, which
                        // keeps running -- closing this GUI would NOT restart it. The on-disk
                        // encryption is already complete (the wallet database was rewritten and
                        // the keypool regenerated in the core); the core just needs a restart to
                        // drop the pre-encryption database state it still holds in memory and
                        // reload cleanly. Deliberately no requestQuit() here.
                        QMessageBox::warning(this, tr("Wallet encrypted - restart the core to finish"),
                                             "<qt>" +
                                                 tr("Your wallet is now fully encrypted and has been locked.") +
                                                 "<br><br>" +
                                                 tr("You are running in multiprocess mode, so the wallet lives in the "
                                                    "separate Gridcoin core process, which is still running. To finish the "
                                                    "encryption cleanly, restart the core process now (the running "
                                                    "<code>gridcoinresearchd</code>). The encryption on disk is already "
                                                    "complete; the restart is only so the core drops the pre-encryption "
                                                    "database state it still holds in memory and reloads the encrypted "
                                                    "wallet fresh.") +
                                                 "<br><br>" +
                                                 tr("Note: closing this window will not restart the core. Only "
                                                    "stopping and starting the core process does that; stopping it "
                                                    "will also close this window, so reopen the wallet once the core "
                                                    "is back up.") +
                                                 "<br><br>" + backupWarning + "</qt>");
                    }
                }
                else {
                    QMessageBox::critical(this, tr("Wallet encryption failed"),
                                         tr("Wallet encryption failed due to an internal error. Your wallet was not encrypted."));
                }
                QDialog::accept(); // Success
            } else {
                QMessageBox::critical(this, tr("Wallet encryption failed"),
                                     tr("The supplied passphrases do not match."));
            }
        } else {
            QDialog::reject(); // Cancelled
        }
    } break;
    case UnlockStaking:
    case Unlock:
        // A successful unlock also persists the staking-only preference
        // node-side (the checkbox state), which the old code applied to the
        // fWalletUnlockStakingOnly global after the fact.
        if(!model->setWalletLocked(false, oldpass, ui->stakingCheckBox->isChecked())) {
            // Check if the passphrase has a null character
            if (oldpass.find('\0') == std::string::npos) {
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
        else
        {
            QDialog::accept(); // Success
        }
        break;
    case ChangePass:
        if(newpass1 == newpass2)
        {
            if(model->changePassphrase(oldpass, newpass1))
            {
                QMessageBox::information(this, tr("Wallet encrypted"),
                                     tr("Wallet passphrase was successfully changed."));
                QDialog::accept(); // Success
            }
            else
            {
                // Check if the old passphrase had a null character
                if (oldpass.find('\0') == std::string::npos) {
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
            }
        }
        else
        {
            QMessageBox::critical(this, tr("Wallet encryption failed"),
                                 tr("The supplied passphrases do not match."));
        }
        break;
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
    case Unlock: // Old passphrase x1
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
