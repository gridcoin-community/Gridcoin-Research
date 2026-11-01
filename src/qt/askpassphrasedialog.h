#ifndef BITCOIN_QT_ASKPASSPHRASEDIALOG_H
#define BITCOIN_QT_ASKPASSPHRASEDIALOG_H

#include <QDialog>

namespace Ui {
    class AskPassphraseDialog;
}

class WalletModel;

/** Multifunctional dialog to ask for passphrases. Used for encryption, unlocking, and changing the passphrase.
 */
class AskPassphraseDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode {
        Encrypt,       /**< Ask passphrase twice and encrypt */
        //! The Unlock BUTTON: a directive. Shows the staking-only checkbox,
        //! ticked by default, and leaves the wallet in the state the user
        //! chose. That state outlives this dialog.
        UnlockStaking,
        //! An operation needs a full unlock and the wallet is LOCKED. No
        //! checkbox: staking-only would not satisfy the caller, so there is
        //! nothing to choose. WalletModel::UnlockContext locks again when the
        //! operation finishes.
        Unlock,
        //! An operation needs a full unlock and the wallet is already unlocked
        //! FOR STAKING. Widens that unlock in place, so its deadline and the
        //! relock armed for it survive, and UnlockContext narrows it back.
        //!
        //! Distinct from Unlock because the two must not be confused when the
        //! unlock expires while the prompt is open. Treating that as an
        //! ordinary Unlock would start a fresh indefinite unlock, which the
        //! context would then narrow to staking-only with no deadline at all --
        //! silently turning the time-boxed unlock the user asked for into a
        //! permanent one. In this mode that case is reported instead.
        Elevate,
        ChangePass,    /**< Ask old passphrase + new passphrase twice */
    };

    explicit AskPassphraseDialog(Mode mode, QWidget* parent = nullptr);
    ~AskPassphraseDialog();

    void accept();

    void setModel(WalletModel *model);

    //! In the -multiprocess split build the wallet lives in a separate core
    //! process, so encrypting it does not restart via this GUI (closing the GUI
    //! leaves the core running). Set from BitcoinGUI (GuiIpcInfo::active) before
    //! exec() so the Encrypt-completion message can guide the user accordingly.
    void setMultiprocess(bool multiprocess) { m_multiprocess = multiprocess; }

private:
    Ui::AskPassphraseDialog *ui;
    Mode mode;
    WalletModel *model;
    bool fCapsLock;
    bool m_multiprocess{false};

private slots:
    void textChanged();
    bool event(QEvent *event);
    bool eventFilter(QObject *, QEvent *event);
    void secureClearPassFields();
};

#endif // BITCOIN_QT_ASKPASSPHRASEDIALOG_H
