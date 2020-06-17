#include "qt/forms/ui_researcherwizard.h"
#include "qt/researcher/researchermodel.h"
#include "qt/researcher/researcherwizard.h"

#include "logging.h"

namespace {
//!
//! \brief Alias for the "start over" custom wizard button.
//!
constexpr QWizard::WizardButton START_OVER_BUTTON = QWizard::CustomButton1;
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Class: ResearcherWizard
// -----------------------------------------------------------------------------

ResearcherWizard::ResearcherWizard(
    QWidget *parent,
    ResearcherModel* researcher_model,
    WalletModel* wallet_model)
    : QWizard(parent)
    , ui(new Ui::ResearcherWizard)
    , m_researcher_model(researcher_model)
{
    ui->setupUi(this);
    configureStartOverButton();

    setAttribute(Qt::WA_DeleteOnClose, true);

    ui->emailPage->setModel(researcher_model);
    ui->projectsPage->setModel(researcher_model);
    ui->beaconPage->setModel(researcher_model, wallet_model);
    ui->authPage->setModel(researcher_model);
    ui->summaryPage->setModel(researcher_model);

    // This enables the beacon "renew" button on the summary page to switch the
    // current page back to beacon page. Since the summary page cannot navigate
    // back to a specific page directly, it emits a signal that we wire up here
    // to change the page when requested:
    //
    connect(ui->summaryPage, SIGNAL(renewBeaconButtonClicked()),
            this, SLOT(onRenewBeaconButtonClicked()));
}

ResearcherWizard::~ResearcherWizard()
{
    m_researcher_model->onWizardClose();

    delete ui;
}

void ResearcherWizard::configureStartOverButton()
{
    setButtonText(START_OVER_BUTTON, tr("&Start Over"));
    connect(this, SIGNAL(customButtonClicked(int)),
            this, SLOT(onCustomButtonClicked(int)));
}

void ResearcherWizard::onCustomButtonClicked(int which)
{
    if (which == START_OVER_BUTTON) {
        setStartId(PageEmail);
        restart();
    }
}

void ResearcherWizard::onRenewBeaconButtonClicked()
{
    // This handles the beacon "renew" button on the summary page. Qt doesn't
    // give us a good way to switch directly to the beacon page, so we rewind
    // and start from it instead:
    //
    setStartId(PageBeacon);
    restart();
}
