/********************************************************************************
** Form generated from reading UI file 'votingpage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_VOTINGPAGE_H
#define UI_VOTINGPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "voting/polltab.h"

QT_BEGIN_NAMESPACE

class Ui_VotingPage
{
public:
    QVBoxLayout *votingPageVerticalLayout;
    QFrame *headerFrame;
    QHBoxLayout *headerFrameLayout;
    QWidget *headerTitleWrapper;
    QVBoxLayout *headerTitleVerticalLayout;
    QLabel *headerTitleLabel;
    QSpacerItem *headerFrameSpacer;
    QLineEdit *filterLineEdit;
    QFrame *tabButtonFrame;
    QHBoxLayout *horizontalLayout;
    QToolButton *cardsToggleButton;
    QToolButton *tableToggleButton;
    QToolButton *sortButton;
    QPushButton *refreshButton;
    QPushButton *createPollButton;
    QLabel *pollReceivedLabel;
    QWidget *tabWrapperWidget;
    QVBoxLayout *tabWrapperWidgetLayout;
    QTabWidget *tabWidget;
    PollTab *activePollsTab;
    PollTab *finishedPollsTab;

    void setupUi(QWidget *VotingPage)
    {
        if (VotingPage->objectName().isEmpty())
            VotingPage->setObjectName("VotingPage");
        VotingPage->resize(899, 456);
        votingPageVerticalLayout = new QVBoxLayout(VotingPage);
        votingPageVerticalLayout->setSpacing(0);
        votingPageVerticalLayout->setObjectName("votingPageVerticalLayout");
        votingPageVerticalLayout->setContentsMargins(0, 0, 0, 0);
        headerFrame = new QFrame(VotingPage);
        headerFrame->setObjectName("headerFrame");
        headerFrame->setFrameShape(QFrame::NoFrame);
        headerFrame->setFrameShadow(QFrame::Plain);
        headerFrameLayout = new QHBoxLayout(headerFrame);
        headerFrameLayout->setSpacing(15);
        headerFrameLayout->setObjectName("headerFrameLayout");
        headerFrameLayout->setContentsMargins(0, 0, 0, 0);
        headerTitleWrapper = new QWidget(headerFrame);
        headerTitleWrapper->setObjectName("headerTitleWrapper");
        headerTitleVerticalLayout = new QVBoxLayout(headerTitleWrapper);
        headerTitleVerticalLayout->setSpacing(4);
        headerTitleVerticalLayout->setObjectName("headerTitleVerticalLayout");
        headerTitleVerticalLayout->setContentsMargins(0, 0, 0, 0);
        headerTitleLabel = new QLabel(headerTitleWrapper);
        headerTitleLabel->setObjectName("headerTitleLabel");

        headerTitleVerticalLayout->addWidget(headerTitleLabel);


        headerFrameLayout->addWidget(headerTitleWrapper, 0, Qt::AlignVCenter);

        headerFrameSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        headerFrameLayout->addItem(headerFrameSpacer);

        filterLineEdit = new QLineEdit(headerFrame);
        filterLineEdit->setObjectName("filterLineEdit");
        filterLineEdit->setClearButtonEnabled(true);

        headerFrameLayout->addWidget(filterLineEdit);


        votingPageVerticalLayout->addWidget(headerFrame, 0, Qt::AlignVCenter);

        tabButtonFrame = new QFrame(VotingPage);
        tabButtonFrame->setObjectName("tabButtonFrame");
        horizontalLayout = new QHBoxLayout(tabButtonFrame);
        horizontalLayout->setSpacing(9);
        horizontalLayout->setObjectName("horizontalLayout");
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        cardsToggleButton = new QToolButton(tabButtonFrame);
        cardsToggleButton->setObjectName("cardsToggleButton");

        horizontalLayout->addWidget(cardsToggleButton);

        tableToggleButton = new QToolButton(tabButtonFrame);
        tableToggleButton->setObjectName("tableToggleButton");

        horizontalLayout->addWidget(tableToggleButton);

        sortButton = new QToolButton(tabButtonFrame);
        sortButton->setObjectName("sortButton");
        sortButton->setPopupMode(QToolButton::InstantPopup);

        horizontalLayout->addWidget(sortButton);

        refreshButton = new QPushButton(tabButtonFrame);
        refreshButton->setObjectName("refreshButton");

        horizontalLayout->addWidget(refreshButton, 0, Qt::AlignVCenter);

        createPollButton = new QPushButton(tabButtonFrame);
        createPollButton->setObjectName("createPollButton");

        horizontalLayout->addWidget(createPollButton, 0, Qt::AlignVCenter);


        votingPageVerticalLayout->addWidget(tabButtonFrame);

        pollReceivedLabel = new QLabel(VotingPage);
        pollReceivedLabel->setObjectName("pollReceivedLabel");
        pollReceivedLabel->setAlignment(Qt::AlignCenter);

        votingPageVerticalLayout->addWidget(pollReceivedLabel);

        tabWrapperWidget = new QWidget(VotingPage);
        tabWrapperWidget->setObjectName("tabWrapperWidget");
        tabWrapperWidgetLayout = new QVBoxLayout(tabWrapperWidget);
        tabWrapperWidgetLayout->setObjectName("tabWrapperWidgetLayout");
        tabWrapperWidgetLayout->setContentsMargins(0, 0, 0, 0);
        tabWidget = new QTabWidget(tabWrapperWidget);
        tabWidget->setObjectName("tabWidget");
        activePollsTab = new PollTab();
        activePollsTab->setObjectName("activePollsTab");
        tabWidget->addTab(activePollsTab, QString());
        finishedPollsTab = new PollTab();
        finishedPollsTab->setObjectName("finishedPollsTab");
        tabWidget->addTab(finishedPollsTab, QString());

        tabWrapperWidgetLayout->addWidget(tabWidget);


        votingPageVerticalLayout->addWidget(tabWrapperWidget);


        retranslateUi(VotingPage);

        tabWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(VotingPage);
    } // setupUi

    void retranslateUi(QWidget *VotingPage)
    {
        VotingPage->setWindowTitle(QCoreApplication::translate("VotingPage", "Voting", nullptr));
        headerTitleLabel->setText(QCoreApplication::translate("VotingPage", "Polls", nullptr));
        filterLineEdit->setPlaceholderText(QCoreApplication::translate("VotingPage", "Search by title", nullptr));
#if QT_CONFIG(tooltip)
        cardsToggleButton->setToolTip(QCoreApplication::translate("VotingPage", "View as list.", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(shortcut)
        cardsToggleButton->setShortcut(QCoreApplication::translate("VotingPage", "Alt+T", nullptr));
#endif // QT_CONFIG(shortcut)
#if QT_CONFIG(tooltip)
        tableToggleButton->setToolTip(QCoreApplication::translate("VotingPage", "View as table.", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(shortcut)
        tableToggleButton->setShortcut(QCoreApplication::translate("VotingPage", "Alt+T", nullptr));
#endif // QT_CONFIG(shortcut)
#if QT_CONFIG(tooltip)
        sortButton->setToolTip(QCoreApplication::translate("VotingPage", "Sort by...", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(shortcut)
        sortButton->setShortcut(QCoreApplication::translate("VotingPage", "Alt+S", nullptr));
#endif // QT_CONFIG(shortcut)
        refreshButton->setText(QCoreApplication::translate("VotingPage", "&Refresh", nullptr));
        createPollButton->setText(QCoreApplication::translate("VotingPage", "Create &Poll", nullptr));
        pollReceivedLabel->setText(QCoreApplication::translate("VotingPage", "A new poll is available. Press \"Refresh\" to load it.", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(activePollsTab), QCoreApplication::translate("VotingPage", "&Active", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(finishedPollsTab), QCoreApplication::translate("VotingPage", "&Completed", nullptr));
    } // retranslateUi

};

namespace Ui {
    class VotingPage: public Ui_VotingPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_VOTINGPAGE_H
