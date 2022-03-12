#ifndef BITCOIN_QT_TRANSACTIONDESC_H
#define BITCOIN_QT_TRANSACTIONDESC_H

#include <QString>
#include <QObject>
#include <string>

#include "wallet/ismine.h"

class CWallet;
class CWalletTx;

/** Provide a human-readable extended HTML description of a transaction.
 */
class TransactionDesc: public QObject
{
    Q_OBJECT
public:
    static QString toHTML(CWallet *wallet, CWalletTx &wtx, unsigned int vout);
private:
    TransactionDesc() {}

    static QString FormatTxStatus(const CWalletTx& wtx);
};

#endif // BITCOIN_QT_TRANSACTIONDESC_H
