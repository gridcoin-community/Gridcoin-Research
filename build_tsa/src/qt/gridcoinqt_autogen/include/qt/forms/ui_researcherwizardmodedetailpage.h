/********************************************************************************
** Form generated from reading UI file 'researcherwizardmodedetailpage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_RESEARCHERWIZARDMODEDETAILPAGE_H
#define UI_RESEARCHERWIZARDMODEDETAILPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <QtWidgets/QWizardPage>
#include "clicklabel.h"

QT_BEGIN_NAMESPACE

class Ui_ResearcherWizardModeDetailPage
{
public:
    QVBoxLayout *verticalLayout;
    QLabel *titleLabel;
    QLabel *explanationLabel;
    QScrollArea *scrollArea;
    QWidget *scrollAreaWidgetContents;
    QVBoxLayout *verticalLayout_2;
    QHBoxLayout *comparisonHorizontalLayout;
    QGridLayout *labelsLayout;
    QLabel *blockRewardLabel;
    QLabel *voteLabel;
    QLabel *decentralizedLabel;
    QLabel *secureNetworkLabel;
    QLabel *keepRewardsLabel;
    QLabel *boincRewardsLabel;
    QLabel *upfrontInvestmentLabel;
    QLabel *myChoiceLabel;
    QLabel *boincLeaderboardsLabel;
    QGridLayout *modeLayout;
    ClickLabel *soloBoincRewardsIconLabel;
    QLabel *noncruncherDecentralizedIconLabel;
    QLabel *noncruncherSecureNetworkIconLabel;
    QLabel *soloDecentralizedIconLabel;
    ClickLabel *soloBlockRewardIconLabel;
    QRadioButton *poolRadioButton;
    QLabel *soloVoteIconLabel;
    QLabel *poolKeepRewardIconLabel;
    QLabel *soloUpfrontInvestmentIconLabel;
    QLabel *poolSecureNetworkIconLabel;
    QLabel *soloSecureNetworkIconLabel;
    QLabel *poolDecentralizedIconLabel;
    QRadioButton *soloRadioButton;
    QLabel *poolUpfrontInvestmentIconLabel;
    ClickLabel *noncruncherBoincRewardsIconLabel;
    QLabel *poolVoteIconLabel;
    QLabel *noncruncherBlockRewardIconLabel;
    QLabel *noncruncherKeepRewardIconLabel;
    QRadioButton *noncruncherRadioButton;
    QLabel *poolBlockRewardIconLabel;
    QLabel *poolBoincRewardsIconLabel;
    QLabel *noncruncherUpfrontInvestmentIconLabel;
    QLabel *noncruncherVoteIconLabel;
    QLabel *soloKeepRewardsIconLabel;
    QLabel *soloBoincLeaderboardsIconLabel;
    QLabel *poolBoincLeaderboardsIconLabel;
    QLabel *noncruncherBoincLeaderboardsIconLabel;
    QButtonGroup *modeButtonGroup;

    void setupUi(QWizardPage *ResearcherWizardModeDetailPage)
    {
        if (ResearcherWizardModeDetailPage->objectName().isEmpty())
            ResearcherWizardModeDetailPage->setObjectName("ResearcherWizardModeDetailPage");
        ResearcherWizardModeDetailPage->resize(630, 480);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(ResearcherWizardModeDetailPage->sizePolicy().hasHeightForWidth());
        ResearcherWizardModeDetailPage->setSizePolicy(sizePolicy);
        verticalLayout = new QVBoxLayout(ResearcherWizardModeDetailPage);
        verticalLayout->setSpacing(12);
        verticalLayout->setObjectName("verticalLayout");
        titleLabel = new QLabel(ResearcherWizardModeDetailPage);
        titleLabel->setObjectName("titleLabel");

        verticalLayout->addWidget(titleLabel, 0, Qt::AlignHCenter|Qt::AlignVCenter);

        explanationLabel = new QLabel(ResearcherWizardModeDetailPage);
        explanationLabel->setObjectName("explanationLabel");
        explanationLabel->setTextFormat(Qt::RichText);
        explanationLabel->setWordWrap(true);

        verticalLayout->addWidget(explanationLabel);

        scrollArea = new QScrollArea(ResearcherWizardModeDetailPage);
        scrollArea->setObjectName("scrollArea");
        scrollArea->setWidgetResizable(true);
        scrollAreaWidgetContents = new QWidget();
        scrollAreaWidgetContents->setObjectName("scrollAreaWidgetContents");
        scrollAreaWidgetContents->setGeometry(QRect(0, 0, 610, 310));
        verticalLayout_2 = new QVBoxLayout(scrollAreaWidgetContents);
        verticalLayout_2->setObjectName("verticalLayout_2");
        comparisonHorizontalLayout = new QHBoxLayout();
        comparisonHorizontalLayout->setSpacing(0);
        comparisonHorizontalLayout->setObjectName("comparisonHorizontalLayout");
        labelsLayout = new QGridLayout();
        labelsLayout->setObjectName("labelsLayout");
        labelsLayout->setSizeConstraint(QLayout::SetDefaultConstraint);
        labelsLayout->setHorizontalSpacing(6);
        labelsLayout->setVerticalSpacing(12);
        labelsLayout->setContentsMargins(16, -1, -1, -1);
        blockRewardLabel = new QLabel(scrollAreaWidgetContents);
        blockRewardLabel->setObjectName("blockRewardLabel");

        labelsLayout->addWidget(blockRewardLabel, 2, 0, 1, 1, Qt::AlignVCenter);

        voteLabel = new QLabel(scrollAreaWidgetContents);
        voteLabel->setObjectName("voteLabel");

        labelsLayout->addWidget(voteLabel, 5, 0, 1, 1, Qt::AlignVCenter);

        decentralizedLabel = new QLabel(scrollAreaWidgetContents);
        decentralizedLabel->setObjectName("decentralizedLabel");

        labelsLayout->addWidget(decentralizedLabel, 4, 0, 1, 1, Qt::AlignVCenter);

        secureNetworkLabel = new QLabel(scrollAreaWidgetContents);
        secureNetworkLabel->setObjectName("secureNetworkLabel");

        labelsLayout->addWidget(secureNetworkLabel, 3, 0, 1, 1, Qt::AlignVCenter);

        keepRewardsLabel = new QLabel(scrollAreaWidgetContents);
        keepRewardsLabel->setObjectName("keepRewardsLabel");

        labelsLayout->addWidget(keepRewardsLabel, 6, 0, 1, 1, Qt::AlignVCenter);

        boincRewardsLabel = new QLabel(scrollAreaWidgetContents);
        boincRewardsLabel->setObjectName("boincRewardsLabel");

        labelsLayout->addWidget(boincRewardsLabel, 1, 0, 1, 1, Qt::AlignVCenter);

        upfrontInvestmentLabel = new QLabel(scrollAreaWidgetContents);
        upfrontInvestmentLabel->setObjectName("upfrontInvestmentLabel");

        labelsLayout->addWidget(upfrontInvestmentLabel, 7, 0, 1, 1, Qt::AlignVCenter);

        myChoiceLabel = new QLabel(scrollAreaWidgetContents);
        myChoiceLabel->setObjectName("myChoiceLabel");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(myChoiceLabel->sizePolicy().hasHeightForWidth());
        myChoiceLabel->setSizePolicy(sizePolicy1);
        myChoiceLabel->setMinimumSize(QSize(0, 21));
        myChoiceLabel->setStyleSheet(QString::fromUtf8("font-weight: bold;"));

        labelsLayout->addWidget(myChoiceLabel, 0, 0, 1, 1, Qt::AlignVCenter);

        boincLeaderboardsLabel = new QLabel(scrollAreaWidgetContents);
        boincLeaderboardsLabel->setObjectName("boincLeaderboardsLabel");

        labelsLayout->addWidget(boincLeaderboardsLabel, 8, 0, 1, 1, Qt::AlignVCenter);


        comparisonHorizontalLayout->addLayout(labelsLayout);

        modeLayout = new QGridLayout();
        modeLayout->setObjectName("modeLayout");
        modeLayout->setVerticalSpacing(12);
        modeLayout->setContentsMargins(-1, -1, 15, -1);
        soloBoincRewardsIconLabel = new ClickLabel(scrollAreaWidgetContents);
        soloBoincRewardsIconLabel->setObjectName("soloBoincRewardsIconLabel");
        soloBoincRewardsIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_green_check")));
        soloBoincRewardsIconLabel->setScaledContents(true);

        modeLayout->addWidget(soloBoincRewardsIconLabel, 2, 0, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        noncruncherDecentralizedIconLabel = new QLabel(scrollAreaWidgetContents);
        noncruncherDecentralizedIconLabel->setObjectName("noncruncherDecentralizedIconLabel");
        noncruncherDecentralizedIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_green_check")));
        noncruncherDecentralizedIconLabel->setScaledContents(true);

        modeLayout->addWidget(noncruncherDecentralizedIconLabel, 5, 2, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        noncruncherSecureNetworkIconLabel = new QLabel(scrollAreaWidgetContents);
        noncruncherSecureNetworkIconLabel->setObjectName("noncruncherSecureNetworkIconLabel");
        noncruncherSecureNetworkIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_green_check")));
        noncruncherSecureNetworkIconLabel->setScaledContents(true);

        modeLayout->addWidget(noncruncherSecureNetworkIconLabel, 4, 2, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        soloDecentralizedIconLabel = new QLabel(scrollAreaWidgetContents);
        soloDecentralizedIconLabel->setObjectName("soloDecentralizedIconLabel");
        soloDecentralizedIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_green_check")));
        soloDecentralizedIconLabel->setScaledContents(true);

        modeLayout->addWidget(soloDecentralizedIconLabel, 5, 0, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        soloBlockRewardIconLabel = new ClickLabel(scrollAreaWidgetContents);
        soloBlockRewardIconLabel->setObjectName("soloBlockRewardIconLabel");
        soloBlockRewardIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_green_check")));
        soloBlockRewardIconLabel->setScaledContents(true);

        modeLayout->addWidget(soloBlockRewardIconLabel, 3, 0, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        poolRadioButton = new QRadioButton(scrollAreaWidgetContents);
        modeButtonGroup = new QButtonGroup(ResearcherWizardModeDetailPage);
        modeButtonGroup->setObjectName("modeButtonGroup");
        modeButtonGroup->addButton(poolRadioButton);
        poolRadioButton->setObjectName("poolRadioButton");
        poolRadioButton->setStyleSheet(QString::fromUtf8("font-weight: bold;"));

        modeLayout->addWidget(poolRadioButton, 1, 1, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        soloVoteIconLabel = new QLabel(scrollAreaWidgetContents);
        soloVoteIconLabel->setObjectName("soloVoteIconLabel");
        soloVoteIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_green_check")));
        soloVoteIconLabel->setScaledContents(true);

        modeLayout->addWidget(soloVoteIconLabel, 6, 0, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        poolKeepRewardIconLabel = new QLabel(scrollAreaWidgetContents);
        poolKeepRewardIconLabel->setObjectName("poolKeepRewardIconLabel");
        poolKeepRewardIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_gray_x")));
        poolKeepRewardIconLabel->setScaledContents(true);

        modeLayout->addWidget(poolKeepRewardIconLabel, 7, 1, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        soloUpfrontInvestmentIconLabel = new QLabel(scrollAreaWidgetContents);
        soloUpfrontInvestmentIconLabel->setObjectName("soloUpfrontInvestmentIconLabel");
        soloUpfrontInvestmentIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_gray_x")));
        soloUpfrontInvestmentIconLabel->setScaledContents(true);

        modeLayout->addWidget(soloUpfrontInvestmentIconLabel, 8, 0, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        poolSecureNetworkIconLabel = new QLabel(scrollAreaWidgetContents);
        poolSecureNetworkIconLabel->setObjectName("poolSecureNetworkIconLabel");
        poolSecureNetworkIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_gray_x")));
        poolSecureNetworkIconLabel->setScaledContents(true);

        modeLayout->addWidget(poolSecureNetworkIconLabel, 4, 1, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        soloSecureNetworkIconLabel = new QLabel(scrollAreaWidgetContents);
        soloSecureNetworkIconLabel->setObjectName("soloSecureNetworkIconLabel");
        soloSecureNetworkIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_green_check")));
        soloSecureNetworkIconLabel->setScaledContents(true);

        modeLayout->addWidget(soloSecureNetworkIconLabel, 4, 0, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        poolDecentralizedIconLabel = new QLabel(scrollAreaWidgetContents);
        poolDecentralizedIconLabel->setObjectName("poolDecentralizedIconLabel");
        poolDecentralizedIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_gray_x")));
        poolDecentralizedIconLabel->setScaledContents(true);

        modeLayout->addWidget(poolDecentralizedIconLabel, 5, 1, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        soloRadioButton = new QRadioButton(scrollAreaWidgetContents);
        modeButtonGroup->addButton(soloRadioButton);
        soloRadioButton->setObjectName("soloRadioButton");
        soloRadioButton->setStyleSheet(QString::fromUtf8("font-weight: bold;"));

        modeLayout->addWidget(soloRadioButton, 1, 0, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        poolUpfrontInvestmentIconLabel = new QLabel(scrollAreaWidgetContents);
        poolUpfrontInvestmentIconLabel->setObjectName("poolUpfrontInvestmentIconLabel");
        poolUpfrontInvestmentIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_green_check")));
        poolUpfrontInvestmentIconLabel->setScaledContents(true);

        modeLayout->addWidget(poolUpfrontInvestmentIconLabel, 8, 1, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        noncruncherBoincRewardsIconLabel = new ClickLabel(scrollAreaWidgetContents);
        noncruncherBoincRewardsIconLabel->setObjectName("noncruncherBoincRewardsIconLabel");
        noncruncherBoincRewardsIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_gray_x")));
        noncruncherBoincRewardsIconLabel->setScaledContents(true);

        modeLayout->addWidget(noncruncherBoincRewardsIconLabel, 2, 2, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        poolVoteIconLabel = new QLabel(scrollAreaWidgetContents);
        poolVoteIconLabel->setObjectName("poolVoteIconLabel");
        poolVoteIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_gray_x")));
        poolVoteIconLabel->setScaledContents(true);

        modeLayout->addWidget(poolVoteIconLabel, 6, 1, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        noncruncherBlockRewardIconLabel = new QLabel(scrollAreaWidgetContents);
        noncruncherBlockRewardIconLabel->setObjectName("noncruncherBlockRewardIconLabel");
        noncruncherBlockRewardIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_green_check")));
        noncruncherBlockRewardIconLabel->setScaledContents(true);

        modeLayout->addWidget(noncruncherBlockRewardIconLabel, 3, 2, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        noncruncherKeepRewardIconLabel = new QLabel(scrollAreaWidgetContents);
        noncruncherKeepRewardIconLabel->setObjectName("noncruncherKeepRewardIconLabel");
        noncruncherKeepRewardIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_green_check")));
        noncruncherKeepRewardIconLabel->setScaledContents(true);

        modeLayout->addWidget(noncruncherKeepRewardIconLabel, 7, 2, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        noncruncherRadioButton = new QRadioButton(scrollAreaWidgetContents);
        modeButtonGroup->addButton(noncruncherRadioButton);
        noncruncherRadioButton->setObjectName("noncruncherRadioButton");
        noncruncherRadioButton->setStyleSheet(QString::fromUtf8("font-weight: bold;"));

        modeLayout->addWidget(noncruncherRadioButton, 1, 2, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        poolBlockRewardIconLabel = new QLabel(scrollAreaWidgetContents);
        poolBlockRewardIconLabel->setObjectName("poolBlockRewardIconLabel");
        poolBlockRewardIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_gray_x")));
        poolBlockRewardIconLabel->setScaledContents(true);

        modeLayout->addWidget(poolBlockRewardIconLabel, 3, 1, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        poolBoincRewardsIconLabel = new QLabel(scrollAreaWidgetContents);
        poolBoincRewardsIconLabel->setObjectName("poolBoincRewardsIconLabel");
        poolBoincRewardsIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_green_check")));
        poolBoincRewardsIconLabel->setScaledContents(true);

        modeLayout->addWidget(poolBoincRewardsIconLabel, 2, 1, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        noncruncherUpfrontInvestmentIconLabel = new QLabel(scrollAreaWidgetContents);
        noncruncherUpfrontInvestmentIconLabel->setObjectName("noncruncherUpfrontInvestmentIconLabel");
        noncruncherUpfrontInvestmentIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_gray_x")));
        noncruncherUpfrontInvestmentIconLabel->setScaledContents(true);

        modeLayout->addWidget(noncruncherUpfrontInvestmentIconLabel, 8, 2, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        noncruncherVoteIconLabel = new QLabel(scrollAreaWidgetContents);
        noncruncherVoteIconLabel->setObjectName("noncruncherVoteIconLabel");
        noncruncherVoteIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_green_check")));
        noncruncherVoteIconLabel->setScaledContents(true);

        modeLayout->addWidget(noncruncherVoteIconLabel, 6, 2, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        soloKeepRewardsIconLabel = new QLabel(scrollAreaWidgetContents);
        soloKeepRewardsIconLabel->setObjectName("soloKeepRewardsIconLabel");
        soloKeepRewardsIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_green_check")));
        soloKeepRewardsIconLabel->setScaledContents(true);

        modeLayout->addWidget(soloKeepRewardsIconLabel, 7, 0, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        soloBoincLeaderboardsIconLabel = new QLabel(scrollAreaWidgetContents);
        soloBoincLeaderboardsIconLabel->setObjectName("soloBoincLeaderboardsIconLabel");
        soloBoincLeaderboardsIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_green_check")));
        soloBoincLeaderboardsIconLabel->setScaledContents(true);

        modeLayout->addWidget(soloBoincLeaderboardsIconLabel, 9, 0, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        poolBoincLeaderboardsIconLabel = new QLabel(scrollAreaWidgetContents);
        poolBoincLeaderboardsIconLabel->setObjectName("poolBoincLeaderboardsIconLabel");

        modeLayout->addWidget(poolBoincLeaderboardsIconLabel, 9, 1, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        noncruncherBoincLeaderboardsIconLabel = new QLabel(scrollAreaWidgetContents);
        noncruncherBoincLeaderboardsIconLabel->setObjectName("noncruncherBoincLeaderboardsIconLabel");
        noncruncherBoincLeaderboardsIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_gray_x")));
        noncruncherBoincLeaderboardsIconLabel->setScaledContents(true);

        modeLayout->addWidget(noncruncherBoincLeaderboardsIconLabel, 9, 2, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);


        comparisonHorizontalLayout->addLayout(modeLayout);

        comparisonHorizontalLayout->setStretch(0, 1);
        comparisonHorizontalLayout->setStretch(1, 2);

        verticalLayout_2->addLayout(comparisonHorizontalLayout);

        scrollArea->setWidget(scrollAreaWidgetContents);

        verticalLayout->addWidget(scrollArea);


        retranslateUi(ResearcherWizardModeDetailPage);

        QMetaObject::connectSlotsByName(ResearcherWizardModeDetailPage);
    } // setupUi

    void retranslateUi(QWizardPage *ResearcherWizardModeDetailPage)
    {
        ResearcherWizardModeDetailPage->setWindowTitle(QCoreApplication::translate("ResearcherWizardModeDetailPage", "Select Researcher Mode", nullptr));
        ResearcherWizardModeDetailPage->setTitle(QString());
        ResearcherWizardModeDetailPage->setSubTitle(QString());
        titleLabel->setText(QCoreApplication::translate("ResearcherWizardModeDetailPage", "How can I participate?", nullptr));
        explanationLabel->setText(QCoreApplication::translate("ResearcherWizardModeDetailPage", "<html>\n"
"<head/>\n"
"<body>\n"
"<p>You can participate as either a miner or non-cruncher. <span style=\" font-weight:600;\">Miners</span> earn Gridcoin by participating in whitelisted BOINC projects. To redeem their rewards, miners must stake blocks. <span style=\" font-weight:600;\">Solo Miners</span> stake blocks on their own which typically requires a balance of at least 5000 GRC. <span style=\" font-weight:600;\">Pool Miners</span> avoid this upfront investment by letting a third party (the pool) stake blocks on their behalf. Pool mining is recommended for new users with a low initial balance. <span style=\" font-weight:600;\">Non-crunchers</span> own Gridcoin but do not participate in BOINC mining. By using their balance to stake blocks, non-crunchers help to secure the network and are rewarded 10 GRC per block.</p>\n"
"</body>\n"
"</html>", nullptr));
        blockRewardLabel->setText(QCoreApplication::translate("ResearcherWizardModeDetailPage", "Earn 10 GRC Block Reward", nullptr));
        voteLabel->setText(QCoreApplication::translate("ResearcherWizardModeDetailPage", "Ability to Vote", nullptr));
        decentralizedLabel->setText(QCoreApplication::translate("ResearcherWizardModeDetailPage", "Decentralized", nullptr));
        secureNetworkLabel->setText(QCoreApplication::translate("ResearcherWizardModeDetailPage", "Helps Secure Network", nullptr));
        keepRewardsLabel->setText(QCoreApplication::translate("ResearcherWizardModeDetailPage", "Keep 100% of Rewards", nullptr));
        boincRewardsLabel->setText(QCoreApplication::translate("ResearcherWizardModeDetailPage", "Earn BOINC Rewards", nullptr));
        upfrontInvestmentLabel->setText(QCoreApplication::translate("ResearcherWizardModeDetailPage", "No Upfront Investment", nullptr));
        myChoiceLabel->setText(QCoreApplication::translate("ResearcherWizardModeDetailPage", "My Choice:", nullptr));
        boincLeaderboardsLabel->setText(QCoreApplication::translate("ResearcherWizardModeDetailPage", "BOINC Leaderboards", nullptr));
        soloBoincRewardsIconLabel->setText(QString());
        noncruncherDecentralizedIconLabel->setText(QString());
        noncruncherSecureNetworkIconLabel->setText(QString());
        soloDecentralizedIconLabel->setText(QString());
        soloBlockRewardIconLabel->setText(QString());
        poolRadioButton->setText(QCoreApplication::translate("ResearcherWizardModeDetailPage", "Pool", nullptr));
        soloVoteIconLabel->setText(QString());
        poolKeepRewardIconLabel->setText(QString());
        soloUpfrontInvestmentIconLabel->setText(QString());
        poolSecureNetworkIconLabel->setText(QString());
        soloSecureNetworkIconLabel->setText(QString());
        poolDecentralizedIconLabel->setText(QString());
        soloRadioButton->setText(QCoreApplication::translate("ResearcherWizardModeDetailPage", "Solo", nullptr));
        poolUpfrontInvestmentIconLabel->setText(QString());
        noncruncherBoincRewardsIconLabel->setText(QString());
        poolVoteIconLabel->setText(QString());
        noncruncherBlockRewardIconLabel->setText(QString());
        noncruncherKeepRewardIconLabel->setText(QString());
        noncruncherRadioButton->setText(QCoreApplication::translate("ResearcherWizardModeDetailPage", "Non-cruncher", nullptr));
        poolBlockRewardIconLabel->setText(QString());
        poolBoincRewardsIconLabel->setText(QString());
        noncruncherUpfrontInvestmentIconLabel->setText(QString());
        noncruncherVoteIconLabel->setText(QString());
        soloKeepRewardsIconLabel->setText(QString());
        soloBoincLeaderboardsIconLabel->setText(QString());
        poolBoincLeaderboardsIconLabel->setText(QCoreApplication::translate("ResearcherWizardModeDetailPage", "Pool Only", nullptr));
        noncruncherBoincLeaderboardsIconLabel->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class ResearcherWizardModeDetailPage: public Ui_ResearcherWizardModeDetailPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_RESEARCHERWIZARDMODEDETAILPAGE_H
