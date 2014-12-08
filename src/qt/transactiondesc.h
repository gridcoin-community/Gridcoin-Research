#ifndef TRANSACTIONDESC_H
#define TRANSACTIONDESC_H

#include <QString>
#include <QObject>
#include <string>

class CWallet;
class CWalletTx;

/** Provide a human-readable extended HTML description of a transaction.
 */
class TransactionDesc: public QObject
{
    Q_OBJECT
public:
	// explicit TransactionDesc(QWidget *parent = 0);
    //~TransactionDesc();
    static QString toHTML(CWallet *wallet, CWalletTx &wtx, std::string type);

	public slots:
		 void on_btnExecute_clicked();


private slots:
   
	//   WalletModel *walletModel;

private:
	// Ui::SendCoinsDialog *ui;
    TransactionDesc() 
	{
	    //connect(btnExecute, SIGNAL(clicked()), this, SLOT(clickmou()));
		//connect(btnExecute, SIGNAL(clicked()), this, SLOT(on_btnExecute_clicked()));

	}

    static QString FormatTxStatus(const CWalletTx& wtx);
};

#endif // TRANSACTIONDESC_H
