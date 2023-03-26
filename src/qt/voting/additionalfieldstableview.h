// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_VOTING_ADDITIONALFIELDSTABLEVIEW_H
#define GRIDCOIN_QT_VOTING_ADDITIONALFIELDSTABLEVIEW_H

#include <QTableView>

namespace Ui {
class AdditionalFieldsTableView;
}

class AdditionalFieldsTableView : public QTableView
{
    Q_OBJECT

public:
    explicit AdditionalFieldsTableView(QWidget* parent = nullptr);
    ~AdditionalFieldsTableView();

protected:
    void resizeEvent(QResizeEvent *event);
};

#endif // GRIDCOIN_QT_VOTING_ADDITIONALFIELDSTABLEVIEW_H
