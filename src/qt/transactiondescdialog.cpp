#include "transactiondescdialog.h"
#include "ui_transactiondescdialog.h"
#include "main.h"
#include "transactiontablemodel.h"
#include <QMessageBox>
#include <QModelIndex>

void ExecuteCode();
extern std::string ExtractXML(std::string XMLdata, std::string key, std::string key_end);
QString ToQString(std::string s);
bool Contains(std::string data, std::string instring);
int qtTrackConfirm(std::string txid);
std::string qtExecuteDotNetStringFunction(std::string function, std::string data);


TransactionDescDialog::TransactionDescDialog(const QModelIndex &idx, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TransactionDescDialog)
{
    ui->setupUi(this);
    QString desc = idx.data(TransactionTableModel::LongDescriptionRole).toString();
    ui->detailText->setHtml(desc);
	//If smart contract is populated
	if (Contains(msHashBoinc,"<CODE>"))
	{
		ui->btnExecute->setVisible(true);
	}
	else
	{
			ui->btnExecute->setVisible(false);
	}

	if (Contains(msHashBoinc,"<TRACK>"))
	{
		ui->btnTrack->setVisible(true);
	}
	else
	{
		ui->btnTrack->setVisible(false);
	}

	if (Contains(msHashBoinc,"<ATTACHMENT>"))
	{
		ui->btnViewAttachment->setVisible(true);
	}
	else
	{
		ui->btnViewAttachment->setVisible(false);
	}

							
}

TransactionDescDialog::~TransactionDescDialog()
{
    delete ui;
}


void TransactionDescDialog::askQuestion(std::string caption, std::string body, bool *result)
{
		QString qsCaption = tr(caption.c_str());
		QString qsBody = tr(body.c_str());
		QMessageBox::StandardButton retval = QMessageBox::question(this, qsCaption, qsBody, QMessageBox::Yes|QMessageBox::Cancel,   QMessageBox::Cancel);
		*result = (retval == QMessageBox::Yes);
}



void TransactionDescDialog::on_btnTrack_clicked()
{
    //Tracking Coins
	std::string sTXID = ExtractXML(msHashBoinc,"<TRACK>","</TRACK>");
	sTXID = msHashBoincTxId;

	if (sTXID.length() > 1)
	{
		int result = 0;
		result = qtTrackConfirm(sTXID);
		std::string body = "";
		bool received = false;
		if (result==1)
		{
			body = "Coins were received by the recipient.";
		}
		else
		{
			body = "Coins were not received by the recipient.";
		}
		QString qsCaption = tr("Gridcoin Coin Tracking System");
		QString qsBody = tr(body.c_str());
	    QMessageBox::critical(this, qsCaption, qsBody, QMessageBox::Ok, QMessageBox::Ok);
  
	}

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
