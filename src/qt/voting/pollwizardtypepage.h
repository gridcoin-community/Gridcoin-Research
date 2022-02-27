// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_VOTING_POLLWIZARDTYPEPAGE_H
#define GRIDCOIN_QT_VOTING_POLLWIZARDTYPEPAGE_H

#include <QWizardPage>

namespace Ui {
class PollWizardTypePage;
}

class PollTypes;

QT_BEGIN_NAMESPACE
class QButtonGroup;
QT_END_NAMESPACE

class PollWizardTypePage : public QWizardPage
{
    Q_OBJECT

public:
    explicit PollWizardTypePage(QWidget* parent = nullptr);
    ~PollWizardTypePage();

    void setPollTypes(const PollTypes* const poll_types);

    int nextId() const override;

private:
    Ui::PollWizardTypePage* ui;
    const PollTypes* m_poll_types;
    QButtonGroup* m_type_buttons;
};

#endif // GRIDCOIN_QT_VOTING_POLLWIZARDTYPEPAGE_H
