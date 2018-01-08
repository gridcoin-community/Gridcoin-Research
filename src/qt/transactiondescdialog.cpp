#include "transactiondescdialog.h"
#include "ui_transactiondescdialog.h"
#include "main.h"
#include "util.h"
#include "transactiontablemodel.h"
#include <QMessageBox>
#include <QModelIndex>

QString ToQString(std::string s);
std::string qtExecuteDotNetStringFunction(std::string function, std::string data);


TransactionDescDialog::TransactionDescDialog(const QModelIndex &idx, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TransactionDescDialog)
{
    ui->setupUi(this);
    QString desc = idx.data(TransactionTableModel::LongDescriptionRole).toString();
    ui->detailText->setHtml(desc);
    //If smart contract is populated
    ui->btnExecute->setVisible(Contains(msHashBoinc,"<CODE>"));
    ui->btnViewAttachment->setVisible(Contains(msHashBoinc,"<ATTACHMENT>"));
}

TransactionDescDialog::~TransactionDescDialog()
{
    delete ui;
}

void TransactionDescDialog::on_btnViewAttachment_clicked()
{
    //9-19-2015
    std::string sTXID = ExtractXML(msHashBoinc,"<ATTACHMENTGUID>","</ATTACHMENTGUID>");
    printf("View attachment %s",sTXID.c_str());

    if (sTXID.empty())
    {
        QString qsCaption = tr("Gridcoin Documents");
        QString qsBody = tr("Document cannot be found on P2P server.");
        QMessageBox::critical(this, qsCaption, qsBody, QMessageBox::Ok, QMessageBox::Ok);
    }
    else
    {
#if defined(WIN32) && defined(QT_GUI)
        std::string sData = qtExecuteDotNetStringFunction("ShowForm","frmAddAttachment," + sTXID);
#endif
    }
}
