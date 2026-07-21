/********************************************************************************
** Form generated from reading UI file 'addressbookpage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ADDRESSBOOKPAGE_H
#define UI_ADDRESSBOOKPAGE_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTableView>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_AddressBookPage
{
public:
    QVBoxLayout *verticalLayout;
    QFrame *wrapperFrame;
    QVBoxLayout *verticalLayout_2;
    QLabel *explanationLabel;
    QTableView *tableView;
    QFrame *buttonFrame;
    QHBoxLayout *horizontalLayout;
    QPushButton *newAddressButton;
    QPushButton *addExistingButton;
    QPushButton *copyToClipboardButton;
    QPushButton *showQRCodeButton;
    QPushButton *signMessageButton;
    QPushButton *verifyMessageButton;
    QPushButton *deleteButton;
    QSpacerItem *horizontalSpacer;
    QDialogButtonBox *okayButtonBox;

    void setupUi(QWidget *AddressBookPage)
    {
        if (AddressBookPage->objectName().isEmpty())
            AddressBookPage->setObjectName("AddressBookPage");
        AddressBookPage->resize(835, 380);
        verticalLayout = new QVBoxLayout(AddressBookPage);
        verticalLayout->setSpacing(0);
        verticalLayout->setObjectName("verticalLayout");
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        wrapperFrame = new QFrame(AddressBookPage);
        wrapperFrame->setObjectName("wrapperFrame");
        wrapperFrame->setFrameShape(QFrame::StyledPanel);
        wrapperFrame->setFrameShadow(QFrame::Raised);
        verticalLayout_2 = new QVBoxLayout(wrapperFrame);
        verticalLayout_2->setSpacing(9);
        verticalLayout_2->setObjectName("verticalLayout_2");
        verticalLayout_2->setContentsMargins(0, 0, 0, 0);
        explanationLabel = new QLabel(wrapperFrame);
        explanationLabel->setObjectName("explanationLabel");
        explanationLabel->setTextFormat(Qt::PlainText);
        explanationLabel->setWordWrap(true);

        verticalLayout_2->addWidget(explanationLabel);

        tableView = new QTableView(wrapperFrame);
        tableView->setObjectName("tableView");
        tableView->setContextMenuPolicy(Qt::CustomContextMenu);
        tableView->setTabKeyNavigation(false);
        tableView->setAlternatingRowColors(true);
        tableView->setSelectionMode(QAbstractItemView::SingleSelection);
        tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
        tableView->setShowGrid(false);
        tableView->setSortingEnabled(true);
        tableView->horizontalHeader()->setHighlightSections(false);
        tableView->verticalHeader()->setVisible(false);

        verticalLayout_2->addWidget(tableView);


        verticalLayout->addWidget(wrapperFrame);

        buttonFrame = new QFrame(AddressBookPage);
        buttonFrame->setObjectName("buttonFrame");
        buttonFrame->setProperty("buttonFrame", QVariant(true));
        horizontalLayout = new QHBoxLayout(buttonFrame);
        horizontalLayout->setObjectName("horizontalLayout");
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        newAddressButton = new QPushButton(buttonFrame);
        newAddressButton->setObjectName("newAddressButton");
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icons/add"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        newAddressButton->setIcon(icon);

        horizontalLayout->addWidget(newAddressButton);

        addExistingButton = new QPushButton(buttonFrame);
        addExistingButton->setObjectName("addExistingButton");
        addExistingButton->setIcon(icon);

        horizontalLayout->addWidget(addExistingButton);

        copyToClipboardButton = new QPushButton(buttonFrame);
        copyToClipboardButton->setObjectName("copyToClipboardButton");
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/icons/editcopy"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        copyToClipboardButton->setIcon(icon1);

        horizontalLayout->addWidget(copyToClipboardButton);

        showQRCodeButton = new QPushButton(buttonFrame);
        showQRCodeButton->setObjectName("showQRCodeButton");
        QIcon icon2;
        icon2.addFile(QString::fromUtf8(":/icons/qrcode"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        showQRCodeButton->setIcon(icon2);

        horizontalLayout->addWidget(showQRCodeButton);

        signMessageButton = new QPushButton(buttonFrame);
        signMessageButton->setObjectName("signMessageButton");
        QIcon icon3;
        icon3.addFile(QString::fromUtf8(":/icons/edit"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        signMessageButton->setIcon(icon3);

        horizontalLayout->addWidget(signMessageButton);

        verifyMessageButton = new QPushButton(buttonFrame);
        verifyMessageButton->setObjectName("verifyMessageButton");
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/icons/transaction_0"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        verifyMessageButton->setIcon(icon4);

        horizontalLayout->addWidget(verifyMessageButton);

        deleteButton = new QPushButton(buttonFrame);
        deleteButton->setObjectName("deleteButton");
        QIcon icon5;
        icon5.addFile(QString::fromUtf8(":/icons/remove"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        deleteButton->setIcon(icon5);

        horizontalLayout->addWidget(deleteButton);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        okayButtonBox = new QDialogButtonBox(buttonFrame);
        okayButtonBox->setObjectName("okayButtonBox");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Maximum, QSizePolicy::Policy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(okayButtonBox->sizePolicy().hasHeightForWidth());
        okayButtonBox->setSizePolicy(sizePolicy);
        okayButtonBox->setStandardButtons(QDialogButtonBox::Ok);

        horizontalLayout->addWidget(okayButtonBox);


        verticalLayout->addWidget(buttonFrame);


        retranslateUi(AddressBookPage);

        QMetaObject::connectSlotsByName(AddressBookPage);
    } // setupUi

    void retranslateUi(QWidget *AddressBookPage)
    {
        AddressBookPage->setWindowTitle(QCoreApplication::translate("AddressBookPage", "Address Book", nullptr));
        explanationLabel->setText(QCoreApplication::translate("AddressBookPage", "These are your Gridcoin addresses for receiving payments. You may want to give a different one to each sender so you can keep track of who is paying you.", nullptr));
#if QT_CONFIG(tooltip)
        tableView->setToolTip(QCoreApplication::translate("AddressBookPage", "Double-click to edit label", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(tooltip)
        newAddressButton->setToolTip(QCoreApplication::translate("AddressBookPage", "Create a new address", nullptr));
#endif // QT_CONFIG(tooltip)
        newAddressButton->setText(QCoreApplication::translate("AddressBookPage", "&New", nullptr));
#if QT_CONFIG(tooltip)
        addExistingButton->setToolTip(QCoreApplication::translate("AddressBookPage", "Add an existing address owned by this wallet to the address book", nullptr));
#endif // QT_CONFIG(tooltip)
        addExistingButton->setText(QCoreApplication::translate("AddressBookPage", "Add &Existing", nullptr));
#if QT_CONFIG(tooltip)
        copyToClipboardButton->setToolTip(QCoreApplication::translate("AddressBookPage", "Copy the currently selected address to the system clipboard", nullptr));
#endif // QT_CONFIG(tooltip)
        copyToClipboardButton->setText(QCoreApplication::translate("AddressBookPage", "&Copy", nullptr));
        showQRCodeButton->setText(QCoreApplication::translate("AddressBookPage", "Show &QR Code", nullptr));
#if QT_CONFIG(tooltip)
        signMessageButton->setToolTip(QCoreApplication::translate("AddressBookPage", "Sign a message to prove you own a Gridcoin address", nullptr));
#endif // QT_CONFIG(tooltip)
        signMessageButton->setText(QCoreApplication::translate("AddressBookPage", "Sign &Message", nullptr));
#if QT_CONFIG(tooltip)
        verifyMessageButton->setToolTip(QCoreApplication::translate("AddressBookPage", "Verify a message to ensure it was signed with a specified Gridcoin address", nullptr));
#endif // QT_CONFIG(tooltip)
        verifyMessageButton->setText(QCoreApplication::translate("AddressBookPage", "&Verify Message", nullptr));
#if QT_CONFIG(tooltip)
        deleteButton->setToolTip(QCoreApplication::translate("AddressBookPage", "Delete the currently selected address from the list", nullptr));
#endif // QT_CONFIG(tooltip)
        deleteButton->setText(QCoreApplication::translate("AddressBookPage", "&Delete", nullptr));
    } // retranslateUi

};

namespace Ui {
    class AddressBookPage: public Ui_AddressBookPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ADDRESSBOOKPAGE_H
