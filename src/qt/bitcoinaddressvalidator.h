#ifndef BITCOIN_QT_BITCOINADDRESSVALIDATOR_H
#define BITCOIN_QT_BITCOINADDRESSVALIDATOR_H

#include <QValidator>

/** Base58 entry widget validator.
   Corrects near-miss characters and refuses characters that are no part of base58.
 */
class BitcoinAddressValidator : public QValidator
{
    Q_OBJECT
public:
    explicit BitcoinAddressValidator(QObject* parent = nullptr);

    State validate(QString &input, int &pos) const;

    static const int MaxAddressLength = 35;
signals:

public slots:

};

#endif // BITCOIN_QT_BITCOINADDRESSVALIDATOR_H
