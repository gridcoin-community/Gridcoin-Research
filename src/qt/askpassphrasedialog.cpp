#include "askpassphrasedialog.h"
#include "ui_askpassphrasedialog.h"

#include "guiconstants.h"
#include "walletmodel.h"

#include <QMessageBox>
#include <QPushButton>
#include <QKeyEvent>

extern bool fWalletUnlockStakingOnly;

AskPassphraseDialog::AskPassphraseDialog(Mode mode, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AskPassphraseDialog),
    mode(mode),
    model(0),
    fCapsLock(false)
{
    ui->setupUi(this);
    ui->oldPassphraseEdit->setMaxLength(MAX_PASSPHRASE_SIZE);
    ui->newPassphraseEdit->setMaxLength(MAX_PASSPHRASE_SIZE);
    ui->repeatNewPassphraseEdit->setMaxLength(MAX_PASSPHRASE_SIZE);

    // Setup Caps Lock detection.
    ui->oldPassphraseEdit->installEventFilter(this);
    ui->newPassphraseEdit->installEventFilter(this);
    ui->repeatNewPassphraseEdit->installEventFilter(this);

    ui->stakingCheckBox->setChecked(fWalletUnlockStakingOnly);

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
        case Decrypt:   // Ask passphrase
            ui->warningLabel->setText(tr("This operation needs your wallet passphrase to decrypt the wallet."));
            ui->newPassphraseLabel->hide();
            ui->newPassphraseEdit->hide();
            ui->repeatNewPassphraseLabel->hide();
            ui->repeatNewPassphraseEdit->hide();
            setWindowTitle(tr("Decrypt wallet"));
            break;
        case ChangePass: // Ask old passphrase + new passphrase x2
            setWindowTitle(tr("Change passphrase"));
            ui->warningLabel->setText(tr("Enter the old and new passphrase to the wallet."));
            break;
    }

    textChanged();
    connect(ui->oldPassphraseEdit, SIGNAL(textChanged(QString)), this, SLOT(textChanged()));
    connect(ui->newPassphraseEdit, SIGNAL(textChanged(QString)), this, SLOT(textChanged()));
    connect(ui->repeatNewPassphraseEdit, SIGNAL(textChanged(QString)), this, SLOT(textChanged()));
}

AskPassphraseDialog::~AskPassphraseDialog()
{
    secureClearPassFields();
    delete ui;
}

void AskPassphraseDialog::setModel(WalletModel *model)
{
    this->model = model;
}

void AskPassphraseDialog::accept()
{
    SecureString oldpass, newpass1, newpass2;
    if(!model)
        return;
    oldpass.reserve(MAX_PASSPHRASE_SIZE);
    newpass1.reserve(MAX_PASSPHRASE_SIZE);
    newpass2.reserve(MAX_PASSPHRASE_SIZE);
    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make this input mlock()'d to begin with.
    oldpass.assign(ui->oldPassphraseEdit->text().toStdString().c_str());
    newpass1.assign(ui->newPassphraseEdit->text().toStdString().c_str());
    newpass2.assign(ui->repeatNewPassphraseEdit->text().toStdString().c_str());

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
        if(retval == QMessageBox::Yes)
        {
            if(newpass1 == newpass2)
            {
                if(model->setWalletEncrypted(true, newpass1))
                {
                    QMessageBox::warning(this, tr("Wallet encrypted"),
                                         "<qt>" +
                                         tr("Gridcoin will close now to finish the encryption process. "
                                         "Remember that encrypting your wallet cannot fully protect "
                                         "your coins from being stolen by malware infecting your computer.") +
                                         "<br><br><b>" +
                                         tr("IMPORTANT: Any previous backups you have made of your wallet file "
                                         "should be replaced with the newly generated, encrypted wallet file. "
                                         "For security reasons, previous backups of the unencrypted wallet file "
                                         "will become useless as soon as you start using the new, encrypted wallet.") +
                                         "</b></qt>");
                    QApplication::quit();
                }
                else
                {
                    QMessageBox::critical(this, tr("Wallet encryption failed"),
                                         tr("Wallet encryption failed due to an internal error. Your wallet was not encrypted."));
                }
                QDialog::accept(); // Success
            }
            else
            {
                QMessageBox::critical(this, tr("Wallet encryption failed"),
                                     tr("The supplied passphrases do not match."));
            }
        }
        else
        {
            QDialog::reject(); // Cancelled
        }
        } break;
    case UnlockStaking:
    case Unlock:
        if(!model->setWalletLocked(false, oldpass))
        {
            QMessageBox::critical(this, tr("Wallet unlock failed"),
                                  tr("The passphrase entered for the wallet decryption was incorrect."));
        }
        else
        {
            fWalletUnlockStakingOnly = ui->stakingCheckBox->isChecked();
            QDialog::accept(); // Success
        }
        break;
    case Decrypt:
        if(!model->setWalletEncrypted(false, oldpass))
        {
            QMessageBox::critical(this, tr("Wallet decryption failed"),
                                  tr("The passphrase entered for the wallet decryption was incorrect."));
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
                QMessageBox::critical(this, tr("Wallet encryption failed"),
                                     tr("The passphrase entered for the wallet decryption was incorrect."));
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
    case Decrypt:
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
