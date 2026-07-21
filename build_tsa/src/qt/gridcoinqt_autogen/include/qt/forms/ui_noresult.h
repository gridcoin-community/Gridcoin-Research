/********************************************************************************
** Form generated from reading UI file 'noresult.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_NORESULT_H
#define UI_NORESULT_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_NoResult
{
public:
    QVBoxLayout *verticalLayout;
    QSpacerItem *topSpacer;
    QLabel *iconLabel;
    QLabel *titleLabel;
    QSpacerItem *bottomSpacer;

    void setupUi(QWidget *NoResult)
    {
        if (NoResult->objectName().isEmpty())
            NoResult->setObjectName("NoResult");
        NoResult->resize(400, 300);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(NoResult->sizePolicy().hasHeightForWidth());
        NoResult->setSizePolicy(sizePolicy);
        verticalLayout = new QVBoxLayout(NoResult);
        verticalLayout->setSpacing(16);
        verticalLayout->setObjectName("verticalLayout");
        topSpacer = new QSpacerItem(20, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout->addItem(topSpacer);

        iconLabel = new QLabel(NoResult);
        iconLabel->setObjectName("iconLabel");

        verticalLayout->addWidget(iconLabel, 0, Qt::AlignHCenter);

        titleLabel = new QLabel(NoResult);
        titleLabel->setObjectName("titleLabel");

        verticalLayout->addWidget(titleLabel, 0, Qt::AlignHCenter);

        bottomSpacer = new QSpacerItem(20, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout->addItem(bottomSpacer);

        verticalLayout->setStretch(0, 50);
        verticalLayout->setStretch(3, 100);

        retranslateUi(NoResult);

        QMetaObject::connectSlotsByName(NoResult);
    } // setupUi

    void retranslateUi(QWidget *NoResult)
    {
        NoResult->setWindowTitle(QCoreApplication::translate("NoResult", "Form", nullptr));
        titleLabel->setText(QCoreApplication::translate("NoResult", "Nothing here yet...", nullptr));
    } // retranslateUi

};

namespace Ui {
    class NoResult: public Ui_NoResult {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_NORESULT_H
