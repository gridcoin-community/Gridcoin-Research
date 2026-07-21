/********************************************************************************
** Form generated from reading UI file 'rpcconsole.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_RPCCONSOLE_H
#define UI_RPCCONSOLE_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "trafficgraphwidget.h"

QT_BEGIN_NAMESPACE

class Ui_RPCConsole
{
public:
    QVBoxLayout *verticalLayout_2;
    QTabWidget *tabWidget;
    QWidget *tab_info;
    QGridLayout *gridLayout;
    QLabel *isTestNetLabel;
    QLabel *boostVersion;
    QLabel *diff;
    QPushButton *showCLOptionsButton;
    QLabel *openSSLVersionLabel;
    QLabel *qtVersion;
    QLabel *showCLOptionsLabel;
    QLabel *numberOfConnectionsLabel;
    QSpacerItem *verticalSpacer_2;
    QLabel *numberOfBlocks;
    QLabel *numberOfConnections;
    QLabel *clientVersionLabel;
    QLabel *totalBlocks;
    QSpacerItem *verticalSpacer;
    QLabel *gridcoinCoreLabel;
    QLabel *clientName;
    QLabel *clientVersion;
    QLabel *clientNameLabel;
    QLabel *qtVersionLabel;
    QLabel *lastBlockTime;
    QLabel *openDebugLogfileLabel;
    QLabel *numberOfBlocksLabel;
    QLabel *diffLabel;
    QLabel *lastBlockTimeLabel;
    QLabel *networkLabel;
    QLabel *startupTimeLabel;
    QLabel *openSSLVersion;
    QLabel *boostVersionLabel;
    QLabel *blockchainLabel;
    QSpacerItem *verticalSpacer_3;
    QLabel *startupTime;
    QLabel *totalBlocksLabel;
    QPushButton *openDebugLogfileButton;
    QLabel *isTestNet;
    QWidget *tab_network;
    QHBoxLayout *horizontalLayout_3;
    QVBoxLayout *verticalLayout_4;
    TrafficGraphWidget *trafficGraph;
    QHBoxLayout *horizontalLayout_2;
    QSlider *graphRangeSlider;
    QLabel *graphRangeLabel;
    QPushButton *clearTrafficGraphButton;
    QVBoxLayout *verticalLayout;
    QGroupBox *groupBox;
    QVBoxLayout *verticalLayout_5;
    QHBoxLayout *horizontalLayout_4;
    QFrame *greenLine;
    QLabel *bytesInTextLabel;
    QLabel *bytesInLabel;
    QHBoxLayout *horizontalLayout_5;
    QFrame *redLine;
    QLabel *bytesOutTextLabel;
    QLabel *bytesOutLabel;
    QSpacerItem *verticalSpacer_4;
    QWidget *tab_peers;
    QVBoxLayout *verticalLayout_10;
    QSplitter *splitter;
    QWidget *peerandbanWidget;
    QVBoxLayout *verticalLayout_7;
    QTableView *peerWidget;
    QLabel *banHeading;
    QTableView *banlistWidget;
    QWidget *peerDetailWidget;
    QVBoxLayout *verticalLayout_8;
    QLabel *peerHeading;
    QGridLayout *peerDetailsGrid;
    QLabel *peerWhitelistedLabel;
    QLabel *peerWhitelisted;
    QLabel *peerDirectionLabel;
    QLabel *peerDirection;
    QLabel *peerVersionLabel;
    QLabel *peerVersion;
    QLabel *peerSubversionLabel;
    QLabel *peerSubversion;
    QLabel *peerServicesLabel;
    QLabel *peerServices;
    QLabel *peerHeightLabel;
    QLabel *peerHeight;
    QLabel *peerSyncHeightLabel;
    QLabel *peerSyncHeight;
    QLabel *peerCommonHeightLabel;
    QLabel *peerCommonHeight;
    QLabel *peerBanScoreLabel;
    QLabel *peerBanScore;
    QLabel *peerConnTimeLavel;
    QLabel *peerConnTime;
    QLabel *lastSendLabel;
    QLabel *peerLastSend;
    QLabel *peerLastRecvLabel;
    QLabel *peerLastRecv;
    QLabel *peerBytesSentLabel;
    QLabel *peerBytesSent;
    QLabel *peerBytesRecvLabel;
    QLabel *peerBytesRecv;
    QLabel *peerPingTimeLabel;
    QLabel *peerPingTime;
    QLabel *peerPingWaitLabel;
    QLabel *peerPingWait;
    QLabel *peerMinPingLabel;
    QLabel *peerMinPing;
    QLabel *timeoffsetLabel;
    QLabel *timeoffset;
    QSpacerItem *verticalSpacer_5;
    QWidget *tab_console;
    QVBoxLayout *verticalLayout_9;
    QTextEdit *messagesWidget;
    QHBoxLayout *horizontalLayout;
    QLabel *label;
    QLineEdit *lineEdit;
    QPushButton *clearButton;
    QWidget *tab_scraper;
    QVBoxLayout *verticalLayout_6;
    QPlainTextEdit *scraper_log;

    void setupUi(QDialog *RPCConsole)
    {
        if (RPCConsole->objectName().isEmpty())
            RPCConsole->setObjectName("RPCConsole");
        RPCConsole->resize(960, 700);
        RPCConsole->setAutoFillBackground(false);
        verticalLayout_2 = new QVBoxLayout(RPCConsole);
        verticalLayout_2->setObjectName("verticalLayout_2");
        tabWidget = new QTabWidget(RPCConsole);
        tabWidget->setObjectName("tabWidget");
        tabWidget->setAutoFillBackground(false);
        tab_info = new QWidget();
        tab_info->setObjectName("tab_info");
        gridLayout = new QGridLayout(tab_info);
        gridLayout->setObjectName("gridLayout");
        gridLayout->setHorizontalSpacing(12);
        isTestNetLabel = new QLabel(tab_info);
        isTestNetLabel->setObjectName("isTestNetLabel");

        gridLayout->addWidget(isTestNetLabel, 42, 0, 1, 1);

        boostVersion = new QLabel(tab_info);
        boostVersion->setObjectName("boostVersion");

        gridLayout->addWidget(boostVersion, 4, 2, 1, 1);

        diff = new QLabel(tab_info);
        diff->setObjectName("diff");

        gridLayout->addWidget(diff, 37, 2, 1, 1);

        showCLOptionsButton = new QPushButton(tab_info);
        showCLOptionsButton->setObjectName("showCLOptionsButton");
        showCLOptionsButton->setFocusPolicy(Qt::TabFocus);
        showCLOptionsButton->setAutoFillBackground(false);
        showCLOptionsButton->setAutoDefault(false);

        gridLayout->addWidget(showCLOptionsButton, 51, 0, 1, 1);

        openSSLVersionLabel = new QLabel(tab_info);
        openSSLVersionLabel->setObjectName("openSSLVersionLabel");
        openSSLVersionLabel->setIndent(-1);

        gridLayout->addWidget(openSSLVersionLabel, 3, 0, 1, 1);

        qtVersion = new QLabel(tab_info);
        qtVersion->setObjectName("qtVersion");

        gridLayout->addWidget(qtVersion, 7, 2, 1, 1);

        showCLOptionsLabel = new QLabel(tab_info);
        showCLOptionsLabel->setObjectName("showCLOptionsLabel");

        gridLayout->addWidget(showCLOptionsLabel, 50, 0, 1, 1);

        numberOfConnectionsLabel = new QLabel(tab_info);
        numberOfConnectionsLabel->setObjectName("numberOfConnectionsLabel");
        numberOfConnectionsLabel->setAutoFillBackground(false);

        gridLayout->addWidget(numberOfConnectionsLabel, 41, 0, 1, 1);

        verticalSpacer_2 = new QSpacerItem(20, 20, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        gridLayout->addItem(verticalSpacer_2, 47, 0, 1, 1);

        numberOfBlocks = new QLabel(tab_info);
        numberOfBlocks->setObjectName("numberOfBlocks");
        numberOfBlocks->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        numberOfBlocks->setTextFormat(Qt::PlainText);
        numberOfBlocks->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        gridLayout->addWidget(numberOfBlocks, 44, 2, 1, 1);

        numberOfConnections = new QLabel(tab_info);
        numberOfConnections->setObjectName("numberOfConnections");
        numberOfConnections->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        numberOfConnections->setTextFormat(Qt::PlainText);
        numberOfConnections->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        gridLayout->addWidget(numberOfConnections, 41, 2, 1, 1);

        clientVersionLabel = new QLabel(tab_info);
        clientVersionLabel->setObjectName("clientVersionLabel");

        gridLayout->addWidget(clientVersionLabel, 2, 0, 1, 1);

        totalBlocks = new QLabel(tab_info);
        totalBlocks->setObjectName("totalBlocks");
        totalBlocks->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        totalBlocks->setTextFormat(Qt::PlainText);
        totalBlocks->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        gridLayout->addWidget(totalBlocks, 45, 2, 1, 1);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        gridLayout->addItem(verticalSpacer, 52, 0, 1, 1);

        gridcoinCoreLabel = new QLabel(tab_info);
        gridcoinCoreLabel->setObjectName("gridcoinCoreLabel");

        gridLayout->addWidget(gridcoinCoreLabel, 0, 0, 1, 1);

        clientName = new QLabel(tab_info);
        clientName->setObjectName("clientName");
        clientName->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        clientName->setTextFormat(Qt::PlainText);
        clientName->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        gridLayout->addWidget(clientName, 1, 2, 1, 1);

        clientVersion = new QLabel(tab_info);
        clientVersion->setObjectName("clientVersion");
        clientVersion->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        clientVersion->setTextFormat(Qt::PlainText);
        clientVersion->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        gridLayout->addWidget(clientVersion, 2, 2, 1, 1);

        clientNameLabel = new QLabel(tab_info);
        clientNameLabel->setObjectName("clientNameLabel");

        gridLayout->addWidget(clientNameLabel, 1, 0, 1, 1);

        qtVersionLabel = new QLabel(tab_info);
        qtVersionLabel->setObjectName("qtVersionLabel");

        gridLayout->addWidget(qtVersionLabel, 7, 0, 1, 1);

        lastBlockTime = new QLabel(tab_info);
        lastBlockTime->setObjectName("lastBlockTime");
        lastBlockTime->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        lastBlockTime->setTextFormat(Qt::PlainText);
        lastBlockTime->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        gridLayout->addWidget(lastBlockTime, 46, 2, 1, 1);

        openDebugLogfileLabel = new QLabel(tab_info);
        openDebugLogfileLabel->setObjectName("openDebugLogfileLabel");

        gridLayout->addWidget(openDebugLogfileLabel, 48, 0, 1, 1);

        numberOfBlocksLabel = new QLabel(tab_info);
        numberOfBlocksLabel->setObjectName("numberOfBlocksLabel");

        gridLayout->addWidget(numberOfBlocksLabel, 44, 0, 1, 1);

        diffLabel = new QLabel(tab_info);
        diffLabel->setObjectName("diffLabel");

        gridLayout->addWidget(diffLabel, 37, 0, 1, 1);

        lastBlockTimeLabel = new QLabel(tab_info);
        lastBlockTimeLabel->setObjectName("lastBlockTimeLabel");

        gridLayout->addWidget(lastBlockTimeLabel, 46, 0, 1, 1);

        networkLabel = new QLabel(tab_info);
        networkLabel->setObjectName("networkLabel");

        gridLayout->addWidget(networkLabel, 39, 0, 1, 1);

        startupTimeLabel = new QLabel(tab_info);
        startupTimeLabel->setObjectName("startupTimeLabel");

        gridLayout->addWidget(startupTimeLabel, 9, 0, 1, 1);

        openSSLVersion = new QLabel(tab_info);
        openSSLVersion->setObjectName("openSSLVersion");
        openSSLVersion->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        openSSLVersion->setTextFormat(Qt::PlainText);
        openSSLVersion->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        gridLayout->addWidget(openSSLVersion, 3, 2, 1, 1);

        boostVersionLabel = new QLabel(tab_info);
        boostVersionLabel->setObjectName("boostVersionLabel");

        gridLayout->addWidget(boostVersionLabel, 4, 0, 1, 1);

        blockchainLabel = new QLabel(tab_info);
        blockchainLabel->setObjectName("blockchainLabel");

        gridLayout->addWidget(blockchainLabel, 43, 0, 1, 1);

        verticalSpacer_3 = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        gridLayout->addItem(verticalSpacer_3, 37, 0, 1, 1);

        startupTime = new QLabel(tab_info);
        startupTime->setObjectName("startupTime");
        startupTime->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        startupTime->setTextFormat(Qt::PlainText);
        startupTime->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        gridLayout->addWidget(startupTime, 9, 2, 1, 1);

        totalBlocksLabel = new QLabel(tab_info);
        totalBlocksLabel->setObjectName("totalBlocksLabel");

        gridLayout->addWidget(totalBlocksLabel, 45, 0, 1, 1);

        openDebugLogfileButton = new QPushButton(tab_info);
        openDebugLogfileButton->setObjectName("openDebugLogfileButton");
        openDebugLogfileButton->setFocusPolicy(Qt::TabFocus);
        openDebugLogfileButton->setAutoFillBackground(false);
        openDebugLogfileButton->setAutoDefault(false);

        gridLayout->addWidget(openDebugLogfileButton, 49, 0, 1, 1);

        isTestNet = new QLabel(tab_info);
        isTestNet->setObjectName("isTestNet");
        isTestNet->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        isTestNet->setTextFormat(Qt::PlainText);
        isTestNet->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        gridLayout->addWidget(isTestNet, 42, 2, 1, 1);

        tabWidget->addTab(tab_info, QString());
        tab_network = new QWidget();
        tab_network->setObjectName("tab_network");
        horizontalLayout_3 = new QHBoxLayout(tab_network);
        horizontalLayout_3->setObjectName("horizontalLayout_3");
        verticalLayout_4 = new QVBoxLayout();
        verticalLayout_4->setObjectName("verticalLayout_4");
        trafficGraph = new TrafficGraphWidget(tab_network);
        trafficGraph->setObjectName("trafficGraph");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(trafficGraph->sizePolicy().hasHeightForWidth());
        trafficGraph->setSizePolicy(sizePolicy);

        verticalLayout_4->addWidget(trafficGraph);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        graphRangeSlider = new QSlider(tab_network);
        graphRangeSlider->setObjectName("graphRangeSlider");
        graphRangeSlider->setMinimum(1);
        graphRangeSlider->setMaximum(288);
        graphRangeSlider->setPageStep(12);
        graphRangeSlider->setValue(6);
        graphRangeSlider->setOrientation(Qt::Horizontal);

        horizontalLayout_2->addWidget(graphRangeSlider);

        graphRangeLabel = new QLabel(tab_network);
        graphRangeLabel->setObjectName("graphRangeLabel");
        graphRangeLabel->setMinimumSize(QSize(100, 0));
        graphRangeLabel->setAlignment(Qt::AlignCenter);

        horizontalLayout_2->addWidget(graphRangeLabel);

        clearTrafficGraphButton = new QPushButton(tab_network);
        clearTrafficGraphButton->setObjectName("clearTrafficGraphButton");

        horizontalLayout_2->addWidget(clearTrafficGraphButton);


        verticalLayout_4->addLayout(horizontalLayout_2);


        horizontalLayout_3->addLayout(verticalLayout_4);

        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName("verticalLayout");
        groupBox = new QGroupBox(tab_network);
        groupBox->setObjectName("groupBox");
        verticalLayout_5 = new QVBoxLayout(groupBox);
        verticalLayout_5->setObjectName("verticalLayout_5");
        horizontalLayout_4 = new QHBoxLayout();
        horizontalLayout_4->setObjectName("horizontalLayout_4");
        greenLine = new QFrame(groupBox);
        greenLine->setObjectName("greenLine");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(greenLine->sizePolicy().hasHeightForWidth());
        greenLine->setSizePolicy(sizePolicy1);
        greenLine->setMinimumSize(QSize(10, 0));
        QPalette palette;
        QBrush brush(QColor(0, 255, 0, 255));
        brush.setStyle(Qt::BrushStyle::SolidPattern);
        palette.setBrush(QPalette::ColorGroup::Active, QPalette::ColorRole::Light, brush);
        palette.setBrush(QPalette::ColorGroup::Inactive, QPalette::ColorRole::Light, brush);
        palette.setBrush(QPalette::ColorGroup::Disabled, QPalette::ColorRole::Light, brush);
        greenLine->setPalette(palette);
        greenLine->setFrameShape(QFrame::Shape::HLine);
        greenLine->setFrameShadow(QFrame::Shadow::Sunken);

        horizontalLayout_4->addWidget(greenLine);

        bytesInTextLabel = new QLabel(groupBox);
        bytesInTextLabel->setObjectName("bytesInTextLabel");

        horizontalLayout_4->addWidget(bytesInTextLabel);

        bytesInLabel = new QLabel(groupBox);
        bytesInLabel->setObjectName("bytesInLabel");
        bytesInLabel->setMinimumSize(QSize(50, 0));
        bytesInLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        horizontalLayout_4->addWidget(bytesInLabel);


        verticalLayout_5->addLayout(horizontalLayout_4);

        horizontalLayout_5 = new QHBoxLayout();
        horizontalLayout_5->setObjectName("horizontalLayout_5");
        redLine = new QFrame(groupBox);
        redLine->setObjectName("redLine");
        sizePolicy1.setHeightForWidth(redLine->sizePolicy().hasHeightForWidth());
        redLine->setSizePolicy(sizePolicy1);
        redLine->setMinimumSize(QSize(10, 0));
        QPalette palette1;
        QBrush brush1(QColor(255, 0, 0, 255));
        brush1.setStyle(Qt::BrushStyle::SolidPattern);
        palette1.setBrush(QPalette::ColorGroup::Active, QPalette::ColorRole::Light, brush1);
        palette1.setBrush(QPalette::ColorGroup::Inactive, QPalette::ColorRole::Light, brush1);
        palette1.setBrush(QPalette::ColorGroup::Disabled, QPalette::ColorRole::Light, brush1);
        redLine->setPalette(palette1);
        redLine->setFrameShape(QFrame::Shape::HLine);
        redLine->setFrameShadow(QFrame::Shadow::Sunken);

        horizontalLayout_5->addWidget(redLine);

        bytesOutTextLabel = new QLabel(groupBox);
        bytesOutTextLabel->setObjectName("bytesOutTextLabel");

        horizontalLayout_5->addWidget(bytesOutTextLabel);

        bytesOutLabel = new QLabel(groupBox);
        bytesOutLabel->setObjectName("bytesOutLabel");
        bytesOutLabel->setMinimumSize(QSize(50, 0));
        bytesOutLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        horizontalLayout_5->addWidget(bytesOutLabel);


        verticalLayout_5->addLayout(horizontalLayout_5);

        verticalSpacer_4 = new QSpacerItem(20, 407, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_5->addItem(verticalSpacer_4);


        verticalLayout->addWidget(groupBox);


        horizontalLayout_3->addLayout(verticalLayout);

        tabWidget->addTab(tab_network, QString());
        tab_peers = new QWidget();
        tab_peers->setObjectName("tab_peers");
        verticalLayout_10 = new QVBoxLayout(tab_peers);
        verticalLayout_10->setObjectName("verticalLayout_10");
        splitter = new QSplitter(tab_peers);
        splitter->setObjectName("splitter");
        splitter->setOrientation(Qt::Horizontal);
        splitter->setChildrenCollapsible(false);
        peerandbanWidget = new QWidget(splitter);
        peerandbanWidget->setObjectName("peerandbanWidget");
        QSizePolicy sizePolicy2(QSizePolicy::Policy::MinimumExpanding, QSizePolicy::Policy::Preferred);
        sizePolicy2.setHorizontalStretch(1);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(peerandbanWidget->sizePolicy().hasHeightForWidth());
        peerandbanWidget->setSizePolicy(sizePolicy2);
        peerandbanWidget->setMinimumSize(QSize(400, 0));
        verticalLayout_7 = new QVBoxLayout(peerandbanWidget);
        verticalLayout_7->setObjectName("verticalLayout_7");
        peerWidget = new QTableView(peerandbanWidget);
        peerWidget->setObjectName("peerWidget");
        peerWidget->setTabKeyNavigation(false);
        peerWidget->setTextElideMode(Qt::ElideMiddle);
        peerWidget->setAlternatingRowColors(true);
        peerWidget->setShowGrid(false);
        peerWidget->setSortingEnabled(true);
        peerWidget->setWordWrap(false);
        peerWidget->horizontalHeader()->setHighlightSections(false);

        verticalLayout_7->addWidget(peerWidget);

        banHeading = new QLabel(peerandbanWidget);
        banHeading->setObjectName("banHeading");
        QSizePolicy sizePolicy3(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Minimum);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(banHeading->sizePolicy().hasHeightForWidth());
        banHeading->setSizePolicy(sizePolicy3);
        banHeading->setMinimumSize(QSize(0, 32));
        banHeading->setMaximumSize(QSize(16777215, 32));
        banHeading->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        banHeading->setAlignment(Qt::AlignBottom|Qt::AlignLeading|Qt::AlignLeft);
        banHeading->setWordWrap(true);
        banHeading->setTextInteractionFlags(Qt::NoTextInteraction);

        verticalLayout_7->addWidget(banHeading);

        banlistWidget = new QTableView(peerandbanWidget);
        banlistWidget->setObjectName("banlistWidget");
        banlistWidget->setTabKeyNavigation(false);
        banlistWidget->setAlternatingRowColors(true);
        banlistWidget->setShowGrid(false);
        banlistWidget->setSortingEnabled(true);
        banlistWidget->horizontalHeader()->setHighlightSections(false);

        verticalLayout_7->addWidget(banlistWidget);

        splitter->addWidget(peerandbanWidget);
        peerDetailWidget = new QWidget(splitter);
        peerDetailWidget->setObjectName("peerDetailWidget");
        QSizePolicy sizePolicy4(QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Preferred);
        sizePolicy4.setHorizontalStretch(0);
        sizePolicy4.setVerticalStretch(0);
        sizePolicy4.setHeightForWidth(peerDetailWidget->sizePolicy().hasHeightForWidth());
        peerDetailWidget->setSizePolicy(sizePolicy4);
        peerDetailWidget->setMinimumSize(QSize(300, 0));
        verticalLayout_8 = new QVBoxLayout(peerDetailWidget);
        verticalLayout_8->setObjectName("verticalLayout_8");
        peerHeading = new QLabel(peerDetailWidget);
        peerHeading->setObjectName("peerHeading");
        peerHeading->setEnabled(true);
        QSizePolicy sizePolicy5(QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Minimum);
        sizePolicy5.setHorizontalStretch(0);
        sizePolicy5.setVerticalStretch(0);
        sizePolicy5.setHeightForWidth(peerHeading->sizePolicy().hasHeightForWidth());
        peerHeading->setSizePolicy(sizePolicy5);
        peerHeading->setMinimumSize(QSize(0, 0));
        peerHeading->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        peerHeading->setAlignment(Qt::AlignHCenter|Qt::AlignTop);
        peerHeading->setWordWrap(true);
        peerHeading->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        verticalLayout_8->addWidget(peerHeading);

        peerDetailsGrid = new QGridLayout();
        peerDetailsGrid->setObjectName("peerDetailsGrid");
        peerWhitelistedLabel = new QLabel(peerDetailWidget);
        peerWhitelistedLabel->setObjectName("peerWhitelistedLabel");

        peerDetailsGrid->addWidget(peerWhitelistedLabel, 0, 0, 1, 1);

        peerWhitelisted = new QLabel(peerDetailWidget);
        peerWhitelisted->setObjectName("peerWhitelisted");
        peerWhitelisted->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        peerWhitelisted->setTextFormat(Qt::PlainText);
        peerWhitelisted->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        peerDetailsGrid->addWidget(peerWhitelisted, 0, 1, 1, 1);

        peerDirectionLabel = new QLabel(peerDetailWidget);
        peerDirectionLabel->setObjectName("peerDirectionLabel");

        peerDetailsGrid->addWidget(peerDirectionLabel, 1, 0, 1, 1);

        peerDirection = new QLabel(peerDetailWidget);
        peerDirection->setObjectName("peerDirection");
        peerDirection->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        peerDirection->setTextFormat(Qt::PlainText);
        peerDirection->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        peerDetailsGrid->addWidget(peerDirection, 1, 1, 1, 1);

        peerVersionLabel = new QLabel(peerDetailWidget);
        peerVersionLabel->setObjectName("peerVersionLabel");

        peerDetailsGrid->addWidget(peerVersionLabel, 2, 0, 1, 1);

        peerVersion = new QLabel(peerDetailWidget);
        peerVersion->setObjectName("peerVersion");
        peerVersion->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        peerVersion->setTextFormat(Qt::PlainText);
        peerVersion->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        peerDetailsGrid->addWidget(peerVersion, 2, 1, 1, 1);

        peerSubversionLabel = new QLabel(peerDetailWidget);
        peerSubversionLabel->setObjectName("peerSubversionLabel");

        peerDetailsGrid->addWidget(peerSubversionLabel, 3, 0, 1, 1);

        peerSubversion = new QLabel(peerDetailWidget);
        peerSubversion->setObjectName("peerSubversion");
        peerSubversion->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        peerSubversion->setTextFormat(Qt::PlainText);
        peerSubversion->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        peerDetailsGrid->addWidget(peerSubversion, 3, 1, 1, 1);

        peerServicesLabel = new QLabel(peerDetailWidget);
        peerServicesLabel->setObjectName("peerServicesLabel");

        peerDetailsGrid->addWidget(peerServicesLabel, 4, 0, 1, 1);

        peerServices = new QLabel(peerDetailWidget);
        peerServices->setObjectName("peerServices");
        peerServices->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        peerServices->setTextFormat(Qt::PlainText);
        peerServices->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        peerDetailsGrid->addWidget(peerServices, 4, 1, 1, 1);

        peerHeightLabel = new QLabel(peerDetailWidget);
        peerHeightLabel->setObjectName("peerHeightLabel");

        peerDetailsGrid->addWidget(peerHeightLabel, 5, 0, 1, 1);

        peerHeight = new QLabel(peerDetailWidget);
        peerHeight->setObjectName("peerHeight");
        peerHeight->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        peerHeight->setTextFormat(Qt::PlainText);
        peerHeight->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        peerDetailsGrid->addWidget(peerHeight, 5, 1, 1, 1);

        peerSyncHeightLabel = new QLabel(peerDetailWidget);
        peerSyncHeightLabel->setObjectName("peerSyncHeightLabel");

        peerDetailsGrid->addWidget(peerSyncHeightLabel, 6, 0, 1, 1);

        peerSyncHeight = new QLabel(peerDetailWidget);
        peerSyncHeight->setObjectName("peerSyncHeight");
        peerSyncHeight->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        peerSyncHeight->setTextFormat(Qt::PlainText);
        peerSyncHeight->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        peerDetailsGrid->addWidget(peerSyncHeight, 6, 1, 1, 1);

        peerCommonHeightLabel = new QLabel(peerDetailWidget);
        peerCommonHeightLabel->setObjectName("peerCommonHeightLabel");

        peerDetailsGrid->addWidget(peerCommonHeightLabel, 7, 0, 1, 1);

        peerCommonHeight = new QLabel(peerDetailWidget);
        peerCommonHeight->setObjectName("peerCommonHeight");
        peerCommonHeight->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        peerCommonHeight->setTextFormat(Qt::PlainText);
        peerCommonHeight->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        peerDetailsGrid->addWidget(peerCommonHeight, 7, 1, 1, 1);

        peerBanScoreLabel = new QLabel(peerDetailWidget);
        peerBanScoreLabel->setObjectName("peerBanScoreLabel");

        peerDetailsGrid->addWidget(peerBanScoreLabel, 8, 0, 1, 1);

        peerBanScore = new QLabel(peerDetailWidget);
        peerBanScore->setObjectName("peerBanScore");
        peerBanScore->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        peerBanScore->setTextFormat(Qt::PlainText);
        peerBanScore->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        peerDetailsGrid->addWidget(peerBanScore, 8, 1, 1, 1);

        peerConnTimeLavel = new QLabel(peerDetailWidget);
        peerConnTimeLavel->setObjectName("peerConnTimeLavel");

        peerDetailsGrid->addWidget(peerConnTimeLavel, 9, 0, 1, 1);

        peerConnTime = new QLabel(peerDetailWidget);
        peerConnTime->setObjectName("peerConnTime");
        peerConnTime->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        peerConnTime->setTextFormat(Qt::PlainText);
        peerConnTime->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        peerDetailsGrid->addWidget(peerConnTime, 9, 1, 1, 1);

        lastSendLabel = new QLabel(peerDetailWidget);
        lastSendLabel->setObjectName("lastSendLabel");

        peerDetailsGrid->addWidget(lastSendLabel, 10, 0, 1, 1);

        peerLastSend = new QLabel(peerDetailWidget);
        peerLastSend->setObjectName("peerLastSend");
        peerLastSend->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        peerLastSend->setTextFormat(Qt::PlainText);
        peerLastSend->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        peerDetailsGrid->addWidget(peerLastSend, 10, 1, 1, 1);

        peerLastRecvLabel = new QLabel(peerDetailWidget);
        peerLastRecvLabel->setObjectName("peerLastRecvLabel");

        peerDetailsGrid->addWidget(peerLastRecvLabel, 11, 0, 1, 1);

        peerLastRecv = new QLabel(peerDetailWidget);
        peerLastRecv->setObjectName("peerLastRecv");
        peerLastRecv->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        peerLastRecv->setTextFormat(Qt::PlainText);
        peerLastRecv->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        peerDetailsGrid->addWidget(peerLastRecv, 11, 1, 1, 1);

        peerBytesSentLabel = new QLabel(peerDetailWidget);
        peerBytesSentLabel->setObjectName("peerBytesSentLabel");

        peerDetailsGrid->addWidget(peerBytesSentLabel, 12, 0, 1, 1);

        peerBytesSent = new QLabel(peerDetailWidget);
        peerBytesSent->setObjectName("peerBytesSent");
        peerBytesSent->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        peerBytesSent->setTextFormat(Qt::PlainText);
        peerBytesSent->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        peerDetailsGrid->addWidget(peerBytesSent, 12, 1, 1, 1);

        peerBytesRecvLabel = new QLabel(peerDetailWidget);
        peerBytesRecvLabel->setObjectName("peerBytesRecvLabel");

        peerDetailsGrid->addWidget(peerBytesRecvLabel, 13, 0, 1, 1);

        peerBytesRecv = new QLabel(peerDetailWidget);
        peerBytesRecv->setObjectName("peerBytesRecv");
        peerBytesRecv->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        peerBytesRecv->setTextFormat(Qt::PlainText);
        peerBytesRecv->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        peerDetailsGrid->addWidget(peerBytesRecv, 13, 1, 1, 1);

        peerPingTimeLabel = new QLabel(peerDetailWidget);
        peerPingTimeLabel->setObjectName("peerPingTimeLabel");

        peerDetailsGrid->addWidget(peerPingTimeLabel, 14, 0, 1, 1);

        peerPingTime = new QLabel(peerDetailWidget);
        peerPingTime->setObjectName("peerPingTime");
        peerPingTime->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        peerPingTime->setTextFormat(Qt::PlainText);
        peerPingTime->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        peerDetailsGrid->addWidget(peerPingTime, 14, 1, 1, 1);

        peerPingWaitLabel = new QLabel(peerDetailWidget);
        peerPingWaitLabel->setObjectName("peerPingWaitLabel");

        peerDetailsGrid->addWidget(peerPingWaitLabel, 15, 0, 1, 1);

        peerPingWait = new QLabel(peerDetailWidget);
        peerPingWait->setObjectName("peerPingWait");
        peerPingWait->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        peerPingWait->setTextFormat(Qt::PlainText);
        peerPingWait->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        peerDetailsGrid->addWidget(peerPingWait, 15, 1, 1, 1);

        peerMinPingLabel = new QLabel(peerDetailWidget);
        peerMinPingLabel->setObjectName("peerMinPingLabel");

        peerDetailsGrid->addWidget(peerMinPingLabel, 16, 0, 1, 1);

        peerMinPing = new QLabel(peerDetailWidget);
        peerMinPing->setObjectName("peerMinPing");
        peerMinPing->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        peerMinPing->setTextFormat(Qt::PlainText);
        peerMinPing->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        peerDetailsGrid->addWidget(peerMinPing, 16, 1, 1, 1);

        timeoffsetLabel = new QLabel(peerDetailWidget);
        timeoffsetLabel->setObjectName("timeoffsetLabel");

        peerDetailsGrid->addWidget(timeoffsetLabel, 17, 0, 1, 1);

        timeoffset = new QLabel(peerDetailWidget);
        timeoffset->setObjectName("timeoffset");
        timeoffset->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        timeoffset->setTextFormat(Qt::PlainText);
        timeoffset->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        peerDetailsGrid->addWidget(timeoffset, 17, 1, 1, 1);

        verticalSpacer_5 = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        peerDetailsGrid->addItem(verticalSpacer_5, 18, 0, 1, 1);

        peerDetailsGrid->setColumnStretch(1, 1);

        verticalLayout_8->addLayout(peerDetailsGrid);

        splitter->addWidget(peerDetailWidget);

        verticalLayout_10->addWidget(splitter);

        tabWidget->addTab(tab_peers, QString());
        tab_console = new QWidget();
        tab_console->setObjectName("tab_console");
        verticalLayout_9 = new QVBoxLayout(tab_console);
        verticalLayout_9->setSpacing(3);
        verticalLayout_9->setObjectName("verticalLayout_9");
        messagesWidget = new QTextEdit(tab_console);
        messagesWidget->setObjectName("messagesWidget");
        messagesWidget->setMinimumSize(QSize(0, 100));
        messagesWidget->setAutoFillBackground(true);
        messagesWidget->setReadOnly(true);
        messagesWidget->setProperty("tabKeyNavigation", QVariant(false));
        messagesWidget->setProperty("columnCount", QVariant(2));

        verticalLayout_9->addWidget(messagesWidget);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setSpacing(3);
        horizontalLayout->setObjectName("horizontalLayout");
        label = new QLabel(tab_console);
        label->setObjectName("label");
        label->setText(QString::fromUtf8(">"));

        horizontalLayout->addWidget(label);

        lineEdit = new QLineEdit(tab_console);
        lineEdit->setObjectName("lineEdit");
        lineEdit->setAutoFillBackground(false);

        horizontalLayout->addWidget(lineEdit);

        clearButton = new QPushButton(tab_console);
        clearButton->setObjectName("clearButton");
        clearButton->setMaximumSize(QSize(24, 24));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icons/remove"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        clearButton->setIcon(icon);
#if QT_CONFIG(shortcut)
        clearButton->setShortcut(QString::fromUtf8("Ctrl+L"));
#endif // QT_CONFIG(shortcut)
        clearButton->setAutoDefault(false);

        horizontalLayout->addWidget(clearButton);


        verticalLayout_9->addLayout(horizontalLayout);

        tabWidget->addTab(tab_console, QString());
        tab_scraper = new QWidget();
        tab_scraper->setObjectName("tab_scraper");
        verticalLayout_6 = new QVBoxLayout(tab_scraper);
        verticalLayout_6->setSpacing(3);
        verticalLayout_6->setObjectName("verticalLayout_6");
        scraper_log = new QPlainTextEdit(tab_scraper);
        scraper_log->setObjectName("scraper_log");
        scraper_log->setAutoFillBackground(true);
        scraper_log->setTextInteractionFlags(Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);
        scraper_log->setMaximumBlockCount(10000);

        verticalLayout_6->addWidget(scraper_log);

        tabWidget->addTab(tab_scraper, QString());

        verticalLayout_2->addWidget(tabWidget);


        retranslateUi(RPCConsole);

        tabWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(RPCConsole);
    } // setupUi

    void retranslateUi(QDialog *RPCConsole)
    {
        RPCConsole->setWindowTitle(QCoreApplication::translate("RPCConsole", "Gridcoin - Debug Console", nullptr));
        isTestNetLabel->setText(QCoreApplication::translate("RPCConsole", "On testnet", nullptr));
        boostVersion->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        diff->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
#if QT_CONFIG(tooltip)
        showCLOptionsButton->setToolTip(QCoreApplication::translate("RPCConsole", "Show the Gridcoin help message to get a list with possible Gridcoin command-line options.", nullptr));
#endif // QT_CONFIG(tooltip)
        showCLOptionsButton->setText(QCoreApplication::translate("RPCConsole", "&Show", nullptr));
        openSSLVersionLabel->setText(QCoreApplication::translate("RPCConsole", "OpenSSL version", nullptr));
        qtVersion->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        showCLOptionsLabel->setText(QCoreApplication::translate("RPCConsole", "Command-line options", nullptr));
        numberOfConnectionsLabel->setText(QCoreApplication::translate("RPCConsole", "Number of connections", nullptr));
        numberOfBlocks->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        numberOfConnections->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        clientVersionLabel->setText(QCoreApplication::translate("RPCConsole", "Client version", nullptr));
        totalBlocks->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        gridcoinCoreLabel->setText(QCoreApplication::translate("RPCConsole", "Gridcoin Core:", nullptr));
        clientName->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        clientVersion->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        clientNameLabel->setText(QCoreApplication::translate("RPCConsole", "Client name", nullptr));
        qtVersionLabel->setText(QCoreApplication::translate("RPCConsole", "Qt version", nullptr));
        lastBlockTime->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        openDebugLogfileLabel->setText(QCoreApplication::translate("RPCConsole", "Debug log file", nullptr));
        numberOfBlocksLabel->setText(QCoreApplication::translate("RPCConsole", "Current number of blocks", nullptr));
        diffLabel->setText(QCoreApplication::translate("RPCConsole", "Difficulty", nullptr));
        lastBlockTimeLabel->setText(QCoreApplication::translate("RPCConsole", "Last block time", nullptr));
        networkLabel->setText(QCoreApplication::translate("RPCConsole", "Network:", nullptr));
        startupTimeLabel->setText(QCoreApplication::translate("RPCConsole", "Startup time", nullptr));
        openSSLVersion->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        boostVersionLabel->setText(QCoreApplication::translate("RPCConsole", "Boost version", nullptr));
        blockchainLabel->setText(QCoreApplication::translate("RPCConsole", "Block chain", nullptr));
        startupTime->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        totalBlocksLabel->setText(QCoreApplication::translate("RPCConsole", "Estimated total blocks", nullptr));
#if QT_CONFIG(tooltip)
        openDebugLogfileButton->setToolTip(QCoreApplication::translate("RPCConsole", "Open the Gridcoin debug log file from the current data directory. This can take a few seconds for large log files.", nullptr));
#endif // QT_CONFIG(tooltip)
        openDebugLogfileButton->setText(QCoreApplication::translate("RPCConsole", "&Open", nullptr));
        isTestNet->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_info), QCoreApplication::translate("RPCConsole", "&Information", nullptr));
        clearTrafficGraphButton->setText(QCoreApplication::translate("RPCConsole", "&Clear", nullptr));
        groupBox->setTitle(QCoreApplication::translate("RPCConsole", "Totals", nullptr));
        bytesInTextLabel->setText(QCoreApplication::translate("RPCConsole", "In:", nullptr));
        bytesOutTextLabel->setText(QCoreApplication::translate("RPCConsole", "Out:", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_network), QCoreApplication::translate("RPCConsole", "&Network Traffic", nullptr));
        banHeading->setText(QCoreApplication::translate("RPCConsole", "Banned peers", nullptr));
        peerHeading->setText(QCoreApplication::translate("RPCConsole", "Select a peer to view detailed information.", nullptr));
        peerWhitelistedLabel->setText(QCoreApplication::translate("RPCConsole", "Whitelisted", nullptr));
        peerWhitelisted->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        peerDirectionLabel->setText(QCoreApplication::translate("RPCConsole", "Direction", nullptr));
        peerDirection->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        peerVersionLabel->setText(QCoreApplication::translate("RPCConsole", "Version", nullptr));
        peerVersion->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        peerSubversionLabel->setText(QCoreApplication::translate("RPCConsole", "User Agent", nullptr));
        peerSubversion->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        peerServicesLabel->setText(QCoreApplication::translate("RPCConsole", "Services", nullptr));
        peerServices->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        peerHeightLabel->setText(QCoreApplication::translate("RPCConsole", "Starting Block", nullptr));
        peerHeight->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        peerSyncHeightLabel->setText(QCoreApplication::translate("RPCConsole", "Synced Headers", nullptr));
        peerSyncHeight->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        peerCommonHeightLabel->setText(QCoreApplication::translate("RPCConsole", "Synced Blocks", nullptr));
        peerCommonHeight->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        peerBanScoreLabel->setText(QCoreApplication::translate("RPCConsole", "Ban Score", nullptr));
        peerBanScore->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        peerConnTimeLavel->setText(QCoreApplication::translate("RPCConsole", "Connection Time", nullptr));
        peerConnTime->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        lastSendLabel->setText(QCoreApplication::translate("RPCConsole", "Last Send", nullptr));
        peerLastSend->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        peerLastRecvLabel->setText(QCoreApplication::translate("RPCConsole", "Last Receive", nullptr));
        peerLastRecv->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        peerBytesSentLabel->setText(QCoreApplication::translate("RPCConsole", "Sent", nullptr));
        peerBytesSent->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        peerBytesRecvLabel->setText(QCoreApplication::translate("RPCConsole", "Received", nullptr));
        peerBytesRecv->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        peerPingTimeLabel->setText(QCoreApplication::translate("RPCConsole", "Ping Time", nullptr));
        peerPingTime->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
#if QT_CONFIG(tooltip)
        peerPingWaitLabel->setToolTip(QCoreApplication::translate("RPCConsole", "The duration of a currently outstanding ping.", nullptr));
#endif // QT_CONFIG(tooltip)
        peerPingWaitLabel->setText(QCoreApplication::translate("RPCConsole", "Ping Wait", nullptr));
        peerPingWait->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        peerMinPingLabel->setText(QCoreApplication::translate("RPCConsole", "Min Ping", nullptr));
        peerMinPing->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        timeoffsetLabel->setText(QCoreApplication::translate("RPCConsole", "Time Offset", nullptr));
        timeoffset->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_peers), QCoreApplication::translate("RPCConsole", "&Peers", nullptr));
#if QT_CONFIG(tooltip)
        clearButton->setToolTip(QCoreApplication::translate("RPCConsole", "Clear console", nullptr));
#endif // QT_CONFIG(tooltip)
        clearButton->setText(QString());
        tabWidget->setTabText(tabWidget->indexOf(tab_console), QCoreApplication::translate("RPCConsole", "&Console", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_scraper), QCoreApplication::translate("RPCConsole", "&Scraper", nullptr));
    } // retranslateUi

};

namespace Ui {
    class RPCConsole: public Ui_RPCConsole {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_RPCCONSOLE_H
