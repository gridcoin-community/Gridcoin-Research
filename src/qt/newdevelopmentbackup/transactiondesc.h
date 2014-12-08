#ifndef TRANSACTIONDESCDIALOG_H
#define TRANSACTIONDESCDIALOG_H
#include <QDialog>
/** Provide a human-readable extended HTML description of a transaction. */

namespace Ui {
    class TransactionDescDialog;
}
class ClientModel;
class CWallet;
class CWalletTx;

class TransactionDescDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TransactionDescDialog(QWidget *parent = 0);
    ~TransactionDescDialog();
    void setModel(ClientModel *model);
    static QString toHTML(CWallet *wallet, CWalletTx &wtx, std::string type);

private:
	Ui::TransactionDescDialog *ui;
    static QString FormatTxStatus(const CWalletTx& wtx);

private slots:
		 void on_btnExecute_clicked();
   
};

#endif // TRANSACTIONDESCDIALOG_H
