/********************************************************************************
** Form generated from reading UI file 'pollcardview.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_POLLCARDVIEW_H
#define UI_POLLCARDVIEW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_PollCardView
{
public:
    QVBoxLayout *pollCardLayout;
    QScrollArea *scrollArea;
    QWidget *scrollAreaWidgetContents;
    QVBoxLayout *scrollAreaVerticalLayout;
    QWidget *cardsWrapperWidget;
    QHBoxLayout *cardsWrapperLayout;
    QSpacerItem *leftSpacer;
    QFrame *contentFrame;
    QVBoxLayout *contentFrameLayout;
    QVBoxLayout *cardsLayout;
    QSpacerItem *rightSpacer;
    QSpacerItem *cardsBottomSpacer;

    void setupUi(QWidget *PollCardView)
    {
        if (PollCardView->objectName().isEmpty())
            PollCardView->setObjectName("PollCardView");
        PollCardView->resize(697, 228);
        pollCardLayout = new QVBoxLayout(PollCardView);
        pollCardLayout->setSpacing(0);
        pollCardLayout->setObjectName("pollCardLayout");
        pollCardLayout->setContentsMargins(0, 0, 0, 0);
        scrollArea = new QScrollArea(PollCardView);
        scrollArea->setObjectName("scrollArea");
        scrollArea->setWidgetResizable(true);
        scrollAreaWidgetContents = new QWidget();
        scrollAreaWidgetContents->setObjectName("scrollAreaWidgetContents");
        scrollAreaWidgetContents->setGeometry(QRect(0, 0, 695, 226));
        scrollAreaVerticalLayout = new QVBoxLayout(scrollAreaWidgetContents);
        scrollAreaVerticalLayout->setSpacing(0);
        scrollAreaVerticalLayout->setObjectName("scrollAreaVerticalLayout");
        scrollAreaVerticalLayout->setContentsMargins(0, 0, 0, 0);
        cardsWrapperWidget = new QWidget(scrollAreaWidgetContents);
        cardsWrapperWidget->setObjectName("cardsWrapperWidget");
        cardsWrapperLayout = new QHBoxLayout(cardsWrapperWidget);
        cardsWrapperLayout->setSpacing(0);
        cardsWrapperLayout->setObjectName("cardsWrapperLayout");
        cardsWrapperLayout->setContentsMargins(12, 12, 12, 12);
        leftSpacer = new QSpacerItem(0, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        cardsWrapperLayout->addItem(leftSpacer);

        contentFrame = new QFrame(cardsWrapperWidget);
        contentFrame->setObjectName("contentFrame");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);
        sizePolicy.setHorizontalStretch(1);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(contentFrame->sizePolicy().hasHeightForWidth());
        contentFrame->setSizePolicy(sizePolicy);
        contentFrame->setFrameShape(QFrame::StyledPanel);
        contentFrame->setFrameShadow(QFrame::Raised);
        contentFrameLayout = new QVBoxLayout(contentFrame);
        contentFrameLayout->setObjectName("contentFrameLayout");
        contentFrameLayout->setContentsMargins(0, 0, 0, 0);
        cardsLayout = new QVBoxLayout();
        cardsLayout->setSpacing(12);
        cardsLayout->setObjectName("cardsLayout");

        contentFrameLayout->addLayout(cardsLayout);


        cardsWrapperLayout->addWidget(contentFrame);

        rightSpacer = new QSpacerItem(0, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        cardsWrapperLayout->addItem(rightSpacer);


        scrollAreaVerticalLayout->addWidget(cardsWrapperWidget);

        cardsBottomSpacer = new QSpacerItem(20, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        scrollAreaVerticalLayout->addItem(cardsBottomSpacer);

        scrollAreaVerticalLayout->setStretch(1, 1000);
        scrollArea->setWidget(scrollAreaWidgetContents);

        pollCardLayout->addWidget(scrollArea);


        retranslateUi(PollCardView);

        QMetaObject::connectSlotsByName(PollCardView);
    } // setupUi

    void retranslateUi(QWidget *PollCardView)
    {
        PollCardView->setWindowTitle(QCoreApplication::translate("PollCardView", "Form", nullptr));
    } // retranslateUi

};

namespace Ui {
    class PollCardView: public Ui_PollCardView {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_POLLCARDVIEW_H
