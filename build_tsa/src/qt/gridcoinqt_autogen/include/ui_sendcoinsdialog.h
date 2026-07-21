/********************************************************************************
** Form generated from reading UI file 'sendcoinsdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SENDCOINSDIALOG_H
#define UI_SENDCOINSDIALOG_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_SendCoinsDialog
{
public:
    QVBoxLayout *sendCoinsVerticalLayout;
    QFrame *headerFrame;
    QHBoxLayout *headerFrameLayout;
    QWidget *headerTitleWrapper;
    QVBoxLayout *headerTitleVerticalLayout;
    QLabel *headerTitleLabel;
    QSpacerItem *headerFrameSpacer;
    QWidget *headerBalanceWrapper;
    QVBoxLayout *headerBalanceVerticalLayout;
    QLabel *headerBalanceLabel;
    QLabel *headerBalanceCaptionLabel;
    QFrame *coinControlWrapper;
    QVBoxLayout *verticalLayoutCoinControl2;
    QHBoxLayout *horizontalLayoutCoinControl1;
    QPushButton *coinControlFeaturesButton;
    QLabel *coinControlStatusIconLabel;
    QLabel *coinControlStatusLabel;
    QWidget *coinControlContentWidget;
    QVBoxLayout *coinControlContentVerticalLayout;
    QHBoxLayout *horizontalLayoutCoinControl2;
    QPushButton *coinControlPushButton;
    QLabel *coinControlAutomaticallySelectedLabel;
    QLabel *coinControlInsuffFundsLabel;
    QPushButton *coinControlResetPushButton;
    QPushButton *coinControlConsolidateWizardPushButton;
    QSpacerItem *coinControlHorizontalSpacer;
    QWidget *widgetCoinControl;
    QHBoxLayout *horizontalLayoutCoinControl5;
    QHBoxLayout *horizontalLayoutCoinControl3;
    QFormLayout *formLayoutCoinControl1;
    QLabel *coinControlQuantityTextLabel;
    QLabel *coinControlQuantityLabel;
    QLabel *coinControlBytesTextLabel;
    QLabel *coinControlBytesLabel;
    QFormLayout *formLayoutCoinControl2;
    QLabel *coinControlAmountTextLabel;
    QLabel *coinControlAmountLabel;
    QFormLayout *formLayoutCoinControl3;
    QLabel *coinControlFeeTextLabel;
    QLabel *coinControlFeeLabel;
    QLabel *coinControlLowOutputTextLabel;
    QLabel *coinControlLowOutputLabel;
    QFormLayout *formLayoutCoinControl4;
    QLabel *coinControlAfterFeeTextLabel;
    QLabel *coinControlAfterFeeLabel;
    QLabel *coinControlChangeTextLabel;
    QLabel *coinControlChangeLabel;
    QHBoxLayout *horizontalLayoutCoinControl4;
    QCheckBox *coinControlChangeCheckBox;
    QLineEdit *coinControlChangeEdit;
    QLabel *coinControlChangeAddressLabel;
    QScrollArea *scrollArea;
    QWidget *scrollAreaWidgetContents;
    QVBoxLayout *verticalLayout_2;
    QVBoxLayout *entries;
    QSpacerItem *verticalSpacer;
    QFrame *actionButtonsFrame;
    QHBoxLayout *horizontalLayout;
    QPushButton *addButton;
    QPushButton *clearButton;
    QSpacerItem *horizontalSpacer;
    QPushButton *sendButton;

    void setupUi(QDialog *SendCoinsDialog)
    {
        if (SendCoinsDialog->objectName().isEmpty())
            SendCoinsDialog->setObjectName("SendCoinsDialog");
        SendCoinsDialog->resize(899, 456);
        sendCoinsVerticalLayout = new QVBoxLayout(SendCoinsDialog);
        sendCoinsVerticalLayout->setSpacing(0);
        sendCoinsVerticalLayout->setObjectName("sendCoinsVerticalLayout");
        sendCoinsVerticalLayout->setContentsMargins(0, 0, 0, 0);
        headerFrame = new QFrame(SendCoinsDialog);
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


        headerFrameLayout->addWidget(headerBalanceWrapper, 0, Qt::AlignVCenter);


        sendCoinsVerticalLayout->addWidget(headerFrame, 0, Qt::AlignVCenter);

        coinControlWrapper = new QFrame(SendCoinsDialog);
        coinControlWrapper->setObjectName("coinControlWrapper");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(coinControlWrapper->sizePolicy().hasHeightForWidth());
        coinControlWrapper->setSizePolicy(sizePolicy);
        coinControlWrapper->setMaximumSize(QSize(16777215, 16777215));
        verticalLayoutCoinControl2 = new QVBoxLayout(coinControlWrapper);
        verticalLayoutCoinControl2->setSpacing(6);
        verticalLayoutCoinControl2->setObjectName("verticalLayoutCoinControl2");
        verticalLayoutCoinControl2->setContentsMargins(0, 0, 0, 0);
        horizontalLayoutCoinControl1 = new QHBoxLayout();
        horizontalLayoutCoinControl1->setSpacing(3);
        horizontalLayoutCoinControl1->setObjectName("horizontalLayoutCoinControl1");
        coinControlFeaturesButton = new QPushButton(coinControlWrapper);
        coinControlFeaturesButton->setObjectName("coinControlFeaturesButton");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(coinControlFeaturesButton->sizePolicy().hasHeightForWidth());
        coinControlFeaturesButton->setSizePolicy(sizePolicy1);

        horizontalLayoutCoinControl1->addWidget(coinControlFeaturesButton);

        coinControlStatusIconLabel = new QLabel(coinControlWrapper);
        coinControlStatusIconLabel->setObjectName("coinControlStatusIconLabel");
        coinControlStatusIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_gray_x")));

        horizontalLayoutCoinControl1->addWidget(coinControlStatusIconLabel, 0, Qt::AlignVCenter);

        coinControlStatusLabel = new QLabel(coinControlWrapper);
        coinControlStatusLabel->setObjectName("coinControlStatusLabel");

        horizontalLayoutCoinControl1->addWidget(coinControlStatusLabel, 0, Qt::AlignVCenter);


        verticalLayoutCoinControl2->addLayout(horizontalLayoutCoinControl1);

        coinControlContentWidget = new QWidget(coinControlWrapper);
        coinControlContentWidget->setObjectName("coinControlContentWidget");
        coinControlContentVerticalLayout = new QVBoxLayout(coinControlContentWidget);
        coinControlContentVerticalLayout->setObjectName("coinControlContentVerticalLayout");
        coinControlContentVerticalLayout->setContentsMargins(0, 0, 0, 0);
        horizontalLayoutCoinControl2 = new QHBoxLayout();
        horizontalLayoutCoinControl2->setSpacing(8);
        horizontalLayoutCoinControl2->setObjectName("horizontalLayoutCoinControl2");
        horizontalLayoutCoinControl2->setContentsMargins(-1, 10, -1, 10);
        coinControlPushButton = new QPushButton(coinControlContentWidget);
        coinControlPushButton->setObjectName("coinControlPushButton");
        coinControlPushButton->setStyleSheet(QString::fromUtf8(""));

        horizontalLayoutCoinControl2->addWidget(coinControlPushButton);

        coinControlAutomaticallySelectedLabel = new QLabel(coinControlContentWidget);
        coinControlAutomaticallySelectedLabel->setObjectName("coinControlAutomaticallySelectedLabel");
        coinControlAutomaticallySelectedLabel->setMargin(5);

        horizontalLayoutCoinControl2->addWidget(coinControlAutomaticallySelectedLabel);

        coinControlInsuffFundsLabel = new QLabel(coinControlContentWidget);
        coinControlInsuffFundsLabel->setObjectName("coinControlInsuffFundsLabel");
        coinControlInsuffFundsLabel->setMargin(5);

        horizontalLayoutCoinControl2->addWidget(coinControlInsuffFundsLabel);

        coinControlResetPushButton = new QPushButton(coinControlContentWidget);
        coinControlResetPushButton->setObjectName("coinControlResetPushButton");

        horizontalLayoutCoinControl2->addWidget(coinControlResetPushButton);

        coinControlConsolidateWizardPushButton = new QPushButton(coinControlContentWidget);
        coinControlConsolidateWizardPushButton->setObjectName("coinControlConsolidateWizardPushButton");

        horizontalLayoutCoinControl2->addWidget(coinControlConsolidateWizardPushButton);

        coinControlHorizontalSpacer = new QSpacerItem(40, 1, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayoutCoinControl2->addItem(coinControlHorizontalSpacer);


        coinControlContentVerticalLayout->addLayout(horizontalLayoutCoinControl2);

        widgetCoinControl = new QWidget(coinControlContentWidget);
        widgetCoinControl->setObjectName("widgetCoinControl");
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(widgetCoinControl->sizePolicy().hasHeightForWidth());
        widgetCoinControl->setSizePolicy(sizePolicy2);
        widgetCoinControl->setMinimumSize(QSize(0, 0));
        widgetCoinControl->setStyleSheet(QString::fromUtf8(""));
        horizontalLayoutCoinControl5 = new QHBoxLayout(widgetCoinControl);
        horizontalLayoutCoinControl5->setObjectName("horizontalLayoutCoinControl5");
        horizontalLayoutCoinControl5->setContentsMargins(0, 0, 0, 0);
        horizontalLayoutCoinControl3 = new QHBoxLayout();
        horizontalLayoutCoinControl3->setSpacing(20);
        horizontalLayoutCoinControl3->setObjectName("horizontalLayoutCoinControl3");
        horizontalLayoutCoinControl3->setContentsMargins(-1, 0, -1, 10);
        formLayoutCoinControl1 = new QFormLayout();
        formLayoutCoinControl1->setObjectName("formLayoutCoinControl1");
        formLayoutCoinControl1->setHorizontalSpacing(10);
        formLayoutCoinControl1->setVerticalSpacing(14);
        formLayoutCoinControl1->setContentsMargins(10, 4, 6, -1);
        coinControlQuantityTextLabel = new QLabel(widgetCoinControl);
        coinControlQuantityTextLabel->setObjectName("coinControlQuantityTextLabel");
        coinControlQuantityTextLabel->setMargin(0);

        formLayoutCoinControl1->setWidget(0, QFormLayout::ItemRole::LabelRole, coinControlQuantityTextLabel);

        coinControlQuantityLabel = new QLabel(widgetCoinControl);
        coinControlQuantityLabel->setObjectName("coinControlQuantityLabel");
        coinControlQuantityLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        coinControlQuantityLabel->setContextMenuPolicy(Qt::ActionsContextMenu);
        coinControlQuantityLabel->setMargin(0);
        coinControlQuantityLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        formLayoutCoinControl1->setWidget(0, QFormLayout::ItemRole::FieldRole, coinControlQuantityLabel);

        coinControlBytesTextLabel = new QLabel(widgetCoinControl);
        coinControlBytesTextLabel->setObjectName("coinControlBytesTextLabel");

        formLayoutCoinControl1->setWidget(1, QFormLayout::ItemRole::LabelRole, coinControlBytesTextLabel);

        coinControlBytesLabel = new QLabel(widgetCoinControl);
        coinControlBytesLabel->setObjectName("coinControlBytesLabel");
        coinControlBytesLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        coinControlBytesLabel->setContextMenuPolicy(Qt::ActionsContextMenu);
        coinControlBytesLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        formLayoutCoinControl1->setWidget(1, QFormLayout::ItemRole::FieldRole, coinControlBytesLabel);


        horizontalLayoutCoinControl3->addLayout(formLayoutCoinControl1);

        formLayoutCoinControl2 = new QFormLayout();
        formLayoutCoinControl2->setObjectName("formLayoutCoinControl2");
        formLayoutCoinControl2->setHorizontalSpacing(10);
        formLayoutCoinControl2->setVerticalSpacing(14);
        formLayoutCoinControl2->setContentsMargins(6, 4, 6, -1);
        coinControlAmountTextLabel = new QLabel(widgetCoinControl);
        coinControlAmountTextLabel->setObjectName("coinControlAmountTextLabel");
        coinControlAmountTextLabel->setMargin(0);

        formLayoutCoinControl2->setWidget(0, QFormLayout::ItemRole::LabelRole, coinControlAmountTextLabel);

        coinControlAmountLabel = new QLabel(widgetCoinControl);
        coinControlAmountLabel->setObjectName("coinControlAmountLabel");
        coinControlAmountLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        coinControlAmountLabel->setContextMenuPolicy(Qt::ActionsContextMenu);
        coinControlAmountLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        formLayoutCoinControl2->setWidget(0, QFormLayout::ItemRole::FieldRole, coinControlAmountLabel);


        horizontalLayoutCoinControl3->addLayout(formLayoutCoinControl2);

        formLayoutCoinControl3 = new QFormLayout();
        formLayoutCoinControl3->setObjectName("formLayoutCoinControl3");
        formLayoutCoinControl3->setHorizontalSpacing(10);
        formLayoutCoinControl3->setVerticalSpacing(14);
        formLayoutCoinControl3->setContentsMargins(6, 4, 6, -1);
        coinControlFeeTextLabel = new QLabel(widgetCoinControl);
        coinControlFeeTextLabel->setObjectName("coinControlFeeTextLabel");
        coinControlFeeTextLabel->setMargin(0);

        formLayoutCoinControl3->setWidget(0, QFormLayout::ItemRole::LabelRole, coinControlFeeTextLabel);

        coinControlFeeLabel = new QLabel(widgetCoinControl);
        coinControlFeeLabel->setObjectName("coinControlFeeLabel");
        coinControlFeeLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        coinControlFeeLabel->setContextMenuPolicy(Qt::ActionsContextMenu);
        coinControlFeeLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        formLayoutCoinControl3->setWidget(0, QFormLayout::ItemRole::FieldRole, coinControlFeeLabel);

        coinControlLowOutputTextLabel = new QLabel(widgetCoinControl);
        coinControlLowOutputTextLabel->setObjectName("coinControlLowOutputTextLabel");

        formLayoutCoinControl3->setWidget(1, QFormLayout::ItemRole::LabelRole, coinControlLowOutputTextLabel);

        coinControlLowOutputLabel = new QLabel(widgetCoinControl);
        coinControlLowOutputLabel->setObjectName("coinControlLowOutputLabel");
        coinControlLowOutputLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        coinControlLowOutputLabel->setContextMenuPolicy(Qt::ActionsContextMenu);
        coinControlLowOutputLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        formLayoutCoinControl3->setWidget(1, QFormLayout::ItemRole::FieldRole, coinControlLowOutputLabel);


        horizontalLayoutCoinControl3->addLayout(formLayoutCoinControl3);

        formLayoutCoinControl4 = new QFormLayout();
        formLayoutCoinControl4->setObjectName("formLayoutCoinControl4");
        formLayoutCoinControl4->setHorizontalSpacing(10);
        formLayoutCoinControl4->setVerticalSpacing(14);
        formLayoutCoinControl4->setContentsMargins(6, 4, 6, -1);
        coinControlAfterFeeTextLabel = new QLabel(widgetCoinControl);
        coinControlAfterFeeTextLabel->setObjectName("coinControlAfterFeeTextLabel");
        coinControlAfterFeeTextLabel->setMargin(0);

        formLayoutCoinControl4->setWidget(0, QFormLayout::ItemRole::LabelRole, coinControlAfterFeeTextLabel);

        coinControlAfterFeeLabel = new QLabel(widgetCoinControl);
        coinControlAfterFeeLabel->setObjectName("coinControlAfterFeeLabel");
        coinControlAfterFeeLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        coinControlAfterFeeLabel->setContextMenuPolicy(Qt::ActionsContextMenu);
        coinControlAfterFeeLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        formLayoutCoinControl4->setWidget(0, QFormLayout::ItemRole::FieldRole, coinControlAfterFeeLabel);

        coinControlChangeTextLabel = new QLabel(widgetCoinControl);
        coinControlChangeTextLabel->setObjectName("coinControlChangeTextLabel");

        formLayoutCoinControl4->setWidget(1, QFormLayout::ItemRole::LabelRole, coinControlChangeTextLabel);

        coinControlChangeLabel = new QLabel(widgetCoinControl);
        coinControlChangeLabel->setObjectName("coinControlChangeLabel");
        coinControlChangeLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        coinControlChangeLabel->setContextMenuPolicy(Qt::ActionsContextMenu);
        coinControlChangeLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        formLayoutCoinControl4->setWidget(1, QFormLayout::ItemRole::FieldRole, coinControlChangeLabel);


        horizontalLayoutCoinControl3->addLayout(formLayoutCoinControl4);

        horizontalLayoutCoinControl3->setStretch(3, 1);

        horizontalLayoutCoinControl5->addLayout(horizontalLayoutCoinControl3);


        coinControlContentVerticalLayout->addWidget(widgetCoinControl);

        horizontalLayoutCoinControl4 = new QHBoxLayout();
        horizontalLayoutCoinControl4->setSpacing(12);
        horizontalLayoutCoinControl4->setObjectName("horizontalLayoutCoinControl4");
        horizontalLayoutCoinControl4->setSizeConstraint(QLayout::SetDefaultConstraint);
        horizontalLayoutCoinControl4->setContentsMargins(-1, 5, 5, -1);
        coinControlChangeCheckBox = new QCheckBox(coinControlContentWidget);
        coinControlChangeCheckBox->setObjectName("coinControlChangeCheckBox");

        horizontalLayoutCoinControl4->addWidget(coinControlChangeCheckBox);

        coinControlChangeEdit = new QLineEdit(coinControlContentWidget);
        coinControlChangeEdit->setObjectName("coinControlChangeEdit");
        coinControlChangeEdit->setEnabled(false);
        sizePolicy1.setHeightForWidth(coinControlChangeEdit->sizePolicy().hasHeightForWidth());
        coinControlChangeEdit->setSizePolicy(sizePolicy1);

        horizontalLayoutCoinControl4->addWidget(coinControlChangeEdit);

        coinControlChangeAddressLabel = new QLabel(coinControlContentWidget);
        coinControlChangeAddressLabel->setObjectName("coinControlChangeAddressLabel");
        QSizePolicy sizePolicy3(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Expanding);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(coinControlChangeAddressLabel->sizePolicy().hasHeightForWidth());
        coinControlChangeAddressLabel->setSizePolicy(sizePolicy3);
        coinControlChangeAddressLabel->setMinimumSize(QSize(0, 0));
        coinControlChangeAddressLabel->setMargin(3);

        horizontalLayoutCoinControl4->addWidget(coinControlChangeAddressLabel);


        coinControlContentVerticalLayout->addLayout(horizontalLayoutCoinControl4);


        verticalLayoutCoinControl2->addWidget(coinControlContentWidget);


        sendCoinsVerticalLayout->addWidget(coinControlWrapper, 0, Qt::AlignTop);

        scrollArea = new QScrollArea(SendCoinsDialog);
        scrollArea->setObjectName("scrollArea");
        scrollArea->setWidgetResizable(true);
        scrollAreaWidgetContents = new QWidget();
        scrollAreaWidgetContents->setObjectName("scrollAreaWidgetContents");
        scrollAreaWidgetContents->setGeometry(QRect(0, 0, 897, 223));
        verticalLayout_2 = new QVBoxLayout(scrollAreaWidgetContents);
        verticalLayout_2->setSpacing(9);
        verticalLayout_2->setObjectName("verticalLayout_2");
        verticalLayout_2->setContentsMargins(12, 12, 12, 12);
        entries = new QVBoxLayout();
        entries->setSpacing(9);
        entries->setObjectName("entries");

        verticalLayout_2->addLayout(entries);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_2->addItem(verticalSpacer);

        scrollArea->setWidget(scrollAreaWidgetContents);

        sendCoinsVerticalLayout->addWidget(scrollArea);

        actionButtonsFrame = new QFrame(SendCoinsDialog);
        actionButtonsFrame->setObjectName("actionButtonsFrame");
        horizontalLayout = new QHBoxLayout(actionButtonsFrame);
        horizontalLayout->setObjectName("horizontalLayout");
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        addButton = new QPushButton(actionButtonsFrame);
        addButton->setObjectName("addButton");
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icons/add"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        addButton->setIcon(icon);
        addButton->setAutoDefault(false);

        horizontalLayout->addWidget(addButton);

        clearButton = new QPushButton(actionButtonsFrame);
        clearButton->setObjectName("clearButton");
        QSizePolicy sizePolicy4(QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);
        sizePolicy4.setHorizontalStretch(0);
        sizePolicy4.setVerticalStretch(0);
        sizePolicy4.setHeightForWidth(clearButton->sizePolicy().hasHeightForWidth());
        clearButton->setSizePolicy(sizePolicy4);
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/icons/remove"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        clearButton->setIcon(icon1);
        clearButton->setAutoRepeatDelay(300);
        clearButton->setAutoDefault(false);

        horizontalLayout->addWidget(clearButton);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        sendButton = new QPushButton(actionButtonsFrame);
        sendButton->setObjectName("sendButton");
        sendButton->setMinimumSize(QSize(150, 0));

        horizontalLayout->addWidget(sendButton);


        sendCoinsVerticalLayout->addWidget(actionButtonsFrame);

#if QT_CONFIG(shortcut)
        headerTitleLabel->setBuddy(actionButtonsFrame);
#endif // QT_CONFIG(shortcut)

        retranslateUi(SendCoinsDialog);

        sendButton->setDefault(true);


        QMetaObject::connectSlotsByName(SendCoinsDialog);
    } // setupUi

    void retranslateUi(QDialog *SendCoinsDialog)
    {
        SendCoinsDialog->setWindowTitle(QCoreApplication::translate("SendCoinsDialog", "Send Coins", nullptr));
        headerTitleLabel->setText(QCoreApplication::translate("SendCoinsDialog", "Send Payment", nullptr));
        headerBalanceLabel->setText(QCoreApplication::translate("SendCoinsDialog", "0.00", nullptr));
        headerBalanceCaptionLabel->setText(QCoreApplication::translate("SendCoinsDialog", "Available (%1)", nullptr));
        coinControlFeaturesButton->setText(QCoreApplication::translate("SendCoinsDialog", "Coin Control Features (Advanced)", nullptr));
        coinControlStatusLabel->setText(QCoreApplication::translate("SendCoinsDialog", "Inactive", nullptr));
        coinControlPushButton->setText(QCoreApplication::translate("SendCoinsDialog", "Inputs...", nullptr));
        coinControlAutomaticallySelectedLabel->setText(QCoreApplication::translate("SendCoinsDialog", "automatically selected", nullptr));
        coinControlInsuffFundsLabel->setText(QCoreApplication::translate("SendCoinsDialog", "Insufficient funds!", nullptr));
        coinControlResetPushButton->setText(QCoreApplication::translate("SendCoinsDialog", "Reset", nullptr));
        coinControlConsolidateWizardPushButton->setText(QCoreApplication::translate("SendCoinsDialog", "Consolidate Wizard", nullptr));
        coinControlQuantityTextLabel->setText(QCoreApplication::translate("SendCoinsDialog", "Quantity:", nullptr));
        coinControlQuantityLabel->setText(QCoreApplication::translate("SendCoinsDialog", "0", nullptr));
        coinControlBytesTextLabel->setText(QCoreApplication::translate("SendCoinsDialog", "Bytes:", nullptr));
        coinControlBytesLabel->setText(QCoreApplication::translate("SendCoinsDialog", "0", nullptr));
        coinControlAmountTextLabel->setText(QCoreApplication::translate("SendCoinsDialog", "Amount:", nullptr));
        coinControlAmountLabel->setText(QCoreApplication::translate("SendCoinsDialog", "0.00 GRC", nullptr));
        coinControlFeeTextLabel->setText(QCoreApplication::translate("SendCoinsDialog", "Fee:", nullptr));
        coinControlFeeLabel->setText(QCoreApplication::translate("SendCoinsDialog", "0.00 GRC", nullptr));
        coinControlLowOutputTextLabel->setText(QCoreApplication::translate("SendCoinsDialog", "Low Output:", nullptr));
        coinControlLowOutputLabel->setText(QCoreApplication::translate("SendCoinsDialog", "no", nullptr));
        coinControlAfterFeeTextLabel->setText(QCoreApplication::translate("SendCoinsDialog", "After Fee:", nullptr));
        coinControlAfterFeeLabel->setText(QCoreApplication::translate("SendCoinsDialog", "0.00 GRC", nullptr));
        coinControlChangeTextLabel->setText(QCoreApplication::translate("SendCoinsDialog", "Change", nullptr));
        coinControlChangeLabel->setText(QCoreApplication::translate("SendCoinsDialog", "0.00 GRC", nullptr));
        coinControlChangeCheckBox->setText(QCoreApplication::translate("SendCoinsDialog", "custom change address", nullptr));
        coinControlChangeEdit->setPlaceholderText(QCoreApplication::translate("SendCoinsDialog", "Enter a Gridcoin address (e.g. S67nL4vELWwdDVzjgtEP4MxryarTZ9a8GB)", nullptr));
        coinControlChangeAddressLabel->setText(QString());
#if QT_CONFIG(tooltip)
        addButton->setToolTip(QCoreApplication::translate("SendCoinsDialog", "Send to multiple recipients at once", nullptr));
#endif // QT_CONFIG(tooltip)
        addButton->setText(QCoreApplication::translate("SendCoinsDialog", "Add &Recipient", nullptr));
#if QT_CONFIG(tooltip)
        clearButton->setToolTip(QCoreApplication::translate("SendCoinsDialog", "Remove all transaction fields", nullptr));
#endif // QT_CONFIG(tooltip)
        clearButton->setText(QCoreApplication::translate("SendCoinsDialog", "Clear &All", nullptr));
#if QT_CONFIG(tooltip)
        sendButton->setToolTip(QCoreApplication::translate("SendCoinsDialog", "Confirm the send action", nullptr));
#endif // QT_CONFIG(tooltip)
        sendButton->setText(QCoreApplication::translate("SendCoinsDialog", "S&end", nullptr));
    } // retranslateUi

};

namespace Ui {
    class SendCoinsDialog: public Ui_SendCoinsDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SENDCOINSDIALOG_H
