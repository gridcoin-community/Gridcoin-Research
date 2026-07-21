/********************************************************************************
** Form generated from reading UI file 'polltab.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_POLLTAB_H
#define UI_POLLTAB_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTableView>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "voting/pollcardview.h"

QT_BEGIN_NAMESPACE

class Ui_PollTab
{
public:
    QVBoxLayout *tabLayout;
    QStackedWidget *stack;
    QWidget *cardPage;
    QVBoxLayout *cardPageLayout;
    PollCardView *cards;
    QWidget *tablePage;
    QVBoxLayout *tablePageLayout;
    QFrame *tableFrame;
    QVBoxLayout *activeTableFrameLayout;
    QTableView *table;

    void setupUi(QWidget *PollTab)
    {
        if (PollTab->objectName().isEmpty())
            PollTab->setObjectName("PollTab");
        PollTab->resize(666, 424);
        tabLayout = new QVBoxLayout(PollTab);
        tabLayout->setSpacing(0);
        tabLayout->setObjectName("tabLayout");
        tabLayout->setContentsMargins(0, 0, 0, 0);
        stack = new QStackedWidget(PollTab);
        stack->setObjectName("stack");
        cardPage = new QWidget();
        cardPage->setObjectName("cardPage");
        cardPageLayout = new QVBoxLayout(cardPage);
        cardPageLayout->setObjectName("cardPageLayout");
        cardPageLayout->setContentsMargins(0, 0, 0, 0);
        cards = new PollCardView(cardPage);
        cards->setObjectName("cards");

        cardPageLayout->addWidget(cards);

        stack->addWidget(cardPage);
        tablePage = new QWidget();
        tablePage->setObjectName("tablePage");
        tablePageLayout = new QVBoxLayout(tablePage);
        tablePageLayout->setObjectName("tablePageLayout");
        tablePageLayout->setContentsMargins(12, 12, 12, 12);
        tableFrame = new QFrame(tablePage);
        tableFrame->setObjectName("tableFrame");
        activeTableFrameLayout = new QVBoxLayout(tableFrame);
        activeTableFrameLayout->setObjectName("activeTableFrameLayout");
        activeTableFrameLayout->setContentsMargins(0, 0, 0, 0);
        table = new QTableView(tableFrame);
        table->setObjectName("table");
        table->setContextMenuPolicy(Qt::CustomContextMenu);
        table->setAlternatingRowColors(true);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        table->setShowGrid(false);
        table->setSortingEnabled(true);
        table->horizontalHeader()->setHighlightSections(false);
        table->horizontalHeader()->setStretchLastSection(true);
        table->verticalHeader()->setVisible(false);

        activeTableFrameLayout->addWidget(table);


        tablePageLayout->addWidget(tableFrame);

        stack->addWidget(tablePage);

        tabLayout->addWidget(stack);


        retranslateUi(PollTab);

        QMetaObject::connectSlotsByName(PollTab);
    } // setupUi

    void retranslateUi(QWidget *PollTab)
    {
        PollTab->setWindowTitle(QCoreApplication::translate("PollTab", "Form", nullptr));
    } // retranslateUi

};

namespace Ui {
    class PollTab: public Ui_PollTab {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_POLLTAB_H
