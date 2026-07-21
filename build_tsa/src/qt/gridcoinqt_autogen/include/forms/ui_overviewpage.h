/********************************************************************************
** Form generated from reading UI file 'overviewpage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_OVERVIEWPAGE_H
#define UI_OVERVIEWPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListView>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "clicklabel.h"
#include "noresult.h"

QT_BEGIN_NAMESPACE

class Ui_OverviewPage
{
public:
    QVBoxLayout *overviewPageVerticalLayout;
    QFrame *headerFrame;
    QHBoxLayout *horizontalLayout_2;
    QWidget *headerTitleWrapper;
    QVBoxLayout *headerTitleVerticalLayout;
    QLabel *headerTitleLabel;
    QHBoxLayout *headerTitleHorizontalLayout;
    QLabel *cpidTextLabel;
    QLabel *cpidLabel;
    QSpacerItem *headerFrameSpacer;
    QWidget *headerMagnitudeWrapper;
    QVBoxLayout *headerMagnitudeVerticalLayout;
    QLabel *headerMagnitudeLabel;
    QLabel *headerMagnitudeCaptionLabel;
    QFrame *headerMagnitudeVLine;
    QWidget *headerBalanceWrapper;
    QVBoxLayout *headerBalanceVerticalLayout;
    QLabel *headerBalanceLabel;
    QLabel *headerBalanceCaptionLabel;
    QHBoxLayout *contentWrapperHorizontalLayout;
    QSpacerItem *leftContentSpacer;
    QFrame *contentFrame;
    QHBoxLayout *contentFrameHorizontalLayout;
    QWidget *leftColumn;
    QVBoxLayout *leftColumnVerticalLayout;
    QFrame *walletFrame;
    QVBoxLayout *walletVerticalLayout;
    QHBoxLayout *walletHeaderLayout;
    QLabel *overviewWalletLabel;
    QSpacerItem *horizontalSpacer;
    QLabel *walletStatusLabel;
    QFrame *walletHeaderLine;
    QGridLayout *walletGridLayout;
    QLabel *totalBalanceLabel;
    QLabel *balanceLabel;
    QLabel *unconfirmedLabel;
    QLabel *stakeLabel;
    QLabel *unconfirmedTextLabel;
    QLabel *totalLabel;
    QLabel *availableLabel;
    QLabel *immatureLabel;
    QLabel *stakeTextLabel;
    QLabel *immatureTextLabel;
    QFrame *stakingFrame;
    QVBoxLayout *stakingVerticalLayout;
    QHBoxLayout *stakingHeaderLayout;
    QLabel *stakingHeaderLabel;
    QSpacerItem *stakingHorizontalSpacer;
    QFrame *stakingHeaderLine;
    QGridLayout *stakingGridLayout;
    QLabel *blocksTextLabel;
    QLabel *blocksLabel;
    QLabel *difficultyTextLabel;
    QLabel *difficultyLabel;
    QLabel *netWeightTextLabel;
    QLabel *netWeightLabel;
    QLabel *coinWeightTextLabel;
    QLabel *coinWeightLabel;
    QFrame *researcherFrame;
    QVBoxLayout *researcherVerticalLayout;
    QHBoxLayout *researcherHeaderLayout;
    QLabel *researcherHeaderLabel;
    QSpacerItem *researcherHorizontalSpacer;
    QToolButton *mrcRequestToolButton;
    QLabel *researcherAlertLabel;
    QToolButton *researcherConfigToolButton;
    QFrame *researcherHeaderLine;
    QGridLayout *researcherGridLayout;
    QLabel *magnitudeTextLabel;
    QLabel *statusLabel;
    QLabel *magnitudeLabel;
    QLabel *accrualLabel;
    QLabel *statusTextLabel;
    QLabel *accrualTextLabel;
    QLabel *accrualLimitWarningIconlabel;
    QFrame *currentPollsFrame;
    QVBoxLayout *currentPollsVerticalLayout;
    QHBoxLayout *currentPollsHeaderLayout;
    QLabel *currentPollsHeaderLabel;
    QSpacerItem *currentPollsHorizontalSpacer;
    QFrame *currentPollsHeaderLine;
    ClickLabel *currentPollsTitleLabel;
    QFrame *recentTransactionsFrame;
    QVBoxLayout *recentTransactionsVerticalLayout;
    QHBoxLayout *horizontalLayout_3;
    QLabel *recentTransLabel;
    QSpacerItem *recentTransactionsHorizontalSpacer;
    QLabel *transactionsStatusLabel;
    QFrame *recentTransactionsHeaderLine;
    NoResult *recentTransactionsNoResult;
    QListView *listTransactions;
    QSpacerItem *rightContentSpacer;

    void setupUi(QWidget *OverviewPage)
    {
        if (OverviewPage->objectName().isEmpty())
            OverviewPage->setObjectName("OverviewPage");
        OverviewPage->resize(948, 755);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(OverviewPage->sizePolicy().hasHeightForWidth());
        OverviewPage->setSizePolicy(sizePolicy);
        OverviewPage->setMinimumSize(QSize(0, 0));
        OverviewPage->setMaximumSize(QSize(16777215, 16777215));
        overviewPageVerticalLayout = new QVBoxLayout(OverviewPage);
        overviewPageVerticalLayout->setSpacing(0);
        overviewPageVerticalLayout->setObjectName("overviewPageVerticalLayout");
        overviewPageVerticalLayout->setContentsMargins(0, 0, 0, 0);
        headerFrame = new QFrame(OverviewPage);
        headerFrame->setObjectName("headerFrame");
        headerFrame->setFrameShape(QFrame::NoFrame);
        headerFrame->setFrameShadow(QFrame::Plain);
        horizontalLayout_2 = new QHBoxLayout(headerFrame);
        horizontalLayout_2->setSpacing(15);
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        horizontalLayout_2->setContentsMargins(0, 0, 0, 0);
        headerTitleWrapper = new QWidget(headerFrame);
        headerTitleWrapper->setObjectName("headerTitleWrapper");
        headerTitleVerticalLayout = new QVBoxLayout(headerTitleWrapper);
        headerTitleVerticalLayout->setSpacing(4);
        headerTitleVerticalLayout->setObjectName("headerTitleVerticalLayout");
        headerTitleVerticalLayout->setContentsMargins(0, 0, 0, 0);
        headerTitleLabel = new QLabel(headerTitleWrapper);
        headerTitleLabel->setObjectName("headerTitleLabel");

        headerTitleVerticalLayout->addWidget(headerTitleLabel);

        headerTitleHorizontalLayout = new QHBoxLayout();
        headerTitleHorizontalLayout->setObjectName("headerTitleHorizontalLayout");
        cpidTextLabel = new QLabel(headerTitleWrapper);
        cpidTextLabel->setObjectName("cpidTextLabel");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Preferred);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(cpidTextLabel->sizePolicy().hasHeightForWidth());
        cpidTextLabel->setSizePolicy(sizePolicy1);

        headerTitleHorizontalLayout->addWidget(cpidTextLabel, 0, Qt::AlignLeft);

        cpidLabel = new QLabel(headerTitleWrapper);
        cpidLabel->setObjectName("cpidLabel");
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Preferred);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(cpidLabel->sizePolicy().hasHeightForWidth());
        cpidLabel->setSizePolicy(sizePolicy2);
        cpidLabel->setText(QString::fromUtf8(""));
        cpidLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        headerTitleHorizontalLayout->addWidget(cpidLabel);


        headerTitleVerticalLayout->addLayout(headerTitleHorizontalLayout);


        horizontalLayout_2->addWidget(headerTitleWrapper, 0, Qt::AlignVCenter);

        headerFrameSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_2->addItem(headerFrameSpacer);

        headerMagnitudeWrapper = new QWidget(headerFrame);
        headerMagnitudeWrapper->setObjectName("headerMagnitudeWrapper");
        headerMagnitudeVerticalLayout = new QVBoxLayout(headerMagnitudeWrapper);
        headerMagnitudeVerticalLayout->setSpacing(0);
        headerMagnitudeVerticalLayout->setObjectName("headerMagnitudeVerticalLayout");
        headerMagnitudeVerticalLayout->setContentsMargins(0, 0, 0, 0);
        headerMagnitudeLabel = new QLabel(headerMagnitudeWrapper);
        headerMagnitudeLabel->setObjectName("headerMagnitudeLabel");
        headerMagnitudeLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        headerMagnitudeVerticalLayout->addWidget(headerMagnitudeLabel);

        headerMagnitudeCaptionLabel = new QLabel(headerMagnitudeWrapper);
        headerMagnitudeCaptionLabel->setObjectName("headerMagnitudeCaptionLabel");
        headerMagnitudeCaptionLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        headerMagnitudeVerticalLayout->addWidget(headerMagnitudeCaptionLabel);


        horizontalLayout_2->addWidget(headerMagnitudeWrapper, 0, Qt::AlignVCenter);

        headerMagnitudeVLine = new QFrame(headerFrame);
        headerMagnitudeVLine->setObjectName("headerMagnitudeVLine");
        headerMagnitudeVLine->setFrameShape(QFrame::Shape::VLine);
        headerMagnitudeVLine->setFrameShadow(QFrame::Shadow::Sunken);

        horizontalLayout_2->addWidget(headerMagnitudeVLine);

        headerBalanceWrapper = new QWidget(headerFrame);
        headerBalanceWrapper->setObjectName("headerBalanceWrapper");
        headerBalanceVerticalLayout = new QVBoxLayout(headerBalanceWrapper);
        headerBalanceVerticalLayout->setSpacing(0);
        headerBalanceVerticalLayout->setObjectName("headerBalanceVerticalLayout");
        headerBalanceVerticalLayout->setContentsMargins(0, 0, 0, 0);
        headerBalanceLabel = new QLabel(headerBalanceWrapper);
        headerBalanceLabel->setObjectName("headerBalanceLabel");
        headerBalanceLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        headerBalanceVerticalLayout->addWidget(headerBalanceLabel);

        headerBalanceCaptionLabel = new QLabel(headerBalanceWrapper);
        headerBalanceCaptionLabel->setObjectName("headerBalanceCaptionLabel");
        headerBalanceCaptionLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        headerBalanceVerticalLayout->addWidget(headerBalanceCaptionLabel);


        horizontalLayout_2->addWidget(headerBalanceWrapper, 0, Qt::AlignVCenter);


        overviewPageVerticalLayout->addWidget(headerFrame, 0, Qt::AlignVCenter);

        contentWrapperHorizontalLayout = new QHBoxLayout();
        contentWrapperHorizontalLayout->setSpacing(0);
        contentWrapperHorizontalLayout->setObjectName("contentWrapperHorizontalLayout");
        leftContentSpacer = new QSpacerItem(0, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        contentWrapperHorizontalLayout->addItem(leftContentSpacer);

        contentFrame = new QFrame(OverviewPage);
        contentFrame->setObjectName("contentFrame");
        QSizePolicy sizePolicy3(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);
        sizePolicy3.setHorizontalStretch(1);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(contentFrame->sizePolicy().hasHeightForWidth());
        contentFrame->setSizePolicy(sizePolicy3);
        contentFrameHorizontalLayout = new QHBoxLayout(contentFrame);
        contentFrameHorizontalLayout->setSpacing(9);
        contentFrameHorizontalLayout->setObjectName("contentFrameHorizontalLayout");
        contentFrameHorizontalLayout->setContentsMargins(9, -1, 9, -1);
        leftColumn = new QWidget(contentFrame);
        leftColumn->setObjectName("leftColumn");
        leftColumnVerticalLayout = new QVBoxLayout(leftColumn);
        leftColumnVerticalLayout->setSpacing(9);
        leftColumnVerticalLayout->setObjectName("leftColumnVerticalLayout");
        leftColumnVerticalLayout->setContentsMargins(0, 0, 0, 0);
        walletFrame = new QFrame(leftColumn);
        walletFrame->setObjectName("walletFrame");
        walletFrame->setFrameShape(QFrame::StyledPanel);
        walletFrame->setFrameShadow(QFrame::Raised);
        walletVerticalLayout = new QVBoxLayout(walletFrame);
        walletVerticalLayout->setObjectName("walletVerticalLayout");
        walletVerticalLayout->setContentsMargins(0, 0, 0, 0);
        walletHeaderLayout = new QHBoxLayout();
        walletHeaderLayout->setSpacing(7);
        walletHeaderLayout->setObjectName("walletHeaderLayout");
        overviewWalletLabel = new QLabel(walletFrame);
        overviewWalletLabel->setObjectName("overviewWalletLabel");
        overviewWalletLabel->setTextFormat(Qt::PlainText);

        walletHeaderLayout->addWidget(overviewWalletLabel);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        walletHeaderLayout->addItem(horizontalSpacer);

        walletStatusLabel = new QLabel(walletFrame);
        walletStatusLabel->setObjectName("walletStatusLabel");
        walletStatusLabel->setText(QString::fromUtf8("Out of Sync"));
        walletStatusLabel->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);

        walletHeaderLayout->addWidget(walletStatusLabel, 0, Qt::AlignVCenter);


        walletVerticalLayout->addLayout(walletHeaderLayout);

        walletHeaderLine = new QFrame(walletFrame);
        walletHeaderLine->setObjectName("walletHeaderLine");
        walletHeaderLine->setFrameShape(QFrame::Shape::HLine);
        walletHeaderLine->setFrameShadow(QFrame::Shadow::Sunken);

        walletVerticalLayout->addWidget(walletHeaderLine);

        walletGridLayout = new QGridLayout();
        walletGridLayout->setSpacing(12);
        walletGridLayout->setObjectName("walletGridLayout");
        totalBalanceLabel = new QLabel(walletFrame);
        totalBalanceLabel->setObjectName("totalBalanceLabel");
        totalBalanceLabel->setProperty("isRowHeader", QVariant(true));

        walletGridLayout->addWidget(totalBalanceLabel, 4, 0, 1, 1);

        balanceLabel = new QLabel(walletFrame);
        balanceLabel->setObjectName("balanceLabel");
        sizePolicy2.setHeightForWidth(balanceLabel->sizePolicy().hasHeightForWidth());
        balanceLabel->setSizePolicy(sizePolicy2);
        balanceLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        balanceLabel->setText(QString::fromUtf8("0 GRC"));
        balanceLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        walletGridLayout->addWidget(balanceLabel, 0, 1, 1, 1);

        unconfirmedLabel = new QLabel(walletFrame);
        unconfirmedLabel->setObjectName("unconfirmedLabel");
        sizePolicy2.setHeightForWidth(unconfirmedLabel->sizePolicy().hasHeightForWidth());
        unconfirmedLabel->setSizePolicy(sizePolicy2);
        unconfirmedLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        unconfirmedLabel->setText(QString::fromUtf8("0 GRC"));
        unconfirmedLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        walletGridLayout->addWidget(unconfirmedLabel, 2, 1, 1, 1);

        stakeLabel = new QLabel(walletFrame);
        stakeLabel->setObjectName("stakeLabel");
        sizePolicy2.setHeightForWidth(stakeLabel->sizePolicy().hasHeightForWidth());
        stakeLabel->setSizePolicy(sizePolicy2);
        stakeLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        stakeLabel->setText(QString::fromUtf8("0 GRC"));
        stakeLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        walletGridLayout->addWidget(stakeLabel, 1, 1, 1, 1);

        unconfirmedTextLabel = new QLabel(walletFrame);
        unconfirmedTextLabel->setObjectName("unconfirmedTextLabel");
        unconfirmedTextLabel->setProperty("isRowHeader", QVariant(true));

        walletGridLayout->addWidget(unconfirmedTextLabel, 2, 0, 1, 1);

        totalLabel = new QLabel(walletFrame);
        totalLabel->setObjectName("totalLabel");
        sizePolicy2.setHeightForWidth(totalLabel->sizePolicy().hasHeightForWidth());
        totalLabel->setSizePolicy(sizePolicy2);
        totalLabel->setText(QString::fromUtf8("0 GRC"));
        totalLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        walletGridLayout->addWidget(totalLabel, 4, 1, 1, 1);

        availableLabel = new QLabel(walletFrame);
        availableLabel->setObjectName("availableLabel");
        availableLabel->setProperty("isRowHeader", QVariant(true));

        walletGridLayout->addWidget(availableLabel, 0, 0, 1, 1);

        immatureLabel = new QLabel(walletFrame);
        immatureLabel->setObjectName("immatureLabel");
        sizePolicy2.setHeightForWidth(immatureLabel->sizePolicy().hasHeightForWidth());
        immatureLabel->setSizePolicy(sizePolicy2);
        immatureLabel->setText(QString::fromUtf8("0 GRC"));
        immatureLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        walletGridLayout->addWidget(immatureLabel, 3, 1, 1, 1);

        stakeTextLabel = new QLabel(walletFrame);
        stakeTextLabel->setObjectName("stakeTextLabel");
        stakeTextLabel->setProperty("isRowHeader", QVariant(true));

        walletGridLayout->addWidget(stakeTextLabel, 1, 0, 1, 1);

        immatureTextLabel = new QLabel(walletFrame);
        immatureTextLabel->setObjectName("immatureTextLabel");
        immatureTextLabel->setProperty("isRowHeader", QVariant(true));

        walletGridLayout->addWidget(immatureTextLabel, 3, 0, 1, 1);


        walletVerticalLayout->addLayout(walletGridLayout);


        leftColumnVerticalLayout->addWidget(walletFrame, 0, Qt::AlignTop);

        stakingFrame = new QFrame(leftColumn);
        stakingFrame->setObjectName("stakingFrame");
        stakingFrame->setFrameShape(QFrame::StyledPanel);
        stakingFrame->setFrameShadow(QFrame::Raised);
        stakingVerticalLayout = new QVBoxLayout(stakingFrame);
        stakingVerticalLayout->setObjectName("stakingVerticalLayout");
        stakingVerticalLayout->setContentsMargins(0, 0, 0, 0);
        stakingHeaderLayout = new QHBoxLayout();
        stakingHeaderLayout->setSpacing(7);
        stakingHeaderLayout->setObjectName("stakingHeaderLayout");
        stakingHeaderLabel = new QLabel(stakingFrame);
        stakingHeaderLabel->setObjectName("stakingHeaderLabel");
        stakingHeaderLabel->setTextFormat(Qt::PlainText);

        stakingHeaderLayout->addWidget(stakingHeaderLabel);

        stakingHorizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        stakingHeaderLayout->addItem(stakingHorizontalSpacer);


        stakingVerticalLayout->addLayout(stakingHeaderLayout);

        stakingHeaderLine = new QFrame(stakingFrame);
        stakingHeaderLine->setObjectName("stakingHeaderLine");
        stakingHeaderLine->setFrameShape(QFrame::Shape::HLine);
        stakingHeaderLine->setFrameShadow(QFrame::Shadow::Sunken);

        stakingVerticalLayout->addWidget(stakingHeaderLine);

        stakingGridLayout = new QGridLayout();
        stakingGridLayout->setSpacing(12);
        stakingGridLayout->setObjectName("stakingGridLayout");
        blocksTextLabel = new QLabel(stakingFrame);
        blocksTextLabel->setObjectName("blocksTextLabel");
        blocksTextLabel->setProperty("isRowHeader", QVariant(true));

        stakingGridLayout->addWidget(blocksTextLabel, 0, 0, 1, 1);

        blocksLabel = new QLabel(stakingFrame);
        blocksLabel->setObjectName("blocksLabel");
        sizePolicy2.setHeightForWidth(blocksLabel->sizePolicy().hasHeightForWidth());
        blocksLabel->setSizePolicy(sizePolicy2);
        blocksLabel->setText(QString::fromUtf8(""));
        blocksLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        stakingGridLayout->addWidget(blocksLabel, 0, 1, 1, 1);

        difficultyTextLabel = new QLabel(stakingFrame);
        difficultyTextLabel->setObjectName("difficultyTextLabel");
        difficultyTextLabel->setProperty("isRowHeader", QVariant(true));

        stakingGridLayout->addWidget(difficultyTextLabel, 1, 0, 1, 1);

        difficultyLabel = new QLabel(stakingFrame);
        difficultyLabel->setObjectName("difficultyLabel");
        sizePolicy2.setHeightForWidth(difficultyLabel->sizePolicy().hasHeightForWidth());
        difficultyLabel->setSizePolicy(sizePolicy2);
        difficultyLabel->setText(QString::fromUtf8(""));
        difficultyLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        stakingGridLayout->addWidget(difficultyLabel, 1, 1, 1, 1);

        netWeightTextLabel = new QLabel(stakingFrame);
        netWeightTextLabel->setObjectName("netWeightTextLabel");
        netWeightTextLabel->setProperty("isRowHeader", QVariant(true));

        stakingGridLayout->addWidget(netWeightTextLabel, 2, 0, 1, 1);

        netWeightLabel = new QLabel(stakingFrame);
        netWeightLabel->setObjectName("netWeightLabel");
        sizePolicy2.setHeightForWidth(netWeightLabel->sizePolicy().hasHeightForWidth());
        netWeightLabel->setSizePolicy(sizePolicy2);
        netWeightLabel->setText(QString::fromUtf8(""));
        netWeightLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        stakingGridLayout->addWidget(netWeightLabel, 2, 1, 1, 1);

        coinWeightTextLabel = new QLabel(stakingFrame);
        coinWeightTextLabel->setObjectName("coinWeightTextLabel");
        coinWeightTextLabel->setProperty("isRowHeader", QVariant(true));

        stakingGridLayout->addWidget(coinWeightTextLabel, 3, 0, 1, 1);

        coinWeightLabel = new QLabel(stakingFrame);
        coinWeightLabel->setObjectName("coinWeightLabel");
        sizePolicy2.setHeightForWidth(coinWeightLabel->sizePolicy().hasHeightForWidth());
        coinWeightLabel->setSizePolicy(sizePolicy2);
        coinWeightLabel->setText(QString::fromUtf8(""));
        coinWeightLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        stakingGridLayout->addWidget(coinWeightLabel, 3, 1, 1, 1);


        stakingVerticalLayout->addLayout(stakingGridLayout);


        leftColumnVerticalLayout->addWidget(stakingFrame, 0, Qt::AlignTop);

        researcherFrame = new QFrame(leftColumn);
        researcherFrame->setObjectName("researcherFrame");
        researcherFrame->setFrameShape(QFrame::StyledPanel);
        researcherFrame->setFrameShadow(QFrame::Raised);
        researcherVerticalLayout = new QVBoxLayout(researcherFrame);
        researcherVerticalLayout->setObjectName("researcherVerticalLayout");
        researcherVerticalLayout->setContentsMargins(0, 0, 0, 0);
        researcherHeaderLayout = new QHBoxLayout();
        researcherHeaderLayout->setSpacing(7);
        researcherHeaderLayout->setObjectName("researcherHeaderLayout");
        researcherHeaderLabel = new QLabel(researcherFrame);
        researcherHeaderLabel->setObjectName("researcherHeaderLabel");
        researcherHeaderLabel->setTextFormat(Qt::PlainText);

        researcherHeaderLayout->addWidget(researcherHeaderLabel, 0, Qt::AlignVCenter);

        researcherHorizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        researcherHeaderLayout->addItem(researcherHorizontalSpacer);

        mrcRequestToolButton = new QToolButton(researcherFrame);
        mrcRequestToolButton->setObjectName("mrcRequestToolButton");

        researcherHeaderLayout->addWidget(mrcRequestToolButton);

        researcherAlertLabel = new QLabel(researcherFrame);
        researcherAlertLabel->setObjectName("researcherAlertLabel");

        researcherHeaderLayout->addWidget(researcherAlertLabel, 0, Qt::AlignVCenter);

        researcherConfigToolButton = new QToolButton(researcherFrame);
        researcherConfigToolButton->setObjectName("researcherConfigToolButton");
        researcherConfigToolButton->setProperty("actionNeeded", QVariant(false));

        researcherHeaderLayout->addWidget(researcherConfigToolButton, 0, Qt::AlignVCenter);


        researcherVerticalLayout->addLayout(researcherHeaderLayout);

        researcherHeaderLine = new QFrame(researcherFrame);
        researcherHeaderLine->setObjectName("researcherHeaderLine");
        researcherHeaderLine->setFrameShape(QFrame::Shape::HLine);
        researcherHeaderLine->setFrameShadow(QFrame::Shadow::Sunken);

        researcherVerticalLayout->addWidget(researcherHeaderLine);

        researcherGridLayout = new QGridLayout();
        researcherGridLayout->setSpacing(12);
        researcherGridLayout->setObjectName("researcherGridLayout");
        magnitudeTextLabel = new QLabel(researcherFrame);
        magnitudeTextLabel->setObjectName("magnitudeTextLabel");
        magnitudeTextLabel->setProperty("isRowHeader", QVariant(true));

        researcherGridLayout->addWidget(magnitudeTextLabel, 1, 0, 1, 1);

        statusLabel = new QLabel(researcherFrame);
        statusLabel->setObjectName("statusLabel");
        sizePolicy2.setHeightForWidth(statusLabel->sizePolicy().hasHeightForWidth());
        statusLabel->setSizePolicy(sizePolicy2);
        statusLabel->setText(QString::fromUtf8(""));
        statusLabel->setWordWrap(true);
        statusLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        researcherGridLayout->addWidget(statusLabel, 0, 1, 1, 1);

        magnitudeLabel = new QLabel(researcherFrame);
        magnitudeLabel->setObjectName("magnitudeLabel");
        sizePolicy2.setHeightForWidth(magnitudeLabel->sizePolicy().hasHeightForWidth());
        magnitudeLabel->setSizePolicy(sizePolicy2);
        magnitudeLabel->setText(QString::fromUtf8(""));
        magnitudeLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        researcherGridLayout->addWidget(magnitudeLabel, 1, 1, 1, 1);

        accrualLabel = new QLabel(researcherFrame);
        accrualLabel->setObjectName("accrualLabel");
        sizePolicy2.setHeightForWidth(accrualLabel->sizePolicy().hasHeightForWidth());
        accrualLabel->setSizePolicy(sizePolicy2);
        accrualLabel->setText(QString::fromUtf8(""));
        accrualLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        researcherGridLayout->addWidget(accrualLabel, 2, 1, 1, 1);

        statusTextLabel = new QLabel(researcherFrame);
        statusTextLabel->setObjectName("statusTextLabel");
        statusTextLabel->setProperty("isRowHeader", QVariant(true));

        researcherGridLayout->addWidget(statusTextLabel, 0, 0, 1, 1);

        accrualTextLabel = new QLabel(researcherFrame);
        accrualTextLabel->setObjectName("accrualTextLabel");
        accrualTextLabel->setProperty("isRowHeader", QVariant(true));

        researcherGridLayout->addWidget(accrualTextLabel, 2, 0, 1, 1);

        accrualLimitWarningIconlabel = new QLabel(researcherFrame);
        accrualLimitWarningIconlabel->setObjectName("accrualLimitWarningIconlabel");
        accrualLimitWarningIconlabel->setMaximumSize(QSize(16777215, 16777215));
        accrualLimitWarningIconlabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/warning")));
        accrualLimitWarningIconlabel->setScaledContents(true);

        researcherGridLayout->addWidget(accrualLimitWarningIconlabel, 2, 2, 1, 1);


        researcherVerticalLayout->addLayout(researcherGridLayout);


        leftColumnVerticalLayout->addWidget(researcherFrame, 0, Qt::AlignTop);

        currentPollsFrame = new QFrame(leftColumn);
        currentPollsFrame->setObjectName("currentPollsFrame");
        currentPollsFrame->setFrameShape(QFrame::StyledPanel);
        currentPollsFrame->setFrameShadow(QFrame::Raised);
        currentPollsVerticalLayout = new QVBoxLayout(currentPollsFrame);
        currentPollsVerticalLayout->setObjectName("currentPollsVerticalLayout");
        currentPollsVerticalLayout->setContentsMargins(0, 0, 0, 0);
        currentPollsHeaderLayout = new QHBoxLayout();
        currentPollsHeaderLayout->setSpacing(7);
        currentPollsHeaderLayout->setObjectName("currentPollsHeaderLayout");
        currentPollsHeaderLabel = new QLabel(currentPollsFrame);
        currentPollsHeaderLabel->setObjectName("currentPollsHeaderLabel");
        currentPollsHeaderLabel->setTextFormat(Qt::PlainText);

        currentPollsHeaderLayout->addWidget(currentPollsHeaderLabel, 0, Qt::AlignVCenter);

        currentPollsHorizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        currentPollsHeaderLayout->addItem(currentPollsHorizontalSpacer);


        currentPollsVerticalLayout->addLayout(currentPollsHeaderLayout);

        currentPollsHeaderLine = new QFrame(currentPollsFrame);
        currentPollsHeaderLine->setObjectName("currentPollsHeaderLine");
        currentPollsHeaderLine->setFrameShape(QFrame::Shape::HLine);
        currentPollsHeaderLine->setFrameShadow(QFrame::Shadow::Sunken);

        currentPollsVerticalLayout->addWidget(currentPollsHeaderLine);

        currentPollsTitleLabel = new ClickLabel(currentPollsFrame);
        currentPollsTitleLabel->setObjectName("currentPollsTitleLabel");
        currentPollsTitleLabel->setWordWrap(true);

        currentPollsVerticalLayout->addWidget(currentPollsTitleLabel);


        leftColumnVerticalLayout->addWidget(currentPollsFrame);


        contentFrameHorizontalLayout->addWidget(leftColumn, 0, Qt::AlignTop);

        recentTransactionsFrame = new QFrame(contentFrame);
        recentTransactionsFrame->setObjectName("recentTransactionsFrame");
        recentTransactionsVerticalLayout = new QVBoxLayout(recentTransactionsFrame);
        recentTransactionsVerticalLayout->setSpacing(6);
        recentTransactionsVerticalLayout->setObjectName("recentTransactionsVerticalLayout");
        recentTransactionsVerticalLayout->setContentsMargins(0, 0, 0, 0);
        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName("horizontalLayout_3");
        recentTransLabel = new QLabel(recentTransactionsFrame);
        recentTransLabel->setObjectName("recentTransLabel");
        sizePolicy.setHeightForWidth(recentTransLabel->sizePolicy().hasHeightForWidth());
        recentTransLabel->setSizePolicy(sizePolicy);
        recentTransLabel->setMinimumSize(QSize(0, 0));

        horizontalLayout_3->addWidget(recentTransLabel);

        recentTransactionsHorizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_3->addItem(recentTransactionsHorizontalSpacer);

        transactionsStatusLabel = new QLabel(recentTransactionsFrame);
        transactionsStatusLabel->setObjectName("transactionsStatusLabel");
        sizePolicy.setHeightForWidth(transactionsStatusLabel->sizePolicy().hasHeightForWidth());
        transactionsStatusLabel->setSizePolicy(sizePolicy);
        transactionsStatusLabel->setMinimumSize(QSize(0, 0));
        transactionsStatusLabel->setText(QString::fromUtf8("Out of Sync"));
        transactionsStatusLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        horizontalLayout_3->addWidget(transactionsStatusLabel, 0, Qt::AlignVCenter);


        recentTransactionsVerticalLayout->addLayout(horizontalLayout_3);

        recentTransactionsHeaderLine = new QFrame(recentTransactionsFrame);
        recentTransactionsHeaderLine->setObjectName("recentTransactionsHeaderLine");
        recentTransactionsHeaderLine->setFrameShape(QFrame::Shape::HLine);
        recentTransactionsHeaderLine->setFrameShadow(QFrame::Shadow::Sunken);

        recentTransactionsVerticalLayout->addWidget(recentTransactionsHeaderLine);

        recentTransactionsNoResult = new NoResult(recentTransactionsFrame);
        recentTransactionsNoResult->setObjectName("recentTransactionsNoResult");
        QSizePolicy sizePolicy4(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Expanding);
        sizePolicy4.setHorizontalStretch(0);
        sizePolicy4.setVerticalStretch(100);
        sizePolicy4.setHeightForWidth(recentTransactionsNoResult->sizePolicy().hasHeightForWidth());
        recentTransactionsNoResult->setSizePolicy(sizePolicy4);

        recentTransactionsVerticalLayout->addWidget(recentTransactionsNoResult);

        listTransactions = new QListView(recentTransactionsFrame);
        listTransactions->setObjectName("listTransactions");
        QSizePolicy sizePolicy5(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy5.setHorizontalStretch(1);
        sizePolicy5.setVerticalStretch(0);
        sizePolicy5.setHeightForWidth(listTransactions->sizePolicy().hasHeightForWidth());
        listTransactions->setSizePolicy(sizePolicy5);
        listTransactions->setMinimumSize(QSize(0, 0));
        listTransactions->setFrameShape(QFrame::NoFrame);
        listTransactions->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        listTransactions->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        listTransactions->setSelectionMode(QAbstractItemView::NoSelection);

        recentTransactionsVerticalLayout->addWidget(listTransactions);


        contentFrameHorizontalLayout->addWidget(recentTransactionsFrame);


        contentWrapperHorizontalLayout->addWidget(contentFrame);

        rightContentSpacer = new QSpacerItem(0, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        contentWrapperHorizontalLayout->addItem(rightContentSpacer);


        overviewPageVerticalLayout->addLayout(contentWrapperHorizontalLayout);


        retranslateUi(OverviewPage);

        QMetaObject::connectSlotsByName(OverviewPage);
    } // setupUi

    void retranslateUi(QWidget *OverviewPage)
    {
        OverviewPage->setWindowTitle(QCoreApplication::translate("OverviewPage", "Form", nullptr));
        headerTitleLabel->setText(QCoreApplication::translate("OverviewPage", "Account Overview", nullptr));
        cpidTextLabel->setText(QCoreApplication::translate("OverviewPage", "CPID", nullptr));
        headerMagnitudeLabel->setText(QCoreApplication::translate("OverviewPage", "0.00", nullptr));
        headerMagnitudeCaptionLabel->setText(QCoreApplication::translate("OverviewPage", "Magnitude", nullptr));
        headerBalanceLabel->setText(QCoreApplication::translate("OverviewPage", "0.00", nullptr));
        headerBalanceCaptionLabel->setText(QCoreApplication::translate("OverviewPage", "Available (GRC)", nullptr));
        overviewWalletLabel->setText(QCoreApplication::translate("OverviewPage", "Wallet", nullptr));
#if QT_CONFIG(tooltip)
        walletStatusLabel->setToolTip(QCoreApplication::translate("OverviewPage", "The displayed information may be out of date. Your wallet automatically synchronizes with the Gridcoin network after a connection is established, but this process has not completed yet.", nullptr));
#endif // QT_CONFIG(tooltip)
        totalBalanceLabel->setText(QCoreApplication::translate("OverviewPage", "Total:", nullptr));
#if QT_CONFIG(tooltip)
        balanceLabel->setToolTip(QCoreApplication::translate("OverviewPage", "Your current spendable balance", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(tooltip)
        unconfirmedLabel->setToolTip(QCoreApplication::translate("OverviewPage", "Total of transactions that have yet to be confirmed, and do not yet count toward the current balance", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(tooltip)
        stakeLabel->setToolTip(QCoreApplication::translate("OverviewPage", "Amount staked for a recent block that must wait for 110 confirmations to mature before you can spend it.", nullptr));
#endif // QT_CONFIG(tooltip)
        unconfirmedTextLabel->setText(QCoreApplication::translate("OverviewPage", "Unconfirmed:", nullptr));
#if QT_CONFIG(tooltip)
        totalLabel->setToolTip(QCoreApplication::translate("OverviewPage", "Your current total balance", nullptr));
#endif // QT_CONFIG(tooltip)
        availableLabel->setText(QCoreApplication::translate("OverviewPage", "Available:", nullptr));
#if QT_CONFIG(tooltip)
        immatureLabel->setToolTip(QCoreApplication::translate("OverviewPage", "Total mined coins that have not yet matured.", nullptr));
#endif // QT_CONFIG(tooltip)
        stakeTextLabel->setText(QCoreApplication::translate("OverviewPage", "Immature Stake:", nullptr));
        immatureTextLabel->setText(QCoreApplication::translate("OverviewPage", "Immature:", nullptr));
        stakingHeaderLabel->setText(QCoreApplication::translate("OverviewPage", "Staking", nullptr));
        blocksTextLabel->setText(QCoreApplication::translate("OverviewPage", "Blocks:", nullptr));
        difficultyTextLabel->setText(QCoreApplication::translate("OverviewPage", "Difficulty:", nullptr));
        netWeightTextLabel->setText(QCoreApplication::translate("OverviewPage", "Net Weight:", nullptr));
        coinWeightTextLabel->setText(QCoreApplication::translate("OverviewPage", "Coin Weight:", nullptr));
        researcherHeaderLabel->setText(QCoreApplication::translate("OverviewPage", "Researcher", nullptr));
#if QT_CONFIG(tooltip)
        mrcRequestToolButton->setToolTip(QCoreApplication::translate("OverviewPage", "Open the Manual Reward Claim (MRC) request page", nullptr));
#endif // QT_CONFIG(tooltip)
        mrcRequestToolButton->setText(QString());
        researcherAlertLabel->setText(QCoreApplication::translate("OverviewPage", "Action Needed", nullptr));
#if QT_CONFIG(tooltip)
        researcherConfigToolButton->setToolTip(QCoreApplication::translate("OverviewPage", "Open the researcher/beacon configuration wizard.", nullptr));
#endif // QT_CONFIG(tooltip)
        magnitudeTextLabel->setText(QCoreApplication::translate("OverviewPage", "Magnitude:", nullptr));
        statusTextLabel->setText(QCoreApplication::translate("OverviewPage", "Status:", nullptr));
        accrualTextLabel->setText(QCoreApplication::translate("OverviewPage", "Pending Reward:", nullptr));
#if QT_CONFIG(tooltip)
        accrualLimitWarningIconlabel->setToolTip(QCoreApplication::translate("OverviewPage", "You are approaching the accrual limit. If you have a relatively low balance, you should request payment via MRC so that you do not lose earned rewards.", nullptr));
#endif // QT_CONFIG(tooltip)
        accrualLimitWarningIconlabel->setText(QString());
        currentPollsHeaderLabel->setText(QCoreApplication::translate("OverviewPage", "Current Polls", nullptr));
        recentTransLabel->setText(QCoreApplication::translate("OverviewPage", "Recent Transactions", nullptr));
#if QT_CONFIG(tooltip)
        transactionsStatusLabel->setToolTip(QCoreApplication::translate("OverviewPage", "The displayed information may be out of date. Your wallet automatically synchronizes with the Gridcoin network after a connection is established, but this process has not completed yet.", nullptr));
#endif // QT_CONFIG(tooltip)
    } // retranslateUi

};

namespace Ui {
    class OverviewPage: public Ui_OverviewPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_OVERVIEWPAGE_H
