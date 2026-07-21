/********************************************************************************
** Form generated from reading UI file 'aboutdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ABOUTDIALOG_H
#define UI_ABOUTDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_AboutDialog
{
public:
    QHBoxLayout *horizontalLayout_2;
    QLabel *aboutLogoLabel;
    QVBoxLayout *verticalLayout_2;
    QSpacerItem *verticalSpacer_2;
    QHBoxLayout *horizontalLayout;
    QLabel *label;
    QLabel *versionLabel;
    QSpacerItem *horizontalSpacer;
    QLabel *copyrightLabel;
    QLabel *licenseLabel;
    QSpacerItem *verticalSpacer;
    QHBoxLayout *horizontalLayout_3;
    QSpacerItem *horizontalSpacer_3;
    QPushButton *versionInfoButton;
    QSpacerItem *horizontalSpacer_2;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *AboutDialog)
    {
        if (AboutDialog->objectName().isEmpty())
            AboutDialog->setObjectName("AboutDialog");
        AboutDialog->resize(839, 360);
        AboutDialog->setAutoFillBackground(false);
        horizontalLayout_2 = new QHBoxLayout(AboutDialog);
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        aboutLogoLabel = new QLabel(AboutDialog);
        aboutLogoLabel->setObjectName("aboutLogoLabel");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Ignored);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(aboutLogoLabel->sizePolicy().hasHeightForWidth());
        aboutLogoLabel->setSizePolicy(sizePolicy);

        horizontalLayout_2->addWidget(aboutLogoLabel);

        verticalLayout_2 = new QVBoxLayout();
        verticalLayout_2->setObjectName("verticalLayout_2");
        verticalSpacer_2 = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_2->addItem(verticalSpacer_2);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        label = new QLabel(AboutDialog);
        label->setObjectName("label");
        label->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        label->setTextInteractionFlags(Qt::LinksAccessibleByMouse);

        horizontalLayout->addWidget(label);

        versionLabel = new QLabel(AboutDialog);
        versionLabel->setObjectName("versionLabel");
        versionLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        versionLabel->setText(QString::fromUtf8(""));
        versionLabel->setTextInteractionFlags(Qt::NoTextInteraction);

        horizontalLayout->addWidget(versionLabel);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);


        verticalLayout_2->addLayout(horizontalLayout);

        copyrightLabel = new QLabel(AboutDialog);
        copyrightLabel->setObjectName("copyrightLabel");
        copyrightLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        copyrightLabel->setText(QString::fromUtf8("Copyright &copy; 2013-YYYY The original software credited to all of the Bitcoin developers\n"
"Copyright &copy; 2013-YYYY The Gridcoin developers"));
        copyrightLabel->setTextFormat(Qt::RichText);
        copyrightLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);

        verticalLayout_2->addWidget(copyrightLabel);

        licenseLabel = new QLabel(AboutDialog);
        licenseLabel->setObjectName("licenseLabel");
        licenseLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        licenseLabel->setWordWrap(true);
        licenseLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);

        verticalLayout_2->addWidget(licenseLabel);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_2->addItem(verticalSpacer);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName("horizontalLayout_3");
        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_3->addItem(horizontalSpacer_3);

        versionInfoButton = new QPushButton(AboutDialog);
        versionInfoButton->setObjectName("versionInfoButton");

        horizontalLayout_3->addWidget(versionInfoButton);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_3->addItem(horizontalSpacer_2);


        verticalLayout_2->addLayout(horizontalLayout_3);

        buttonBox = new QDialogButtonBox(AboutDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Ok);

        verticalLayout_2->addWidget(buttonBox);


        horizontalLayout_2->addLayout(verticalLayout_2);


        retranslateUi(AboutDialog);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, AboutDialog, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, AboutDialog, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(AboutDialog);
    } // setupUi

    void retranslateUi(QDialog *AboutDialog)
    {
        AboutDialog->setWindowTitle(QCoreApplication::translate("AboutDialog", "About Gridcoin", nullptr));
        label->setText(QCoreApplication::translate("AboutDialog", "<b>Gridcoin</b> ", nullptr));
        licenseLabel->setText(QCoreApplication::translate("AboutDialog", "\n"
"This is experimental software.\n"
"\n"
"Distributed under the MIT/X11 software license, see the accompanying file COPYING or https://opensource.org/licenses/mit-license.php.\n"
"\n"
"This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit (https://www.openssl.org/) and cryptographic software written by Eric Young (eay@cryptsoft.com) and UPnP software written by Thomas Bernard.", nullptr));
        versionInfoButton->setText(QCoreApplication::translate("AboutDialog", "Version Information", nullptr));
    } // retranslateUi

};

namespace Ui {
    class AboutDialog: public Ui_AboutDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ABOUTDIALOG_H
