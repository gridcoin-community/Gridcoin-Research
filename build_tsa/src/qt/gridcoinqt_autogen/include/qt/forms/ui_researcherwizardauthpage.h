/********************************************************************************
** Form generated from reading UI file 'researcherwizardauthpage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_RESEARCHERWIZARDAUTHPAGE_H
#define UI_RESEARCHERWIZARDAUTHPAGE_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <QtWidgets/QWizardPage>

QT_BEGIN_NAMESPACE

class Ui_ResearcherWizardAuthPage
{
public:
    QVBoxLayout *authPageLayout;
    QScrollArea *rememberScrollArea;
    QWidget *rememberScrollAreaContents;
    QVBoxLayout *verticalLayout;
    QLabel *step1Label;
    QLabel *step2Label;
    QLabel *step3Label;
    QHBoxLayout *verificationCodeLayout;
    QLabel *verificationCodeLabel;
    QPushButton *copyToClipboardButton;
    QSpacerItem *verificationCodeSpacer;
    QLabel *step4Label;
    QLabel *step5Label;
    QLabel *step6Label;
    QSpacerItem *rememberSpacer;
    QLabel *rememberLabel;

    void setupUi(QWizardPage *ResearcherWizardAuthPage)
    {
        if (ResearcherWizardAuthPage->objectName().isEmpty())
            ResearcherWizardAuthPage->setObjectName("ResearcherWizardAuthPage");
        ResearcherWizardAuthPage->resize(630, 480);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(ResearcherWizardAuthPage->sizePolicy().hasHeightForWidth());
        ResearcherWizardAuthPage->setSizePolicy(sizePolicy);
        authPageLayout = new QVBoxLayout(ResearcherWizardAuthPage);
        authPageLayout->setObjectName("authPageLayout");
        rememberScrollArea = new QScrollArea(ResearcherWizardAuthPage);
        rememberScrollArea->setObjectName("rememberScrollArea");
        rememberScrollArea->setWidgetResizable(true);
        rememberScrollArea->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        rememberScrollAreaContents = new QWidget();
        rememberScrollAreaContents->setObjectName("rememberScrollAreaContents");
        rememberScrollAreaContents->setGeometry(QRect(0, 0, 596, 498));
        rememberScrollAreaContents->setStyleSheet(QString::fromUtf8(""));
        verticalLayout = new QVBoxLayout(rememberScrollAreaContents);
        verticalLayout->setObjectName("verticalLayout");
        step1Label = new QLabel(rememberScrollAreaContents);
        step1Label->setObjectName("step1Label");
        step1Label->setWordWrap(true);
        step1Label->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        verticalLayout->addWidget(step1Label);

        step2Label = new QLabel(rememberScrollAreaContents);
        step2Label->setObjectName("step2Label");
        step2Label->setWordWrap(true);
        step2Label->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        verticalLayout->addWidget(step2Label);

        step3Label = new QLabel(rememberScrollAreaContents);
        step3Label->setObjectName("step3Label");
        step3Label->setWordWrap(true);
        step3Label->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        verticalLayout->addWidget(step3Label);

        verificationCodeLayout = new QHBoxLayout();
        verificationCodeLayout->setObjectName("verificationCodeLayout");
        verificationCodeLayout->setContentsMargins(16, 9, 9, 9);
        verificationCodeLabel = new QLabel(rememberScrollAreaContents);
        verificationCodeLabel->setObjectName("verificationCodeLabel");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Preferred);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(verificationCodeLabel->sizePolicy().hasHeightForWidth());
        verificationCodeLabel->setSizePolicy(sizePolicy1);
        QFont font;
        font.setFamilies({QString::fromUtf8("Monospace")});
        verificationCodeLabel->setFont(font);
        verificationCodeLabel->setText(QString::fromUtf8(""));
        verificationCodeLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        verificationCodeLayout->addWidget(verificationCodeLabel, 0, Qt::AlignLeft|Qt::AlignVCenter);

        copyToClipboardButton = new QPushButton(rememberScrollAreaContents);
        copyToClipboardButton->setObjectName("copyToClipboardButton");
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(copyToClipboardButton->sizePolicy().hasHeightForWidth());
        copyToClipboardButton->setSizePolicy(sizePolicy2);
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icons/editcopy"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        copyToClipboardButton->setIcon(icon);

        verificationCodeLayout->addWidget(copyToClipboardButton, 0, Qt::AlignLeft|Qt::AlignVCenter);

        verificationCodeSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        verificationCodeLayout->addItem(verificationCodeSpacer);


        verticalLayout->addLayout(verificationCodeLayout);

        step4Label = new QLabel(rememberScrollAreaContents);
        step4Label->setObjectName("step4Label");
        step4Label->setWordWrap(true);
        step4Label->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        verticalLayout->addWidget(step4Label);

        step5Label = new QLabel(rememberScrollAreaContents);
        step5Label->setObjectName("step5Label");
        step5Label->setWordWrap(true);
        step5Label->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        verticalLayout->addWidget(step5Label);

        step6Label = new QLabel(rememberScrollAreaContents);
        step6Label->setObjectName("step6Label");
        step6Label->setWordWrap(true);
        step6Label->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        verticalLayout->addWidget(step6Label);

        rememberSpacer = new QSpacerItem(20, 10, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        verticalLayout->addItem(rememberSpacer);

        rememberLabel = new QLabel(rememberScrollAreaContents);
        rememberLabel->setObjectName("rememberLabel");
        rememberLabel->setStyleSheet(QString::fromUtf8(""));
        rememberLabel->setTextFormat(Qt::RichText);
        rememberLabel->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        rememberLabel->setWordWrap(true);
        rememberLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        verticalLayout->addWidget(rememberLabel);

        rememberScrollArea->setWidget(rememberScrollAreaContents);

        authPageLayout->addWidget(rememberScrollArea);


        retranslateUi(ResearcherWizardAuthPage);

        QMetaObject::connectSlotsByName(ResearcherWizardAuthPage);
    } // setupUi

    void retranslateUi(QWizardPage *ResearcherWizardAuthPage)
    {
        ResearcherWizardAuthPage->setWindowTitle(QCoreApplication::translate("ResearcherWizardAuthPage", "Beacon Verification", nullptr));
        ResearcherWizardAuthPage->setTitle(QCoreApplication::translate("ResearcherWizardAuthPage", "Beacon Verification", nullptr));
        ResearcherWizardAuthPage->setSubTitle(QCoreApplication::translate("ResearcherWizardAuthPage", "Gridcoin needs to verify your BOINC account CPID. Please follow the instructions below to change your BOINC account username. The network needs 24 to 48 hours to verify a new CPID.", nullptr));
        step1Label->setText(QCoreApplication::translate("ResearcherWizardAuthPage", "1. Sign in to your account at the website for a whitelisted BOINC project.", nullptr));
        step2Label->setText(QCoreApplication::translate("ResearcherWizardAuthPage", "2. Visit the settings page to change your username. Many projects label it as \"other account info\".", nullptr));
        step3Label->setText(QCoreApplication::translate("ResearcherWizardAuthPage", "3. Change your \"name\" (real name or nickname) to the following verification code:", nullptr));
#if QT_CONFIG(tooltip)
        copyToClipboardButton->setToolTip(QCoreApplication::translate("ResearcherWizardAuthPage", "Copy the verification code to the system clipboard", nullptr));
#endif // QT_CONFIG(tooltip)
        copyToClipboardButton->setText(QCoreApplication::translate("ResearcherWizardAuthPage", "&Copy", nullptr));
        step4Label->setText(QCoreApplication::translate("ResearcherWizardAuthPage", "4. Some projects will not export your statistics by default. If available, enable the privacy setting that gives consent to the project to export your statistics data. Many projects place this setting on the \"Preferences for this Project\" page and label it as \"Do you consent to exporting your data to BOINC statistics aggregation web sites?\"", nullptr));
        step5Label->setText(QCoreApplication::translate("ResearcherWizardAuthPage", "5. Wait 24 to 48 hours for the verification process to finish (beacon status will change to \"active\").", nullptr));
        step6Label->setText(QCoreApplication::translate("ResearcherWizardAuthPage", "6. After that, you may change the username back to your preference.", nullptr));
        rememberLabel->setText(QCoreApplication::translate("ResearcherWizardAuthPage", "<html>\n"
"<head/>\n"
"<body>\n"
"<h4 style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">\n"
"<span style=\" font-size:medium; font-weight:600;\">Remember:</span>\n"
"</h4>\n"
"<ul style=\"margin-top: 0px; margin-bottom: 0px; margin-left: 0px; margin-right: 0px; -qt-list-indent: 0;\">\n"
"<li style=\" margin-top:6px; margin-bottom:0px; margin-left:15px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">The network only needs to verify the code above at a single whitelisted BOINC project even when you participate in multiple projects. </li>\n"
"<li style=\" margin-top:6px; margin-bottom:0px; margin-left:15px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">The verification code expires after three days pass. </li>\n"
"<li style=\" margin-top:6px; margin-bottom:0px; margin-left:15px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">A beacon expires after six months pass. </li><li style=\" margin-top:6px; margin-bottom:0p"
                        "x; margin-left:15px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">A beacon becomes eligible for renewal after five months pass. The wallet will remind you to renew the beacon. </li>\n"
"<li style=\" margin-top:6px; margin-bottom:12px; margin-left:15px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">You will not need to change your username again to renew a beacon unless it expires. </li>\n"
"</ul>\n"
"</body>\n"
"</html>", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ResearcherWizardAuthPage: public Ui_ResearcherWizardAuthPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_RESEARCHERWIZARDAUTHPAGE_H
