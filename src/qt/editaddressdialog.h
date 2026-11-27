#ifndef BITCOIN_QT_EDITADDRESSDIALOG_H
#define BITCOIN_QT_EDITADDRESSDIALOG_H

#include <QDialog>
#include <QPushButton>
#include <QString>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QComboBox;
class QDataWidgetMapper;
QT_END_NAMESPACE

namespace Ui {
    class EditAddressDialog;
}
class AddressTableModel;

/** Dialog for editing an address and associated information.
 */
class EditAddressDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode {
        NewReceivingAddress,
        NewSendingAddress,
        EditReceivingAddress,
        EditSendingAddress,
        AddExistingReceivingAddress
    };

    explicit EditAddressDialog(Mode mode, QWidget* parent = nullptr);
    ~EditAddressDialog();

    void setModel(AddressTableModel *model);
    void loadRow(int row);

    QString getAddress() const;
    void setAddress(const QString &address);

public slots:
    void accept();

private slots:
    //! The answer to a picker request: when \p serial is this dialog's own,
    //! ends the loading state and fills the combo; any other answer is ignored.
    void unbookedReceiveAddressesLoaded(quint64 serial, const QStringList& addresses, const QString& error);

private:
    bool saveCurrentRow();
    QString getDisplayAddress() const;

    Ui::EditAddressDialog *ui;
    QComboBox *addressCombo;
    QDataWidgetMapper *mapper;
    Mode mode;
    AddressTableModel *model;

    QString address;

    //! The serial of this dialog's own picker request; 0 until it asks.
    quint64 m_unbooked_serial = 0;
};

#endif // BITCOIN_QT_EDITADDRESSDIALOG_H
