// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_GUIEVENTLOOPTESTS_H
#define BITCOIN_QT_TEST_GUIEVENTLOOPTESTS_H

#include <QObject>
#include <QTest>

class GuiEventLoopTests : public QObject
{
    Q_OBJECT

private slots:
    void reapsBeforeNodeDiesOnUnwind();
    void reapsBeforeReturn();
};

#endif // BITCOIN_QT_TEST_GUIEVENTLOOPTESTS_H
