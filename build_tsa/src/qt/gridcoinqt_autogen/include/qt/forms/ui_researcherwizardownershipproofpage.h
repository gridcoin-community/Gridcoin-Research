/********************************************************************************
** Form generated from reading UI file 'researcherwizardownershipproofpage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_RESEARCHERWIZARDOWNERSHIPPROOFPAGE_H
#define UI_RESEARCHERWIZARDOWNERSHIPPROOFPAGE_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <QtWidgets/QWizardPage>

QT_BEGIN_NAMESPACE

class Ui_ResearcherWizardOwnershipProofPage
{
public:
    QVBoxLayout *ownershipProofPageLayout;
    QScrollArea *contentScrollArea;
    QWidget *contentScrollAreaContents;
    QVBoxLayout *contentLayout;
    QLabel *pubkeyHeaderLabel;
    QHBoxLayout *pubkeyLayout;
    QLabel *pubkeyLabel;
    QPushButton *copyPubKeyButton;
    QSpacerItem *pubkeySpacer;
    QSpacerItem *sectionSpacer1;
    QLabel *projectListHeaderLabel;
    QLabel *projectListLabel;
    QSpacerItem *sectionSpacer2;
    QLabel *step1Label;
    QLabel *step2Label;
    QLabel *step3Label;
    QPlainTextEdit *ownershipProofXmlEdit;
    QHBoxLayout *sendLayout;
    QPushButton *sendBeaconButton;
    QLabel *statusIconLabel;
    QLabel *statusLabel;
    QSpacerItem *sendSpacer;
    QSpacerItem *sectionSpacer3;
    QLabel *rememberLabel;
    QSpacerItem *footerSpacer;

    void setupUi(QWizardPage *ResearcherWizardOwnershipProofPage)
    {
        if (ResearcherWizardOwnershipProofPage->objectName().isEmpty())
            ResearcherWizardOwnershipProofPage->setObjectName("ResearcherWizardOwnershipProofPage");
        ResearcherWizardOwnershipProofPage->resize(630, 480);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(ResearcherWizardOwnershipProofPage->sizePolicy().hasHeightForWidth());
        ResearcherWizardOwnershipProofPage->setSizePolicy(sizePolicy);
        ownershipProofPageLayout = new QVBoxLayout(ResearcherWizardOwnershipProofPage);
        ownershipProofPageLayout->setObjectName("ownershipProofPageLayout");
        contentScrollArea = new QScrollArea(ResearcherWizardOwnershipProofPage);
        contentScrollArea->setObjectName("contentScrollArea");
        contentScrollArea->setWidgetResizable(true);
        contentScrollArea->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        contentScrollAreaContents = new QWidget();
        contentScrollAreaContents->setObjectName("contentScrollAreaContents");
        contentScrollAreaContents->setGeometry(QRect(0, 0, 596, 700));
        contentLayout = new QVBoxLayout(contentScrollAreaContents);
        contentLayout->setObjectName("contentLayout");
        pubkeyHeaderLabel = new QLabel(contentScrollAreaContents);
        pubkeyHeaderLabel->setObjectName("pubkeyHeaderLabel");
        pubkeyHeaderLabel->setTextInteractionFlags(Qt::NoTextInteraction);

        contentLayout->addWidget(pubkeyHeaderLabel);

        pubkeyLayout = new QHBoxLayout();
        pubkeyLayout->setObjectName("pubkeyLayout");
        pubkeyLayout->setContentsMargins(16, -1, -1, -1);
        pubkeyLabel = new QLabel(contentScrollAreaContents);
        pubkeyLabel->setObjectName("pubkeyLabel");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(pubkeyLabel->sizePolicy().hasHeightForWidth());
        pubkeyLabel->setSizePolicy(sizePolicy1);
        QFont font;
        font.setFamilies({QString::fromUtf8("Monospace")});
        pubkeyLabel->setFont(font);
        pubkeyLabel->setText(QString::fromUtf8(""));
        pubkeyLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        pubkeyLayout->addWidget(pubkeyLabel, 0, Qt::AlignLeft|Qt::AlignVCenter);

        copyPubKeyButton = new QPushButton(contentScrollAreaContents);
        copyPubKeyButton->setObjectName("copyPubKeyButton");
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(copyPubKeyButton->sizePolicy().hasHeightForWidth());
        copyPubKeyButton->setSizePolicy(sizePolicy2);
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icons/editcopy"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        copyPubKeyButton->setIcon(icon);

        pubkeyLayout->addWidget(copyPubKeyButton, 0, Qt::AlignLeft|Qt::AlignVCenter);

        pubkeySpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        pubkeyLayout->addItem(pubkeySpacer);


        contentLayout->addLayout(pubkeyLayout);

        sectionSpacer1 = new QSpacerItem(20, 6, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        contentLayout->addItem(sectionSpacer1);

        projectListHeaderLabel = new QLabel(contentScrollAreaContents);
        projectListHeaderLabel->setObjectName("projectListHeaderLabel");
        projectListHeaderLabel->setTextInteractionFlags(Qt::NoTextInteraction);

        contentLayout->addWidget(projectListHeaderLabel);

        projectListLabel = new QLabel(contentScrollAreaContents);
        projectListLabel->setObjectName("projectListLabel");
        projectListLabel->setText(QString::fromUtf8(""));
        projectListLabel->setTextFormat(Qt::RichText);
        projectListLabel->setWordWrap(true);
        projectListLabel->setIndent(16);
        projectListLabel->setOpenExternalLinks(true);
        projectListLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        contentLayout->addWidget(projectListLabel);

        sectionSpacer2 = new QSpacerItem(20, 6, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        contentLayout->addItem(sectionSpacer2);

        step1Label = new QLabel(contentScrollAreaContents);
        step1Label->setObjectName("step1Label");
        step1Label->setWordWrap(true);
        step1Label->setTextInteractionFlags(Qt::TextSelectableByMouse);

        contentLayout->addWidget(step1Label);

        step2Label = new QLabel(contentScrollAreaContents);
        step2Label->setObjectName("step2Label");
        step2Label->setWordWrap(true);
        step2Label->setTextInteractionFlags(Qt::TextSelectableByMouse);

        contentLayout->addWidget(step2Label);

        step3Label = new QLabel(contentScrollAreaContents);
        step3Label->setObjectName("step3Label");
        step3Label->setWordWrap(true);
        step3Label->setTextInteractionFlags(Qt::TextSelectableByMouse);

        contentLayout->addWidget(step3Label);

        ownershipProofXmlEdit = new QPlainTextEdit(contentScrollAreaContents);
        ownershipProofXmlEdit->setObjectName("ownershipProofXmlEdit");
        QSizePolicy sizePolicy3(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Fixed);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(ownershipProofXmlEdit->sizePolicy().hasHeightForWidth());
        ownershipProofXmlEdit->setSizePolicy(sizePolicy3);
        ownershipProofXmlEdit->setMinimumSize(QSize(0, 100));
        ownershipProofXmlEdit->setMaximumSize(QSize(16777215, 120));
        ownershipProofXmlEdit->setFont(font);

        contentLayout->addWidget(ownershipProofXmlEdit);

        sendLayout = new QHBoxLayout();
        sendLayout->setObjectName("sendLayout");
        sendBeaconButton = new QPushButton(contentScrollAreaContents);
        sendBeaconButton->setObjectName("sendBeaconButton");
        sizePolicy2.setHeightForWidth(sendBeaconButton->sizePolicy().hasHeightForWidth());
        sendBeaconButton->setSizePolicy(sizePolicy2);

        sendLayout->addWidget(sendBeaconButton, 0, Qt::AlignLeft|Qt::AlignVCenter);

        statusIconLabel = new QLabel(contentScrollAreaContents);
        statusIconLabel->setObjectName("statusIconLabel");
        sizePolicy2.setHeightForWidth(statusIconLabel->sizePolicy().hasHeightForWidth());
        statusIconLabel->setSizePolicy(sizePolicy2);
        statusIconLabel->setMinimumSize(QSize(24, 24));
        statusIconLabel->setMaximumSize(QSize(24, 24));
        statusIconLabel->setText(QString::fromUtf8(""));
        statusIconLabel->setScaledContents(true);
        statusIconLabel->setTextInteractionFlags(Qt::NoTextInteraction);

        sendLayout->addWidget(statusIconLabel, 0, Qt::AlignLeft|Qt::AlignVCenter);

        statusLabel = new QLabel(contentScrollAreaContents);
        statusLabel->setObjectName("statusLabel");
        statusLabel->setText(QString::fromUtf8(""));
        statusLabel->setWordWrap(true);
        statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        sendLayout->addWidget(statusLabel);

        sendSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        sendLayout->addItem(sendSpacer);


        contentLayout->addLayout(sendLayout);

        sectionSpacer3 = new QSpacerItem(20, 10, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        contentLayout->addItem(sectionSpacer3);

        rememberLabel = new QLabel(contentScrollAreaContents);
        rememberLabel->setObjectName("rememberLabel");
        rememberLabel->setTextFormat(Qt::RichText);
        rememberLabel->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        rememberLabel->setWordWrap(true);
        rememberLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        contentLayout->addWidget(rememberLabel);

        footerSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        contentLayout->addItem(footerSpacer);

        contentScrollArea->setWidget(contentScrollAreaContents);

        ownershipProofPageLayout->addWidget(contentScrollArea);


        retranslateUi(ResearcherWizardOwnershipProofPage);

        QMetaObject::connectSlotsByName(ResearcherWizardOwnershipProofPage);
    } // setupUi

    void retranslateUi(QWizardPage *ResearcherWizardOwnershipProofPage)
    {
        ResearcherWizardOwnershipProofPage->setWindowTitle(QCoreApplication::translate("ResearcherWizardOwnershipProofPage", "BOINC Account Ownership Proof", nullptr));
        ResearcherWizardOwnershipProofPage->setTitle(QCoreApplication::translate("ResearcherWizardOwnershipProofPage", "BOINC Account Ownership Proof", nullptr));
        ResearcherWizardOwnershipProofPage->setSubTitle(QCoreApplication::translate("ResearcherWizardOwnershipProofPage", "Prove ownership of a BOINC account by obtaining a cryptographic signature from a supporting project website.", nullptr));
        pubkeyHeaderLabel->setText(QCoreApplication::translate("ResearcherWizardOwnershipProofPage", "Your beacon public key:", nullptr));
#if QT_CONFIG(tooltip)
        copyPubKeyButton->setToolTip(QCoreApplication::translate("ResearcherWizardOwnershipProofPage", "Copy the beacon public key to the system clipboard", nullptr));
#endif // QT_CONFIG(tooltip)
        copyPubKeyButton->setText(QCoreApplication::translate("ResearcherWizardOwnershipProofPage", "&Copy", nullptr));
        projectListHeaderLabel->setText(QCoreApplication::translate("ResearcherWizardOwnershipProofPage", "Projects supporting account ownership proof:", nullptr));
        step1Label->setText(QCoreApplication::translate("ResearcherWizardOwnershipProofPage", "1. Sign in to one of the projects listed above and visit the account ownership proof page.", nullptr));
        step2Label->setText(QCoreApplication::translate("ResearcherWizardOwnershipProofPage", "2. Enter your beacon public key (shown above) where the project asks for it.", nullptr));
        step3Label->setText(QCoreApplication::translate("ResearcherWizardOwnershipProofPage", "3. Copy the entire XML block the project returns and paste it below:", nullptr));
        ownershipProofXmlEdit->setPlaceholderText(QCoreApplication::translate("ResearcherWizardOwnershipProofPage", "Paste the ownership proof XML here...", nullptr));
        sendBeaconButton->setText(QCoreApplication::translate("ResearcherWizardOwnershipProofPage", "&Send Beacon", nullptr));
        rememberLabel->setText(QCoreApplication::translate("ResearcherWizardOwnershipProofPage", "<html>\n"
"<head/>\n"
"<body>\n"
"<h4 style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">\n"
"<span style=\" font-size:medium; font-weight:600;\">Remember:</span>\n"
"</h4>\n"
"<ul style=\"margin-top: 0px; margin-bottom: 0px; margin-left: 0px; margin-right: 0px; -qt-list-indent: 0;\">\n"
"<li style=\" margin-top:6px; margin-bottom:0px; margin-left:15px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">The network only needs to verify the ownership proof from a single project even when you participate in multiple projects. </li>\n"
"<li style=\" margin-top:6px; margin-bottom:0px; margin-left:15px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">A beacon expires after six months pass. </li>\n"
"<li style=\" margin-top:6px; margin-bottom:0px; margin-left:15px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">A beacon becomes eligible for renewal after five months pass. The wallet will remind you to renew the beacon. </li"
                        ">\n"
"<li style=\" margin-top:6px; margin-bottom:12px; margin-left:15px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">This method does not require changing your BOINC username. </li>\n"
"</ul>\n"
"</body>\n"
"</html>", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ResearcherWizardOwnershipProofPage: public Ui_ResearcherWizardOwnershipProofPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_RESEARCHERWIZARDOWNERSHIPPROOFPAGE_H
