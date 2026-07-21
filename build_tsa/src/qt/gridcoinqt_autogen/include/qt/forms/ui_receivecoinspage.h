/********************************************************************************
** Form generated from reading UI file 'receivecoinspage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_RECEIVECOINSPAGE_H
#define UI_RECEIVECOINSPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ReceiveCoinsPage
{
public:
    QVBoxLayout *receiveCoinsVerticalLayout;
    QFrame *headerFrame;
    QHBoxLayout *headerFrameLayout;
    QWidget *headerTitleWrapper;
    QVBoxLayout *headerTitleVerticalLayout;
    QLabel *headerTitleLabel;
    QSpacerItem *headerFrameSpacer;
    QLineEdit *filterLineEdit;
    QHBoxLayout *contentWrapperHorizontalLayout;
    QSpacerItem *leftContentSpacer;
    QFrame *contentFrame;
    QVBoxLayout *contentFrameVerticalLayout;
    QSpacerItem *rightContentSpacer;

    void setupUi(QWidget *ReceiveCoinsPage)
    {
        if (ReceiveCoinsPage->objectName().isEmpty())
            ReceiveCoinsPage->setObjectName("ReceiveCoinsPage");
        ReceiveCoinsPage->resize(899, 456);
        receiveCoinsVerticalLayout = new QVBoxLayout(ReceiveCoinsPage);
        receiveCoinsVerticalLayout->setSpacing(0);
        receiveCoinsVerticalLayout->setObjectName("receiveCoinsVerticalLayout");
        receiveCoinsVerticalLayout->setContentsMargins(0, 0, 0, 0);
        headerFrame = new QFrame(ReceiveCoinsPage);
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

        filterLineEdit = new QLineEdit(headerFrame);
        filterLineEdit->setObjectName("filterLineEdit");
        filterLineEdit->setClearButtonEnabled(true);

        headerFrameLayout->addWidget(filterLineEdit);


        receiveCoinsVerticalLayout->addWidget(headerFrame, 0, Qt::AlignVCenter);

        contentWrapperHorizontalLayout = new QHBoxLayout();
        contentWrapperHorizontalLayout->setSpacing(0);
        contentWrapperHorizontalLayout->setObjectName("contentWrapperHorizontalLayout");
        leftContentSpacer = new QSpacerItem(0, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        contentWrapperHorizontalLayout->addItem(leftContentSpacer);

        contentFrame = new QFrame(ReceiveCoinsPage);
        contentFrame->setObjectName("contentFrame");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(1);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(contentFrame->sizePolicy().hasHeightForWidth());
        contentFrame->setSizePolicy(sizePolicy);
        contentFrame->setFrameShape(QFrame::StyledPanel);
        contentFrame->setFrameShadow(QFrame::Raised);
        contentFrameVerticalLayout = new QVBoxLayout(contentFrame);
        contentFrameVerticalLayout->setSpacing(9);
        contentFrameVerticalLayout->setObjectName("contentFrameVerticalLayout");
        contentFrameVerticalLayout->setContentsMargins(9, 9, 9, 9);

        contentWrapperHorizontalLayout->addWidget(contentFrame);

        rightContentSpacer = new QSpacerItem(0, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        contentWrapperHorizontalLayout->addItem(rightContentSpacer);


        receiveCoinsVerticalLayout->addLayout(contentWrapperHorizontalLayout);


        retranslateUi(ReceiveCoinsPage);

        QMetaObject::connectSlotsByName(ReceiveCoinsPage);
    } // setupUi

    void retranslateUi(QWidget *ReceiveCoinsPage)
    {
        ReceiveCoinsPage->setWindowTitle(QCoreApplication::translate("ReceiveCoinsPage", "Receive Payment", nullptr));
        headerTitleLabel->setText(QCoreApplication::translate("ReceiveCoinsPage", "Receive Payment", nullptr));
        filterLineEdit->setPlaceholderText(QCoreApplication::translate("ReceiveCoinsPage", "Search by address or label", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ReceiveCoinsPage: public Ui_ReceiveCoinsPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_RECEIVECOINSPAGE_H
