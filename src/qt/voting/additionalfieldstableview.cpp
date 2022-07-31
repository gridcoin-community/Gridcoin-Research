// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/guiutil.h"
#include "qt/voting/additionalfieldstableview.h"

#include <QHeaderView>
#include <QScrollBar>

AdditionalFieldsTableView::AdditionalFieldsTableView(QWidget* parent)
    : QTableView(parent)
{
}

AdditionalFieldsTableView::~AdditionalFieldsTableView()
{
}

void AdditionalFieldsTableView::resizeEvent(QResizeEvent* event)
{
    int height = horizontalHeader()->height();
    for (int i = 0; i < model()->rowCount(); ++i)
    {
        height += rowHeight(i);
    }

    if (horizontalScrollBar()->isVisible()) {
        height += horizontalScrollBar()->height();
    }

    setMaximumHeight(height + 2);
}
