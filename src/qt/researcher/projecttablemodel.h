// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_RESEARCHER_PROJECTTABLEMODEL_H
#define GRIDCOIN_QT_RESEARCHER_PROJECTTABLEMODEL_H

#include <memory>
#include <QAbstractTableModel>
#include <QStringList>

class ProjectTableData;
class ResearcherModel;

class ProjectTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum ColumnIndex {
        Name,
        Eligible,
        Whitelisted,
        Magnitude,
        RecentAverageCredit,
        Cpid,
    };

    explicit ProjectTableModel(
        ResearcherModel *parent = nullptr,
        const bool extended = true);

    ~ProjectTableModel();

    void setModel(ResearcherModel *model);

    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    void sort(int column, Qt::SortOrder order) override;

public slots:
    void refresh();

private:
    ResearcherModel *m_model;
    QStringList m_columns;
    std::unique_ptr<ProjectTableData> m_data;
    bool m_extended;
};

#endif // GRIDCOIN_QT_RESEARCHER_PROJECTTABLEMODEL_H
