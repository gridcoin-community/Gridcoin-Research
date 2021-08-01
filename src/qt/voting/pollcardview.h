// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VOTING_POLLCARDVIEW_H
#define VOTING_POLLCARDVIEW_H

#include <memory>
#include <QWidget>

namespace Ui {
class PollCardView;
}

class PollTableModel;

QT_BEGIN_NAMESPACE
class QHideEvent;
class QShowEvent;
class QTimer;
QT_END_NAMESPACE

class PollCardView : public QWidget
{
    Q_OBJECT

public:
    explicit PollCardView(QWidget* parent = nullptr);
    ~PollCardView();

    void setModel(PollTableModel* model);

    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

signals:
    void voteRequested(int row);
    void detailsRequested(int row);

public slots:
    void updateRemainingTime();
    void updateIcons(const QString& theme);

private:
    Ui::PollCardView* ui;
    PollTableModel* m_model;
    std::unique_ptr<QTimer> m_refresh_timer;
    QString m_theme;

private slots:
    void redraw();
    void clear();
};

#endif // VOTING_POLLCARDVIEW_H
