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
        UnlockStaking, /**< Ask passphrase and unlock */
        Unlock,        /**< Ask passphrase and unlock */
        ChangePass,    /**< Ask old passphrase + new passphrase twice */
    };

    explicit AskPassphraseDialog(Mode mode, QWidget* parent = nullptr);
    ~AskPassphraseDialog();

    void accept();

    void setModel(WalletModel *model);

private:
    Ui::AskPassphraseDialog *ui;
    Mode mode;
    WalletModel *model;
    bool fCapsLock;

private slots:
    void textChanged();
    bool event(QEvent *event);
    bool eventFilter(QObject *, QEvent *event);
    void secureClearPassFields();
};

#endif // BITCOIN_QT_ASKPASSPHRASEDIALOG_H
