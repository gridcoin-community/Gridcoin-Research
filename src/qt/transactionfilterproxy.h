#ifndef BITCOIN_QT_TRANSACTIONFILTERPROXY_H
#define BITCOIN_QT_TRANSACTIONFILTERPROXY_H

#include <qt/txfilter.h>

#include <QSortFilterProxyModel>
#include <QDateTime>

/** Filter the transaction list according to pre-specified rules. */
class TransactionFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit TransactionFilterProxy(QObject* parent = nullptr);

    /** Earliest date that can be represented (far in the past) */
    static const QDateTime MIN_DATE;
    /** Last date that can be represented (far in the future) */
    static const QDateTime MAX_DATE;
    /** Type filter bit field (all types) */
    static const quint32 ALL_TYPES = 0xFFFFFFFF;

    static quint32 TYPE(int type) { return 1<<type; }

    void setDateRange(const QDateTime &from, const QDateTime &to);
    void setAddressPrefix(const QString &addrPrefix);
    /**
      @note Type filter takes a bit field created with TYPE() or ALL_TYPES
     */
    void setTypeFilter(quint32 modes);
    void setMinAmount(qint64 minimum);

    /** Set maximum number of rows returned, -1 if unlimited. */
    void setLimit(int limit);

    /** Get maximum number of rows returned, -1 if unlimited. */
    int getLimit();

    /** Set whether to show conflicted transactions. */
    void setShowInactive(bool showInactive);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
protected:
    bool filterAcceptsRow(int source_row, const QModelIndex & source_parent) const;

private:
    // The scattered date/address/type/amount/limit/show-inactive members were
    // collapsed into one value-typed GRC::FilterSpec, which is the parameter
    // the view will push to the producer-side cursor in the windowed model.
    // The setters below translate their Qt arguments into this spec; the
    // per-row predicate is evaluated by the Qt-free GRC::Accepts(). show_orphans
    // is read once from gArgs at construction (it is a launch arg, so this is
    // equivalent to the original per-row read).
    GRC::FilterSpec m_spec;

signals:

public slots:

};

#endif // BITCOIN_QT_TRANSACTIONFILTERPROXY_H
