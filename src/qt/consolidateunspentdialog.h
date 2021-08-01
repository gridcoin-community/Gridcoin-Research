#ifndef CONSOLIDATEUNSPENTDIALOG_H
#define CONSOLIDATEUNSPENTDIALOG_H

#include <QDialogButtonBox>
#include <QDialog>
#include <QString>

namespace Ui {
    class ConsolidateUnspentDialog;
}

class ConsolidateUnspentDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConsolidateUnspentDialog(QWidget *parent = nullptr, size_t inputSelectionLimit = 600);
    ~ConsolidateUnspentDialog();

    void SetAddressList(const std::map<QString, QString>& addressList);
    void SetOutputWarningVisible(bool status);

signals:
    void selectedConsolidationAddressSignal(std::pair<QString, QString> address);

private:
    Ui::ConsolidateUnspentDialog *ui;

    std::pair<QString, QString> m_selectedDestinationAddress;

    size_t m_inputSelectionLimit;

private slots:
    void buttonBoxClicked(QAbstractButton *button);
    void addressSelectionChanged();
};

#endif // CONSOLIDATEUNSPENTDIALOG_H
