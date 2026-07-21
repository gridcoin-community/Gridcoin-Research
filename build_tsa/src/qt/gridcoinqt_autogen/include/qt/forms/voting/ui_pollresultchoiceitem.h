/********************************************************************************
** Form generated from reading UI file 'pollresultchoiceitem.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_POLLRESULTCHOICEITEM_H
#define UI_POLLRESULTCHOICEITEM_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_PollResultChoiceItem
{
public:
    QVBoxLayout *resultChoiceLayout;
    QLabel *choiceLabel;
    QProgressBar *weightBar;
    QHBoxLayout *horizontalLayout;
    QLabel *weightTextLabel;
    QLabel *weightLabel;
    QSpacerItem *horizontalSpacer;
    QLabel *percentageLabel;

    void setupUi(QWidget *PollResultChoiceItem)
    {
        if (PollResultChoiceItem->objectName().isEmpty())
            PollResultChoiceItem->setObjectName("PollResultChoiceItem");
        PollResultChoiceItem->resize(666, 90);
        resultChoiceLayout = new QVBoxLayout(PollResultChoiceItem);
        resultChoiceLayout->setObjectName("resultChoiceLayout");
        resultChoiceLayout->setContentsMargins(0, 0, 0, 0);
        choiceLabel = new QLabel(PollResultChoiceItem);
        choiceLabel->setObjectName("choiceLabel");
        choiceLabel->setText(QString::fromUtf8("Choice Text"));
        choiceLabel->setTextFormat(Qt::PlainText);
        choiceLabel->setWordWrap(true);
        choiceLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        resultChoiceLayout->addWidget(choiceLabel);

        weightBar = new QProgressBar(PollResultChoiceItem);
        weightBar->setObjectName("weightBar");
        weightBar->setMaximum(1000);
        weightBar->setTextVisible(false);
        weightBar->setInvertedAppearance(false);

        resultChoiceLayout->addWidget(weightBar);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        weightTextLabel = new QLabel(PollResultChoiceItem);
        weightTextLabel->setObjectName("weightTextLabel");

        horizontalLayout->addWidget(weightTextLabel);

        weightLabel = new QLabel(PollResultChoiceItem);
        weightLabel->setObjectName("weightLabel");
        weightLabel->setText(QString::fromUtf8("123"));
        weightLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        horizontalLayout->addWidget(weightLabel);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        percentageLabel = new QLabel(PollResultChoiceItem);
        percentageLabel->setObjectName("percentageLabel");
        percentageLabel->setText(QString::fromUtf8("1%"));
        percentageLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        horizontalLayout->addWidget(percentageLabel);


        resultChoiceLayout->addLayout(horizontalLayout);


        retranslateUi(PollResultChoiceItem);

        QMetaObject::connectSlotsByName(PollResultChoiceItem);
    } // setupUi

    void retranslateUi(QWidget *PollResultChoiceItem)
    {
        PollResultChoiceItem->setWindowTitle(QCoreApplication::translate("PollResultChoiceItem", "Form", nullptr));
        weightTextLabel->setText(QCoreApplication::translate("PollResultChoiceItem", "Weight:", nullptr));
    } // retranslateUi

};

namespace Ui {
    class PollResultChoiceItem: public Ui_PollResultChoiceItem {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_POLLRESULTCHOICEITEM_H
