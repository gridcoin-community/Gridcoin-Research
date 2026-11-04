#ifndef BITCOIN_QT_ASKPASSPHRASEDIALOG_H
#define BITCOIN_QT_ASKPASSPHRASEDIALOG_H

#include "interfaces/wallet.h" // For CapabilityKind.

#include <QDialog>

#include <atomic>
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

    //! Ignored while the passphrase is being checked: the check cannot be
    //! called back, and closing the prompt under it would lose its outcome.
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

    //! The passphrase check for UnlockStaking and Elevate. It runs off the GUI
    //! thread, because a wrong passphrase waits out a delay of up to 30 s and
    //! the key derivation itself takes a moment, and the GUI must not freeze
    //! for either. The prompt stays open, its inputs disabled, until the check
    //! reports to checkFinished(). Joined there, or in the destructor.
    std::thread m_check;

    //! The check's outcome, written by the check's thread before it reports.
    std::atomic<bool> m_check_accepted{false};

    //! checkFinished() has acted on the outcome.
    bool m_check_reported{false};

    //! The passphrase being checked contained a null character.
    bool m_check_had_null{false};

    //! Start the check of \p passphrase on its own thread.
    void startCheck(SecureString passphrase);

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
