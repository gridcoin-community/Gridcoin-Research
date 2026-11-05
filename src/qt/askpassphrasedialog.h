#ifndef BITCOIN_QT_ASKPASSPHRASEDIALOG_H
#define BITCOIN_QT_ASKPASSPHRASEDIALOG_H

#include "interfaces/wallet.h" // For CapabilityKind and the autounlock types.

#include <QDialog>

#include <future>
#include <memory>
#include <string>

namespace Ui {
    class AskPassphraseDialog;
}

class WalletModel;

/** Multifunctional dialog to ask for passphrases. Used for encryption, unlocking, changing the passphrase, and
    setting up or turning off autounlock.
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
        //! Store the passphrase for staking-only autounlock once the core
        //! has checked it (Wallet::setAutoUnlockSecret()). The text above
        //! the passphrase says who can obtain it in the scope the core stores
        //! it in (setAutoUnlockStatus()). The wallet is left as it was.
        SetupAutoUnlock,
        //! Remove the stored passphrase once the core has checked the
        //! passphrase, which also locks the wallet
        //! (Wallet::clearAutoUnlockSecret()).
        ClearAutoUnlock,
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

    //! For SetupAutoUnlock: what the core's store reported when the prompt was
    //! asked for. Its scope picks the text above the passphrase, and its
    //! location is named once the passphrase is stored. Set before exec().
    void setAutoUnlockStatus(const AutoUnlockStoreStatus& status);

    //! What a passphrase stored in \p scope is exposed to, as the prompt says it:
    //! the text for "machine" or "service", translated, and an empty string for
    //! any other scope. Untranslated, it is AutoUnlockScopeWarning(scope).
    static QString AutoUnlockScopeText(const std::string& scope);

private:
    Ui::AskPassphraseDialog *ui;
    Mode mode;
    WalletModel *model;
    bool fCapsLock;
    bool m_multiprocess{false};
    CapabilityKind m_kind{CapabilityKind::Send};

    //! What setAutoUnlockStatus() was given.
    AutoUnlockStoreStatus m_autounlock_status{};

    //! The wallet was unlocked for staking only when the prompt opened, so a
    //! refusal that finds it locked can say the unlock ran out meanwhile.
    bool m_opened_staking_only{false};

    //! What the dialog and its check share. Defined in the .cpp.
    struct CheckState;

    //! The passphrase check for every mode but Encrypt. It runs
    //! off the GUI thread, on the process's modal GuiIpcWorker, because a
    //! wrong passphrase waits out a delay of up to 30 s and the key derivation
    //! itself takes a moment, and the GUI must not freeze for either. The
    //! dialog stays open, its inputs disabled, until the check reports to
    //! checkFinished(). The check writes its outcome here, never into the
    //! dialog. Encrypt checks no passphrase and stays on the GUI thread.
    std::shared_ptr<CheckState> m_check;

    //! Ready once the check's job has finished on the worker. The destructor
    //! waits for it while a check that has begun is still running.
    std::future<void> m_check_done;

    //! A check has started and has not yet reported to checkFinished().
    bool m_check_in_flight{false};

    //! checkFinished() has acted on the outcome.
    bool m_check_reported{false};

    //! The prompt was cancelled while its check ran.
    bool m_check_cancelled{false};

    //! The passphrase being checked (the old one, for ChangePass) contained a
    //! null character.
    bool m_check_had_null{false};

    //! Start the check of \p passphrase on the modal worker. For ChangePass,
    //! \p new_passphrase is the one to change it to.
    void startCheck(SecureString passphrase, SecureString new_passphrase = SecureString());

    //! Disable or re-enable the inputs, and show or clear the status line.
    void setChecking(bool checking);

    //! Tell the user how setting up or turning off autounlock ended, from the
    //! check's outcome (CheckState::autounlock_result). True when the prompt
    //! stays open for the passphrase to be typed again.
    bool reportAutoUnlock();

private slots:
    void textChanged();
    bool event(QEvent *event);
    bool eventFilter(QObject *, QEvent *event);
    void secureClearPassFields();

    //! The check has reported; runs on the GUI thread.
    void checkFinished();
};

#endif // BITCOIN_QT_ASKPASSPHRASEDIALOG_H
