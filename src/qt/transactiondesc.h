#ifndef BITCOIN_QT_TRANSACTIONDESC_H
#define BITCOIN_QT_TRANSACTIONDESC_H

#include <QString>
#include <QObject>

#include "interfaces/wallet_tx_channel.h"

/** Render a human-readable extended HTML description of a transaction from the
 *  node-filled GRC::WalletTxDetail value DTO. All localization lives here on
 *  the GUI side (multiprocess design §4.1); the node never renders HTML.
 */
class TransactionDesc: public QObject
{
    Q_OBJECT
public:
    static QString toHTML(const GRC::WalletTxDetail& detail);
private:
    TransactionDesc() {}

    static QString FormatTxStatus(const GRC::WalletTxDetail& detail);
};

#endif // BITCOIN_QT_TRANSACTIONDESC_H
