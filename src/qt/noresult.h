// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_NORESULT_H
#define BITCOIN_QT_NORESULT_H

#include <QWidget>

namespace Ui {
class NoResult;
}

class NoResult : public QWidget
{
    Q_OBJECT

public:
    explicit NoResult(QWidget *parent = nullptr);
    ~NoResult();

    QWidget* contentWidget();

    template <typename ContentWidget>
    ContentWidget* contentWidgetAs()
    {
        return qobject_cast<ContentWidget*>(m_content_widget);
    }


public slots:
    void setTitle(const QString& title);
    void setContentWidget(QWidget* widget);
    void showDefaultNothingHereTitle();
    void showDefaultNoResultTitle();
    void showDefaultLoadingTitle();
    void showPrivacyEnabledTitle();

private:
    Ui::NoResult *ui;
    QWidget* m_content_widget;
};

#endif // BITCOIN_QT_NORESULT_H
