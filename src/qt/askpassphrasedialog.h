#ifndef BITCOIN_QT_ASKPASSPHRASEDIALOG_H
#define BITCOIN_QT_ASKPASSPHRASEDIALOG_H

#include "interfaces/wallet.h" // For CapabilityKind.

#include <QDialog>

#include <atomic>
#include <memory>
#include <thread>

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
        //! An operation needs the wallet's keys, and the wallet is locked or
        //! unlocked for staking only. Begins a capability for the operation's
        //! kind (setCapabilityKind()) and widens nothing: the wallet stays as
        //! it is for every other caller, and the deadline of a timed unlock
        //! is untouched. WalletModel::UnlockContext ends the capability when
        //! the operation finishes. No checkbox: there is nothing to choose.
        Elevate,
        ChangePass,    /**< Ask old passphrase + new passphrase twice */
    };

    explicit AskPassphraseDialog(Mode mode, QWidget* parent = nullptr);
    ~AskPassphraseDialog();

    void accept();

    //! While the passphrase is being checked, cancels the check and returns;
    //! the prompt stays open, showing that it is cancelling, until the check
    //! reports. A check still waiting for its turn in this process gives up at
    //! once; one already checking, or one waiting in the node's process in the
    //! split build, finishes first. checkFinished() then ends the prompt
    //! rejected, acting on nothing the check found except to end a capability
    //! it began and to say so if the passphrase did change.
    void reject();

    void setModel(WalletModel *model);

    //! The kind of operation an Elevate prompt begins a capability for. Set
    //! before exec().
    void setCapabilityKind(CapabilityKind kind) { m_kind = kind; }

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
    CapabilityKind m_kind{CapabilityKind::Send};

    //! The wallet was unlocked for staking only when the prompt opened, so a
    //! refusal that finds it locked can say the unlock ran out meanwhile.
    bool m_opened_staking_only{false};

    //! The passphrase check for UnlockStaking, Elevate and ChangePass. It runs
    //! off the GUI thread, because a wrong passphrase waits out a delay of up
    //! to 30 s and the key derivation itself takes a moment, and the GUI must
    //! not freeze for either. The dialog stays open, its inputs disabled, until
    //! the check reports to checkFinished(). Joined there, or in the
    //! destructor. Encrypt checks no passphrase and stays on the GUI thread.
    std::thread m_check;

    //! The check's outcome, written by the check's thread before it reports.
    std::atomic<bool> m_check_accepted{false};

    //! checkFinished() has acted on the outcome.
    bool m_check_reported{false};

    //! Set by reject() to cancel the running check (PassphraseAttemptCancel).
    //! Shared with the check's thread, which watches it while it waits.
    std::shared_ptr<std::atomic<bool>> m_check_cancel;

    //! The prompt was cancelled while its check ran.
    bool m_check_cancelled{false};

    //! The passphrase being checked (the old one, for ChangePass) contained a
    //! null character.
    bool m_check_had_null{false};

    //! Start the check of \p passphrase on its own thread. For ChangePass,
    //! \p new_passphrase is the one to change it to.
    void startCheck(SecureString passphrase, SecureString new_passphrase = SecureString());

    //! Disable or re-enable the inputs, and show or clear the status line.
    void setChecking(bool checking);

private slots:
    void textChanged();
    bool event(QEvent *event);
    bool eventFilter(QObject *, QEvent *event);
    void secureClearPassFields();

    //! The check has reported; runs on the GUI thread.
    void checkFinished();
};

#endif // BITCOIN_QT_ASKPASSPHRASEDIALOG_H
