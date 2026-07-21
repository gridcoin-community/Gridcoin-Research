/********************************************************************************
** Form generated from reading UI file 'favoritespage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_FAVORITESPAGE_H
#define UI_FAVORITESPAGE_H

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

class Ui_FavoritesPage
{
public:
    QVBoxLayout *favoritesVerticalLayout;
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

    void setupUi(QWidget *FavoritesPage)
    {
        if (FavoritesPage->objectName().isEmpty())
            FavoritesPage->setObjectName("FavoritesPage");
        FavoritesPage->resize(899, 456);
        favoritesVerticalLayout = new QVBoxLayout(FavoritesPage);
        favoritesVerticalLayout->setSpacing(0);
        favoritesVerticalLayout->setObjectName("favoritesVerticalLayout");
        favoritesVerticalLayout->setContentsMargins(0, 0, 0, 0);
        headerFrame = new QFrame(FavoritesPage);
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


        favoritesVerticalLayout->addWidget(headerFrame, 0, Qt::AlignVCenter);

        contentWrapperHorizontalLayout = new QHBoxLayout();
        contentWrapperHorizontalLayout->setSpacing(0);
        contentWrapperHorizontalLayout->setObjectName("contentWrapperHorizontalLayout");
        leftContentSpacer = new QSpacerItem(0, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        contentWrapperHorizontalLayout->addItem(leftContentSpacer);

        contentFrame = new QFrame(FavoritesPage);
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


        favoritesVerticalLayout->addLayout(contentWrapperHorizontalLayout);


        retranslateUi(FavoritesPage);

        QMetaObject::connectSlotsByName(FavoritesPage);
    } // setupUi

    void retranslateUi(QWidget *FavoritesPage)
    {
        FavoritesPage->setWindowTitle(QCoreApplication::translate("FavoritesPage", "Favorites", nullptr));
        headerTitleLabel->setText(QCoreApplication::translate("FavoritesPage", "Favorites", nullptr));
        filterLineEdit->setPlaceholderText(QCoreApplication::translate("FavoritesPage", "Search by address or label", nullptr));
    } // retranslateUi

};

namespace Ui {
    class FavoritesPage: public Ui_FavoritesPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_FAVORITESPAGE_H
