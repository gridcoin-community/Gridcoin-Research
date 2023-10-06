// Copyright (c) 2014-2023 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_EDITSIDESTAKEDIALOG_H
#define BITCOIN_QT_EDITSIDESTAKEDIALOG_H

#include <QDialog>

QT_BEGIN_NAMESPACE
class QDataWidgetMapper;
QT_END_NAMESPACE

namespace Ui {
class EditSideStakeDialog;
}
class SideStakeTableModel;

/** Dialog for editing an address and associated information.
 */
class EditSideStakeDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode {
        NewSideStake,
        EditSideStake
    };

    explicit EditSideStakeDialog(Mode mode, QWidget* parent = nullptr);
    ~EditSideStakeDialog();

    void setModel(SideStakeTableModel* model);
    void loadRow(int row);

public slots:
    void accept();

private:
    bool saveCurrentRow();

    Ui::EditSideStakeDialog *ui;
    Mode mode;
    SideStakeTableModel *model;
    int m_row;

    QString address;
};
#endif // BITCOIN_QT_EDITSIDESTAKEDIALOG_H
