/********************************************************************************
** Form generated from reading UI file 'pollresultdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_POLLRESULTDIALOG_H
#define UI_POLLRESULTDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "voting/polldetails.h"

QT_BEGIN_NAMESPACE

class Ui_PollResultDialog
{
public:
    QVBoxLayout *pollResultDialogLayout;
    QFrame *frame;
    QVBoxLayout *verticalLayout;
    PollDetails *details;
    QFrame *detailsLine;
    QScrollArea *choicesScrollArea;
    QWidget *choices;
    QVBoxLayout *choicesScrollLayout;
    QVBoxLayout *choicesLayout;
    QSpacerItem *choicesBottomSpacer;
    QSpacerItem *frameBottomSpacer;
    QHBoxLayout *bottomLayout;
    QLabel *idLabel;
    QDialogButtonBox *buttonBox;

    void setupUi(QWidget *PollResultDialog)
    {
        if (PollResultDialog->objectName().isEmpty())
            PollResultDialog->setObjectName("PollResultDialog");
        PollResultDialog->resize(740, 580);
        pollResultDialogLayout = new QVBoxLayout(PollResultDialog);
        pollResultDialogLayout->setObjectName("pollResultDialogLayout");
        frame = new QFrame(PollResultDialog);
        frame->setObjectName("frame");
        frame->setFrameShape(QFrame::StyledPanel);
        frame->setFrameShadow(QFrame::Raised);
        verticalLayout = new QVBoxLayout(frame);
        verticalLayout->setSpacing(9);
        verticalLayout->setObjectName("verticalLayout");
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        details = new PollDetails(frame);
        details->setObjectName("details");

        verticalLayout->addWidget(details);

        detailsLine = new QFrame(frame);
        detailsLine->setObjectName("detailsLine");
        detailsLine->setFrameShape(QFrame::Shape::HLine);
        detailsLine->setFrameShadow(QFrame::Shadow::Sunken);

        verticalLayout->addWidget(detailsLine);

        choicesScrollArea = new QScrollArea(frame);
        choicesScrollArea->setObjectName("choicesScrollArea");
        choicesScrollArea->setWidgetResizable(true);
        choices = new QWidget();
        choices->setObjectName("choices");
        choices->setGeometry(QRect(0, 0, 718, 486));
        choicesScrollLayout = new QVBoxLayout(choices);
        choicesScrollLayout->setObjectName("choicesScrollLayout");
        choicesScrollLayout->setContentsMargins(0, 0, 0, 0);
        choicesLayout = new QVBoxLayout();
        choicesLayout->setSpacing(12);
        choicesLayout->setObjectName("choicesLayout");
        choicesLayout->setContentsMargins(-1, -1, 9, -1);

        choicesScrollLayout->addLayout(choicesLayout);

        choicesBottomSpacer = new QSpacerItem(20, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        choicesScrollLayout->addItem(choicesBottomSpacer);

        choicesScrollArea->setWidget(choices);

        verticalLayout->addWidget(choicesScrollArea);

        frameBottomSpacer = new QSpacerItem(20, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout->addItem(frameBottomSpacer);

        verticalLayout->setStretch(2, 1);

        pollResultDialogLayout->addWidget(frame);

        bottomLayout = new QHBoxLayout();
        bottomLayout->setObjectName("bottomLayout");
        idLabel = new QLabel(PollResultDialog);
        idLabel->setObjectName("idLabel");
        idLabel->setText(QString::fromUtf8("ID"));
        idLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        bottomLayout->addWidget(idLabel);

        buttonBox = new QDialogButtonBox(PollResultDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setStandardButtons(QDialogButtonBox::Close);

        bottomLayout->addWidget(buttonBox);


        pollResultDialogLayout->addLayout(bottomLayout);


        retranslateUi(PollResultDialog);

        QMetaObject::connectSlotsByName(PollResultDialog);
    } // setupUi

    void retranslateUi(QWidget *PollResultDialog)
    {
        PollResultDialog->setWindowTitle(QCoreApplication::translate("PollResultDialog", "Poll Details", nullptr));
#if QT_CONFIG(tooltip)
        idLabel->setToolTip(QCoreApplication::translate("PollResultDialog", "Poll ID", nullptr));
#endif // QT_CONFIG(tooltip)
    } // retranslateUi

};

namespace Ui {
    class PollResultDialog: public Ui_PollResultDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_POLLRESULTDIALOG_H
