#include "qt/forms/ui_researcherwizardauthpage.h"
#include "qt/researcher/researchermodel.h"
#include "qt/researcher/researcherwizardauthpage.h"

#include <QClipboard>

// -----------------------------------------------------------------------------
// Class: ResearcherWizardAuthPage
// -----------------------------------------------------------------------------

ResearcherWizardAuthPage::ResearcherWizardAuthPage(QWidget *parent)
    : QWizardPage(parent)
    , ui(new Ui::ResearcherWizardAuthPage)
    , m_researcher_model(nullptr)
{
    ui->setupUi(this);
}

ResearcherWizardAuthPage::~ResearcherWizardAuthPage()
{
    delete ui;
}

void ResearcherWizardAuthPage::setModel(ResearcherModel* researcher_model)
{
    this->m_researcher_model = researcher_model;

    if (!m_researcher_model) {
        return;
    }

    connect(m_researcher_model, SIGNAL(researcherChanged()), this, SLOT(refresh()));
}

void ResearcherWizardAuthPage::initializePage()
{
    if (!m_researcher_model) {
        return;
    }

    refresh();
}

void ResearcherWizardAuthPage::refresh()
{
    if (!m_researcher_model) {
        return;
    }

    ui->verificationCodeLabel->setText(m_researcher_model->formatBeaconKeyId());
}

void ResearcherWizardAuthPage::on_copyToClipboardButton_clicked()
{
    QApplication::clipboard()->setText(ui->verificationCodeLabel->text());
}
