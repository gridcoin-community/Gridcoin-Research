#include "transactiondescdialog.h"
#include "ui_transactiondescdialog.h"
#include "main.h"

#include "transactiontablemodel.h"
#include <QMessageBox>

#include <QModelIndex>
std::string RoundToString(double d, int place);
void ExecuteCode();
extern std::string ExtractXML(std::string XMLdata, std::string key, std::string key_end);
QString ToQString(std::string s);


TransactionDescDialog::TransactionDescDialog(const QModelIndex &idx, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TransactionDescDialog)
{
    ui->setupUi(this);
    QString desc = idx.data(TransactionTableModel::LongDescriptionRole).toString();
    ui->detailText->setHtml(desc);
	if (msHashBoinc.length() > 1)
	{
		ui->btnExecute->setVisible(true);
	}
	else
	{
		ui->btnExecute->setVisible(false);
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


void TransactionDescDialog::on_btnExecute_clicked()
{
    //printf("Executing code... %s",hashBoinc.c_str());
	std::string code = ExtractXML(msHashBoinc,"<CODE>","</CODE>");
	if (code.length() > 1)
	{
		std::string narr2 = "Are you sure you want to execute this smart contract: " + code;
		//bool result = askQuestion("Confirm Smart Contract Execution",narr2);
		bool result = false;
		askQuestion("Confirm Smart Contract Execution",narr2,&result);
		//QMessageBox::StandardButton retval = QMessageBox::question(uiInterface, tr("Confirm transaction fee"), narr1,         QMessageBox::Yes|QMessageBox::Cancel, QMessageBox::Yes);
		if (result)
		{
			ExecuteCode();
		}
	}

}