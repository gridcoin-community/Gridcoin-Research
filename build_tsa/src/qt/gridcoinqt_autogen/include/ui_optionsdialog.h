/********************************************************************************
** Form generated from reading UI file 'optionsdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_OPTIONSDIALOG_H
#define UI_OPTIONSDIALOG_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDateEdit>
#include <QtWidgets/QDialog>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTableView>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <qvalidatedlineedit.h>
#include "bitcoinamountfield.h"
#include "qvaluecombobox.h"

QT_BEGIN_NAMESPACE

class Ui_OptionsDialog
{
public:
    QGridLayout *gridLayout;
    QTabWidget *tabWidget;
    QWidget *tabMain;
    QVBoxLayout *verticalLayout_Main;
    QLabel *reserveBalanceInfoLabel;
    QHBoxLayout *horizontalLayoutReserveBalance;
    QLabel *reserveBalanceLabel;
    BitcoinAmountField *reserveBalance;
    QSpacerItem *horizontalSpacerReserveBalance;
    QHBoxLayout *horizontalLayoutGridcoinStartup;
    QCheckBox *gridcoinAtStartup;
    QCheckBox *gridcoinAtStartupMinimised;
    QCheckBox *disableUpdateCheck;
    QCheckBox *returnChangeToInputAddressForContracts;
    QSpacerItem *verticalSpacer_Main;
    QWidget *tabNetwork;
    QVBoxLayout *verticalLayout_Network;
    QCheckBox *mapPortUpnp;
    QCheckBox *connectSocks;
    QHBoxLayout *horizontalLayout_Network;
    QLabel *proxyIpLabel;
    QValidatedLineEdit *proxyIp;
    QLabel *proxyPortLabel;
    QLineEdit *proxyPort;
    QSpacerItem *horizontalSpacer_Network;
    QSpacerItem *verticalSpacer_Network;
    QWidget *tabStaking;
    QVBoxLayout *verticalLayout;
    QVBoxLayout *verticalLayout_StakeSplit;
    QCheckBox *enableStaking;
    QCheckBox *enableStakeSplit;
    QHBoxLayout *horizontalLayout_StakeSplit;
    QLabel *stakingEfficiencyLabel;
    QValidatedLineEdit *stakingEfficiency;
    QLabel *minPostSplitOutputValueLabel;
    QValidatedLineEdit *minPostSplitOutputValue;
    QSpacerItem *verticalSpacer_StakeSplit;
    QCheckBox *enableSideStaking;
    QTableView *sidestakingTableView;
    QHBoxLayout *horizontalLayoutSideStake;
    QPushButton *pushButtonNewSideStake;
    QPushButton *pushButtonEditSideStake;
    QPushButton *pushButtonDeleteSideStake;
    QSpacerItem *horizontalSpacer_SideStake;
    QWidget *tabWindow;
    QVBoxLayout *verticalLayout_Window;
    QCheckBox *minimizeToTray;
    QCheckBox *minimizeOnClose;
    QCheckBox *confirmOnClose;
    QCheckBox *disableTransactionNotifications;
    QCheckBox *disablePollNotifications;
    QHBoxLayout *horizontalLayout;
    QLabel *pollExpireNotifyLabel;
    QValidatedLineEdit *pollExpireNotifyLineEdit;
    QSpacerItem *horizontalSpacer;
    QSpacerItem *verticalSpacer_Window;
    QWidget *tabDisplay;
    QVBoxLayout *verticalLayout_Display;
    QHBoxLayout *horizontalLayout_1_Display;
    QLabel *langLabel;
    QValueComboBox *lang;
    QHBoxLayout *horizontalLayout_2_Display;
    QLabel *unitLabel;
    QValueComboBox *unit;
    QHBoxLayout *horizontalLayout_3_Display;
    QLabel *styleLabel;
    QComboBox *styleComboBox;
    QCheckBox *displayAddresses;
    QHBoxLayout *horizontalLayout_4_Display;
    QCheckBox *limitTxnDisplayCheckBox;
    QDateEdit *limitTxnDisplayDateEdit;
    QSpacerItem *verticalSpacer_Display;
    QHBoxLayout *horizontalLayout_Buttons;
    QSpacerItem *horizontalSpacer_1;
    QLabel *statusLabel;
    QSpacerItem *horizontalSpacer_2;
    QPushButton *okButton;
    QPushButton *cancelButton;
    QPushButton *applyButton;

    void setupUi(QDialog *OptionsDialog)
    {
        if (OptionsDialog->objectName().isEmpty())
            OptionsDialog->setObjectName("OptionsDialog");
        OptionsDialog->resize(700, 420);
        OptionsDialog->setModal(true);
        gridLayout = new QGridLayout(OptionsDialog);
        gridLayout->setObjectName("gridLayout");
        tabWidget = new QTabWidget(OptionsDialog);
        tabWidget->setObjectName("tabWidget");
        tabWidget->setTabPosition(QTabWidget::North);
        tabMain = new QWidget();
        tabMain->setObjectName("tabMain");
        verticalLayout_Main = new QVBoxLayout(tabMain);
        verticalLayout_Main->setObjectName("verticalLayout_Main");
        reserveBalanceInfoLabel = new QLabel(tabMain);
        reserveBalanceInfoLabel->setObjectName("reserveBalanceInfoLabel");
        reserveBalanceInfoLabel->setTextFormat(Qt::PlainText);
        reserveBalanceInfoLabel->setWordWrap(true);

        verticalLayout_Main->addWidget(reserveBalanceInfoLabel);

        horizontalLayoutReserveBalance = new QHBoxLayout();
        horizontalLayoutReserveBalance->setObjectName("horizontalLayoutReserveBalance");
        reserveBalanceLabel = new QLabel(tabMain);
        reserveBalanceLabel->setObjectName("reserveBalanceLabel");
        reserveBalanceLabel->setTextFormat(Qt::PlainText);

        horizontalLayoutReserveBalance->addWidget(reserveBalanceLabel);

        reserveBalance = new BitcoinAmountField(tabMain);
        reserveBalance->setObjectName("reserveBalance");

        horizontalLayoutReserveBalance->addWidget(reserveBalance);

        horizontalSpacerReserveBalance = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayoutReserveBalance->addItem(horizontalSpacerReserveBalance);


        verticalLayout_Main->addLayout(horizontalLayoutReserveBalance);

        horizontalLayoutGridcoinStartup = new QHBoxLayout();
        horizontalLayoutGridcoinStartup->setObjectName("horizontalLayoutGridcoinStartup");
        gridcoinAtStartup = new QCheckBox(tabMain);
        gridcoinAtStartup->setObjectName("gridcoinAtStartup");

        horizontalLayoutGridcoinStartup->addWidget(gridcoinAtStartup);

        gridcoinAtStartupMinimised = new QCheckBox(tabMain);
        gridcoinAtStartupMinimised->setObjectName("gridcoinAtStartupMinimised");

        horizontalLayoutGridcoinStartup->addWidget(gridcoinAtStartupMinimised);


        verticalLayout_Main->addLayout(horizontalLayoutGridcoinStartup);

        disableUpdateCheck = new QCheckBox(tabMain);
        disableUpdateCheck->setObjectName("disableUpdateCheck");
        disableUpdateCheck->setEnabled(true);

        verticalLayout_Main->addWidget(disableUpdateCheck);

        returnChangeToInputAddressForContracts = new QCheckBox(tabMain);
        returnChangeToInputAddressForContracts->setObjectName("returnChangeToInputAddressForContracts");

        verticalLayout_Main->addWidget(returnChangeToInputAddressForContracts);

        verticalSpacer_Main = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_Main->addItem(verticalSpacer_Main);

        tabWidget->addTab(tabMain, QString());
        tabNetwork = new QWidget();
        tabNetwork->setObjectName("tabNetwork");
        verticalLayout_Network = new QVBoxLayout(tabNetwork);
        verticalLayout_Network->setObjectName("verticalLayout_Network");
        mapPortUpnp = new QCheckBox(tabNetwork);
        mapPortUpnp->setObjectName("mapPortUpnp");

        verticalLayout_Network->addWidget(mapPortUpnp);

        connectSocks = new QCheckBox(tabNetwork);
        connectSocks->setObjectName("connectSocks");

        verticalLayout_Network->addWidget(connectSocks);

        horizontalLayout_Network = new QHBoxLayout();
        horizontalLayout_Network->setObjectName("horizontalLayout_Network");
        proxyIpLabel = new QLabel(tabNetwork);
        proxyIpLabel->setObjectName("proxyIpLabel");
        proxyIpLabel->setTextFormat(Qt::PlainText);

        horizontalLayout_Network->addWidget(proxyIpLabel);

        proxyIp = new QValidatedLineEdit(tabNetwork);
        proxyIp->setObjectName("proxyIp");
        proxyIp->setMaximumSize(QSize(140, 16777215));

        horizontalLayout_Network->addWidget(proxyIp);

        proxyPortLabel = new QLabel(tabNetwork);
        proxyPortLabel->setObjectName("proxyPortLabel");
        proxyPortLabel->setTextFormat(Qt::PlainText);

        horizontalLayout_Network->addWidget(proxyPortLabel);

        proxyPort = new QLineEdit(tabNetwork);
        proxyPort->setObjectName("proxyPort");
        proxyPort->setMaximumSize(QSize(55, 16777215));

        horizontalLayout_Network->addWidget(proxyPort);

        horizontalSpacer_Network = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_Network->addItem(horizontalSpacer_Network);


        verticalLayout_Network->addLayout(horizontalLayout_Network);

        verticalSpacer_Network = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_Network->addItem(verticalSpacer_Network);

        tabWidget->addTab(tabNetwork, QString());
        tabStaking = new QWidget();
        tabStaking->setObjectName("tabStaking");
        verticalLayout = new QVBoxLayout(tabStaking);
        verticalLayout->setObjectName("verticalLayout");
        verticalLayout_StakeSplit = new QVBoxLayout();
        verticalLayout_StakeSplit->setObjectName("verticalLayout_StakeSplit");
        enableStaking = new QCheckBox(tabStaking);
        enableStaking->setObjectName("enableStaking");

        verticalLayout_StakeSplit->addWidget(enableStaking);

        enableStakeSplit = new QCheckBox(tabStaking);
        enableStakeSplit->setObjectName("enableStakeSplit");

        verticalLayout_StakeSplit->addWidget(enableStakeSplit);

        horizontalLayout_StakeSplit = new QHBoxLayout();
        horizontalLayout_StakeSplit->setObjectName("horizontalLayout_StakeSplit");
        stakingEfficiencyLabel = new QLabel(tabStaking);
        stakingEfficiencyLabel->setObjectName("stakingEfficiencyLabel");

        horizontalLayout_StakeSplit->addWidget(stakingEfficiencyLabel);

        stakingEfficiency = new QValidatedLineEdit(tabStaking);
        stakingEfficiency->setObjectName("stakingEfficiency");

        horizontalLayout_StakeSplit->addWidget(stakingEfficiency);

        minPostSplitOutputValueLabel = new QLabel(tabStaking);
        minPostSplitOutputValueLabel->setObjectName("minPostSplitOutputValueLabel");

        horizontalLayout_StakeSplit->addWidget(minPostSplitOutputValueLabel);

        minPostSplitOutputValue = new QValidatedLineEdit(tabStaking);
        minPostSplitOutputValue->setObjectName("minPostSplitOutputValue");

        horizontalLayout_StakeSplit->addWidget(minPostSplitOutputValue);

        verticalSpacer_StakeSplit = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        horizontalLayout_StakeSplit->addItem(verticalSpacer_StakeSplit);


        verticalLayout_StakeSplit->addLayout(horizontalLayout_StakeSplit);

        enableSideStaking = new QCheckBox(tabStaking);
        enableSideStaking->setObjectName("enableSideStaking");

        verticalLayout_StakeSplit->addWidget(enableSideStaking);

        sidestakingTableView = new QTableView(tabStaking);
        sidestakingTableView->setObjectName("sidestakingTableView");
        sidestakingTableView->setSortingEnabled(true);

        verticalLayout_StakeSplit->addWidget(sidestakingTableView);

        horizontalLayoutSideStake = new QHBoxLayout();
        horizontalLayoutSideStake->setObjectName("horizontalLayoutSideStake");
        pushButtonNewSideStake = new QPushButton(tabStaking);
        pushButtonNewSideStake->setObjectName("pushButtonNewSideStake");
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icons/add"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        pushButtonNewSideStake->setIcon(icon);

        horizontalLayoutSideStake->addWidget(pushButtonNewSideStake);

        pushButtonEditSideStake = new QPushButton(tabStaking);
        pushButtonEditSideStake->setObjectName("pushButtonEditSideStake");
        pushButtonEditSideStake->setEnabled(false);
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/icons/edit"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        pushButtonEditSideStake->setIcon(icon1);

        horizontalLayoutSideStake->addWidget(pushButtonEditSideStake);

        pushButtonDeleteSideStake = new QPushButton(tabStaking);
        pushButtonDeleteSideStake->setObjectName("pushButtonDeleteSideStake");
        pushButtonDeleteSideStake->setEnabled(false);
        QIcon icon2;
        icon2.addFile(QString::fromUtf8(":/icons/remove"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        pushButtonDeleteSideStake->setIcon(icon2);

        horizontalLayoutSideStake->addWidget(pushButtonDeleteSideStake);

        horizontalSpacer_SideStake = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayoutSideStake->addItem(horizontalSpacer_SideStake);


        verticalLayout_StakeSplit->addLayout(horizontalLayoutSideStake);


        verticalLayout->addLayout(verticalLayout_StakeSplit);

        tabWidget->addTab(tabStaking, QString());
        tabWindow = new QWidget();
        tabWindow->setObjectName("tabWindow");
        verticalLayout_Window = new QVBoxLayout(tabWindow);
        verticalLayout_Window->setObjectName("verticalLayout_Window");
        minimizeToTray = new QCheckBox(tabWindow);
        minimizeToTray->setObjectName("minimizeToTray");

        verticalLayout_Window->addWidget(minimizeToTray);

        minimizeOnClose = new QCheckBox(tabWindow);
        minimizeOnClose->setObjectName("minimizeOnClose");

        verticalLayout_Window->addWidget(minimizeOnClose);

        confirmOnClose = new QCheckBox(tabWindow);
        confirmOnClose->setObjectName("confirmOnClose");

        verticalLayout_Window->addWidget(confirmOnClose);

        disableTransactionNotifications = new QCheckBox(tabWindow);
        disableTransactionNotifications->setObjectName("disableTransactionNotifications");

        verticalLayout_Window->addWidget(disableTransactionNotifications);

        disablePollNotifications = new QCheckBox(tabWindow);
        disablePollNotifications->setObjectName("disablePollNotifications");

        verticalLayout_Window->addWidget(disablePollNotifications);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        pollExpireNotifyLabel = new QLabel(tabWindow);
        pollExpireNotifyLabel->setObjectName("pollExpireNotifyLabel");

        horizontalLayout->addWidget(pollExpireNotifyLabel);

        pollExpireNotifyLineEdit = new QValidatedLineEdit(tabWindow);
        pollExpireNotifyLineEdit->setObjectName("pollExpireNotifyLineEdit");

        horizontalLayout->addWidget(pollExpireNotifyLineEdit);

        horizontalSpacer = new QSpacerItem(80, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);


        verticalLayout_Window->addLayout(horizontalLayout);

        verticalSpacer_Window = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_Window->addItem(verticalSpacer_Window);

        tabWidget->addTab(tabWindow, QString());
        tabDisplay = new QWidget();
        tabDisplay->setObjectName("tabDisplay");
        verticalLayout_Display = new QVBoxLayout(tabDisplay);
        verticalLayout_Display->setObjectName("verticalLayout_Display");
        horizontalLayout_1_Display = new QHBoxLayout();
        horizontalLayout_1_Display->setObjectName("horizontalLayout_1_Display");
        langLabel = new QLabel(tabDisplay);
        langLabel->setObjectName("langLabel");
        langLabel->setTextFormat(Qt::PlainText);

        horizontalLayout_1_Display->addWidget(langLabel);

        lang = new QValueComboBox(tabDisplay);
        lang->setObjectName("lang");

        horizontalLayout_1_Display->addWidget(lang);


        verticalLayout_Display->addLayout(horizontalLayout_1_Display);

        horizontalLayout_2_Display = new QHBoxLayout();
        horizontalLayout_2_Display->setObjectName("horizontalLayout_2_Display");
        unitLabel = new QLabel(tabDisplay);
        unitLabel->setObjectName("unitLabel");
        unitLabel->setTextFormat(Qt::PlainText);

        horizontalLayout_2_Display->addWidget(unitLabel);

        unit = new QValueComboBox(tabDisplay);
        unit->setObjectName("unit");

        horizontalLayout_2_Display->addWidget(unit);


        verticalLayout_Display->addLayout(horizontalLayout_2_Display);

        horizontalLayout_3_Display = new QHBoxLayout();
        horizontalLayout_3_Display->setObjectName("horizontalLayout_3_Display");
        styleLabel = new QLabel(tabDisplay);
        styleLabel->setObjectName("styleLabel");

        horizontalLayout_3_Display->addWidget(styleLabel);

        styleComboBox = new QComboBox(tabDisplay);
        styleComboBox->setObjectName("styleComboBox");

        horizontalLayout_3_Display->addWidget(styleComboBox);


        verticalLayout_Display->addLayout(horizontalLayout_3_Display);

        displayAddresses = new QCheckBox(tabDisplay);
        displayAddresses->setObjectName("displayAddresses");

        verticalLayout_Display->addWidget(displayAddresses);

        horizontalLayout_4_Display = new QHBoxLayout();
        horizontalLayout_4_Display->setObjectName("horizontalLayout_4_Display");
        limitTxnDisplayCheckBox = new QCheckBox(tabDisplay);
        limitTxnDisplayCheckBox->setObjectName("limitTxnDisplayCheckBox");

        horizontalLayout_4_Display->addWidget(limitTxnDisplayCheckBox);

        limitTxnDisplayDateEdit = new QDateEdit(tabDisplay);
        limitTxnDisplayDateEdit->setObjectName("limitTxnDisplayDateEdit");

        horizontalLayout_4_Display->addWidget(limitTxnDisplayDateEdit);


        verticalLayout_Display->addLayout(horizontalLayout_4_Display);

        verticalSpacer_Display = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_Display->addItem(verticalSpacer_Display);

        tabWidget->addTab(tabDisplay, QString());

        gridLayout->addWidget(tabWidget, 0, 0, 1, 1);

        horizontalLayout_Buttons = new QHBoxLayout();
        horizontalLayout_Buttons->setObjectName("horizontalLayout_Buttons");
        horizontalSpacer_1 = new QSpacerItem(40, 48, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_Buttons->addItem(horizontalSpacer_1);

        statusLabel = new QLabel(OptionsDialog);
        statusLabel->setObjectName("statusLabel");
        statusLabel->setTextFormat(Qt::PlainText);
        statusLabel->setWordWrap(true);

        horizontalLayout_Buttons->addWidget(statusLabel);

        horizontalSpacer_2 = new QSpacerItem(40, 48, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_Buttons->addItem(horizontalSpacer_2);

        okButton = new QPushButton(OptionsDialog);
        okButton->setObjectName("okButton");

        horizontalLayout_Buttons->addWidget(okButton);

        cancelButton = new QPushButton(OptionsDialog);
        cancelButton->setObjectName("cancelButton");
        cancelButton->setAutoDefault(false);

        horizontalLayout_Buttons->addWidget(cancelButton);

        applyButton = new QPushButton(OptionsDialog);
        applyButton->setObjectName("applyButton");
        applyButton->setAutoDefault(false);

        horizontalLayout_Buttons->addWidget(applyButton);


        gridLayout->addLayout(horizontalLayout_Buttons, 1, 0, 1, 1);

#if QT_CONFIG(shortcut)
        reserveBalanceLabel->setBuddy(reserveBalance);
        proxyIpLabel->setBuddy(proxyIp);
        proxyPortLabel->setBuddy(proxyPort);
        langLabel->setBuddy(lang);
        unitLabel->setBuddy(unit);
#endif // QT_CONFIG(shortcut)

        retranslateUi(OptionsDialog);

        tabWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(OptionsDialog);
    } // setupUi

    void retranslateUi(QDialog *OptionsDialog)
    {
        OptionsDialog->setWindowTitle(QCoreApplication::translate("OptionsDialog", "Options", nullptr));
        reserveBalanceInfoLabel->setText(QCoreApplication::translate("OptionsDialog", "Reserved amount secures a balance in wallet that can be spendable at anytime. However reserve will secure utxo(s) of any size to respect this setting.", nullptr));
        reserveBalanceLabel->setText(QCoreApplication::translate("OptionsDialog", "Reser&ve", nullptr));
#if QT_CONFIG(tooltip)
        gridcoinAtStartup->setToolTip(QCoreApplication::translate("OptionsDialog", "Automatically start Gridcoin after logging in to the system.", nullptr));
#endif // QT_CONFIG(tooltip)
        gridcoinAtStartup->setText(QCoreApplication::translate("OptionsDialog", "&Start Gridcoin on system login", nullptr));
        gridcoinAtStartupMinimised->setText(QCoreApplication::translate("OptionsDialog", "Start minimized", nullptr));
#if QT_CONFIG(tooltip)
        disableUpdateCheck->setToolTip(QCoreApplication::translate("OptionsDialog", "Allow regular checks for updates", nullptr));
#endif // QT_CONFIG(tooltip)
        disableUpdateCheck->setText(QCoreApplication::translate("OptionsDialog", "Disable &update checks", nullptr));
        returnChangeToInputAddressForContracts->setText(QCoreApplication::translate("OptionsDialog", "Return change to an input address for contract transactions", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tabMain), QCoreApplication::translate("OptionsDialog", "&Main", nullptr));
#if QT_CONFIG(tooltip)
        mapPortUpnp->setToolTip(QCoreApplication::translate("OptionsDialog", "Automatically open the Gridcoin client port on the router. This only works when your router supports UPnP and it is enabled.", nullptr));
#endif // QT_CONFIG(tooltip)
        mapPortUpnp->setText(QCoreApplication::translate("OptionsDialog", "Map port using &UPnP", nullptr));
#if QT_CONFIG(tooltip)
        connectSocks->setToolTip(QCoreApplication::translate("OptionsDialog", "Connect to the Gridcoin network through a SOCKS5 proxy (e.g. when connecting through Tor).", nullptr));
#endif // QT_CONFIG(tooltip)
        connectSocks->setText(QCoreApplication::translate("OptionsDialog", "&Connect through SOCKS5 proxy:", nullptr));
        proxyIpLabel->setText(QCoreApplication::translate("OptionsDialog", "Pro&xy IP:", nullptr));
#if QT_CONFIG(tooltip)
        proxyIp->setToolTip(QCoreApplication::translate("OptionsDialog", "IP address of the proxy (e.g. 127.0.0.1)", nullptr));
#endif // QT_CONFIG(tooltip)
        proxyPortLabel->setText(QCoreApplication::translate("OptionsDialog", "&Port:", nullptr));
#if QT_CONFIG(tooltip)
        proxyPort->setToolTip(QCoreApplication::translate("OptionsDialog", "Port of the proxy (e.g. 9050)", nullptr));
#endif // QT_CONFIG(tooltip)
        tabWidget->setTabText(tabWidget->indexOf(tabNetwork), QCoreApplication::translate("OptionsDialog", "&Network", nullptr));
#if QT_CONFIG(tooltip)
        enableStaking->setToolTip(QCoreApplication::translate("OptionsDialog", "This enables or disables staking (the default is enabled). Note that a change to this setting will permanently override the config file with an entry in the settings file.", nullptr));
#endif // QT_CONFIG(tooltip)
        enableStaking->setText(QCoreApplication::translate("OptionsDialog", "Enable Staking", nullptr));
#if QT_CONFIG(tooltip)
        enableStakeSplit->setToolTip(QCoreApplication::translate("OptionsDialog", "This enables or disables splitting of stake outputs to optimize staking (default disabled). Note that a change to this setting will permanently override the config file with an entry in the settings file.", nullptr));
#endif // QT_CONFIG(tooltip)
        enableStakeSplit->setText(QCoreApplication::translate("OptionsDialog", "Enable Stake Splitting", nullptr));
        stakingEfficiencyLabel->setText(QCoreApplication::translate("OptionsDialog", "Target Efficiency", nullptr));
#if QT_CONFIG(tooltip)
        stakingEfficiency->setToolTip(QCoreApplication::translate("OptionsDialog", "Valid values are between 75 and 98 percent. Note that a change to this setting will permanently override the config file with an entry in the settings file.", nullptr));
#endif // QT_CONFIG(tooltip)
        minPostSplitOutputValueLabel->setText(QCoreApplication::translate("OptionsDialog", "Min Post Split UTXO", nullptr));
#if QT_CONFIG(tooltip)
        minPostSplitOutputValue->setToolTip(QCoreApplication::translate("OptionsDialog", "Valid values are 800 or greater. Note that a change to this setting will permanently override the config file with an entry in the settings file.", nullptr));
#endif // QT_CONFIG(tooltip)
        enableSideStaking->setText(QCoreApplication::translate("OptionsDialog", "Enable Locally Specified Sidestaking", nullptr));
        pushButtonNewSideStake->setText(QCoreApplication::translate("OptionsDialog", "New", nullptr));
        pushButtonEditSideStake->setText(QCoreApplication::translate("OptionsDialog", "Edit", nullptr));
        pushButtonDeleteSideStake->setText(QCoreApplication::translate("OptionsDialog", "Delete", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tabStaking), QCoreApplication::translate("OptionsDialog", "Staking", nullptr));
#if QT_CONFIG(tooltip)
        minimizeToTray->setToolTip(QCoreApplication::translate("OptionsDialog", "Show only a tray icon after minimizing the window.", nullptr));
#endif // QT_CONFIG(tooltip)
        minimizeToTray->setText(QCoreApplication::translate("OptionsDialog", "&Minimize to the tray instead of the taskbar", nullptr));
#if QT_CONFIG(tooltip)
        minimizeOnClose->setToolTip(QCoreApplication::translate("OptionsDialog", "Minimize instead of exit the application when the window is closed. When this option is enabled, the application will be closed only after selecting Quit in the menu.", nullptr));
#endif // QT_CONFIG(tooltip)
        minimizeOnClose->setText(QCoreApplication::translate("OptionsDialog", "M&inimize on close", nullptr));
        confirmOnClose->setText(QCoreApplication::translate("OptionsDialog", "&Confirm on close", nullptr));
        disableTransactionNotifications->setText(QCoreApplication::translate("OptionsDialog", "Disable Transaction Notifications", nullptr));
        disablePollNotifications->setText(QCoreApplication::translate("OptionsDialog", "Disable Poll Notifications", nullptr));
        pollExpireNotifyLabel->setText(QCoreApplication::translate("OptionsDialog", "Hours before poll expiry reminder", nullptr));
#if QT_CONFIG(tooltip)
        pollExpireNotifyLineEdit->setToolTip(QCoreApplication::translate("OptionsDialog", "Valid values are between 0.25 and 168.0 hours.", nullptr));
#endif // QT_CONFIG(tooltip)
        tabWidget->setTabText(tabWidget->indexOf(tabWindow), QCoreApplication::translate("OptionsDialog", "&Window", nullptr));
        langLabel->setText(QCoreApplication::translate("OptionsDialog", "User Interface &language:", nullptr));
#if QT_CONFIG(tooltip)
        lang->setToolTip(QCoreApplication::translate("OptionsDialog", "The user interface language can be set here. This setting will take effect after restarting Gridcoin.", nullptr));
#endif // QT_CONFIG(tooltip)
        unitLabel->setText(QCoreApplication::translate("OptionsDialog", "&Unit to show amounts in:", nullptr));
#if QT_CONFIG(tooltip)
        unit->setToolTip(QCoreApplication::translate("OptionsDialog", "Choose the default subdivision unit to show in the interface and when sending coins.", nullptr));
#endif // QT_CONFIG(tooltip)
        styleLabel->setText(QCoreApplication::translate("OptionsDialog", "Style:", nullptr));
#if QT_CONFIG(tooltip)
        styleComboBox->setToolTip(QCoreApplication::translate("OptionsDialog", "Choose a stylesheet to change the look of the wallet.", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(tooltip)
        displayAddresses->setToolTip(QCoreApplication::translate("OptionsDialog", "Whether to show Gridcoin addresses in the transaction list or not.", nullptr));
#endif // QT_CONFIG(tooltip)
        displayAddresses->setText(QCoreApplication::translate("OptionsDialog", "&Display addresses in transaction list", nullptr));
        limitTxnDisplayCheckBox->setText(QCoreApplication::translate("OptionsDialog", "Only display transactions on or after ", nullptr));
#if QT_CONFIG(tooltip)
        limitTxnDisplayDateEdit->setToolTip(QCoreApplication::translate("OptionsDialog", "Setting this will cause the transaction table to only display transactions created on or after this date.", nullptr));
#endif // QT_CONFIG(tooltip)
        tabWidget->setTabText(tabWidget->indexOf(tabDisplay), QCoreApplication::translate("OptionsDialog", "&Display", nullptr));
        statusLabel->setText(QString());
        okButton->setText(QCoreApplication::translate("OptionsDialog", "&OK", nullptr));
        cancelButton->setText(QCoreApplication::translate("OptionsDialog", "&Cancel", nullptr));
        applyButton->setText(QCoreApplication::translate("OptionsDialog", "&Apply", nullptr));
    } // retranslateUi

};

namespace Ui {
    class OptionsDialog: public Ui_OptionsDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_OPTIONSDIALOG_H
