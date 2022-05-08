// Copyright (c) 2014-2022 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_MRCMODEL_H
#define GRIDCOIN_QT_MRCMODEL_H

#include <QObject>

enum class MRCRequestStatus
{
    PENDING,
    REJECTED_QUEUE_FULL,
    STALE,
    DELETED
};

class MRCModel : public QObject
{
    Q_OBJECT

public:
    MRCModel();
    ~MRCModel();

};


#endif // GRIDCOIN_QT_MRCMODEL_H
