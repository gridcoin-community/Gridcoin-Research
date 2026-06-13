// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/forms/ui_researcherwizardprojectspage.h"
#include "qt/researcher/projecttablemodel.h"
#include "qt/researcher/researchermodel.h"
#include "qt/researcher/researcherwizardprojectspage.h"

#include "gridcoin/boinc.h"
#include "gridcoin/support/xml.h"
#include "util/system.h"

#include <QCryptographicHash>
#include <QFile>
#include <QInputDialog>
#include <QMessageBox>
#include <QRegularExpression>

#include <optional>
#include <cstddef>
#include <string>
#include <vector>

namespace {
struct BoincProjectSnapshot
{
    QString name;
    QString email_hash;
    QString external_cpid;
    QString user_name;
};

QString toLowerTrimmed(QString s)
{
    return s.trimmed().toLower();
}

QString md5HexLower(const QString& s)
{
    return QString(QCryptographicHash::hash(s.toUtf8(), QCryptographicHash::Md5).toHex());
}

std::optional<std::string> readClientStateXml()
{
    const fs::path boinc_dir = GRC::GetBoincDataDir();

    if (boinc_dir.empty()) {
        return std::nullopt;
    }

    const QString file_path = QString::fromStdString((boinc_dir / "client_state.xml").string());
    QFile f(file_path);

    if (!f.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }

    const QByteArray bytes = f.readAll();

    return std::make_optional<std::string>(bytes.constData(), static_cast<size_t>(bytes.size()));
}

std::vector<std::string> splitProjectNodes(const std::string& client_state_xml)
{
    std::vector<std::string> nodes;
    const std::string delimiter = "<project>";

    std::string::size_type pos = 0;
    while ((pos = client_state_xml.find(delimiter, pos)) != std::string::npos) {
        pos += delimiter.size();

        // Each node ends at the closing tag. Keep the node content only.
        const std::string::size_type end = client_state_xml.find("</project>", pos);
        if (end == std::string::npos) {
            break;
        }

        nodes.emplace_back(client_state_xml.substr(pos, end - pos));
    }

    return nodes;
}

std::vector<BoincProjectSnapshot> parseBoincProjectSnapshots(const std::string& client_state_xml)
{
    std::vector<BoincProjectSnapshot> out;

    for (const auto& node : splitProjectNodes(client_state_xml)) {
        BoincProjectSnapshot p;
        p.name = QString::fromStdString(ExtractXML(node, "<project_name>", "</project_name>"));
        p.email_hash = toLowerTrimmed(QString::fromStdString(ExtractXML(node, "<email_hash>", "</email_hash>")));
        p.external_cpid = toLowerTrimmed(QString::fromStdString(ExtractXML(node, "<external_cpid>", "</external_cpid>")));
        p.user_name = toLowerTrimmed(QString::fromStdString(ExtractXML(node, "<user_name>", "</user_name>")));

        if (!p.name.isEmpty()) {
            out.emplace_back(std::move(p));
        }
    }

    return out;
}

QString recommendForceCpid(const std::vector<ProjectRow>& rows)
{
    const QRegularExpression cpid_re("^[0-9a-f]{32}$");
    QHash<QString, int> counts;

    for (const auto& row : rows) {
        const QString cpid = toLowerTrimmed(row.m_cpid);
        if (!cpid_re.match(cpid).hasMatch()) {
            continue;
        }

        counts[cpid] = counts.value(cpid, 0) + 1;
    }

    if (counts.size() <= 1) {
        return QString();
    }

    QString best;
    int best_count = -1;

    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        if (it.value() > best_count) {
            best = it.key();
            best_count = it.value();
        }
    }

    return best;
}
} // namespace

// -----------------------------------------------------------------------------
// Class: ResearcherWizardProjectsPage
// -----------------------------------------------------------------------------

ResearcherWizardProjectsPage::ResearcherWizardProjectsPage(QWidget *parent)
    : QWizardPage(parent)
    , ui(new Ui::ResearcherWizardProjectsPage)
    , m_researcher_model(nullptr)
    , m_table_model(nullptr)
{
    ui->setupUi(this);
    ui->projectTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

ResearcherWizardProjectsPage::~ResearcherWizardProjectsPage()
{
    delete ui;
    delete m_table_model;
}

void ResearcherWizardProjectsPage::setModel(ResearcherModel *model)
{
    this->m_researcher_model = model;

    if (!m_researcher_model) {
        return;
    }

    m_table_model = new ProjectTableModel(model, false);

    if (!m_table_model) {
        return;
    }

    ui->projectTableView->setModel(m_table_model);
    ui->projectTableView->hideColumn(ProjectTableModel::Magnitude);

    connect(ui->refreshButton, &QPushButton::clicked, this, &ResearcherWizardProjectsPage::refresh);
}

void ResearcherWizardProjectsPage::initializePage()
{
    if (!m_researcher_model) {
        return;
    }

    QString email = toLowerTrimmed(field("emailAddress").toString());

    const std::optional<std::string> client_state_xml = readClientStateXml();
    std::vector<BoincProjectSnapshot> boinc_projects;

    if (client_state_xml) {
        boinc_projects = parseBoincProjectSnapshots(*client_state_xml);
    }

    if (!boinc_projects.empty()) {
        // BOINC stores only the MD5 of the account email in client_state.xml, so validate by hash.
        QString email_hash = md5HexLower(email);

        const auto matches_hash = [&](const QString& hash) {
            for (const auto& p : boinc_projects) {
                if (!p.email_hash.isEmpty() && p.email_hash == hash) {
                    return true;
                }
            }
            return false;
        };

        // Keep asking until the email matches at least one attached project, or the user cancels.
        while (!email.isEmpty() && !matches_hash(email_hash)) {
            bool ok = false;
            const QString prompt = tr("The email address does not match any BOINC projects currently attached on this "
                                      "computer.\n\nPlease enter the BOINC account email you used when you registered "
                                      "for your projects.");

            const QString entered = QInputDialog::getText(
                this,
                windowTitle(),
                prompt,
                QLineEdit::Normal,
                email,
                &ok);

            if (!ok) {
                break;
            }

            email = toLowerTrimmed(entered);
            email_hash = md5HexLower(email);
        }

        if (email != toLowerTrimmed(field("emailAddress").toString())) {
            setField("emailAddress", email);
        }

        // Warn if some projects use a different email hash. We can't show the plaintext email, so list projects.
        if (!email.isEmpty() && matches_hash(email_hash)) {
            QStringList mismatched_projects;
            for (const auto& p : boinc_projects) {
                if (!p.email_hash.isEmpty() && p.email_hash != email_hash) {
                    mismatched_projects << p.name;
                }
            }

            if (!mismatched_projects.isEmpty()) {
                QMessageBox::information(
                    this,
                    windowTitle(),
                    tr("Some BOINC projects appear to be attached using a different account email. "
                       "This may cause CPID mismatch or split CPID warnings.\n\n"
                       "Projects not matching the email you entered:\n- %1")
                        .arg(mismatched_projects.join("\n- ")),
                    QMessageBox::Ok,
                    QMessageBox::Ok);
            }
        }

        if (client_state_xml->find("scienceunited") != std::string::npos) {
            QMessageBox::information(
                this,
                windowTitle(),
                tr("Science United account manager detected.\n\n"
                   "Science United is not compatible with Gridcoin solo crunching. Please detach Science United "
                   "in BOINC Manager (Tools -> Use account manager) and then refresh."),
                QMessageBox::Ok,
                QMessageBox::Ok);
        }
    }

    // Process the email address input from the previous page:
    if (!m_researcher_model->switchToSolo(email)) {
        QMessageBox::warning(
            this,
            windowTitle(),
            tr("An error occurred while saving the email address to the "
                "configuration file. Please see debug.log for details."),
            QMessageBox::Ok,
            QMessageBox::Ok);
    }

    refresh();

    if (m_researcher_model->detectedPoolMode()) {
        QMessageBox::information(
            this,
            windowTitle(),
            tr("BOINC appears to be attached to a Gridcoin pool account.\n\n"
               "Gridcoin solo crunching requires that your BOINC projects are attached to your personal BOINC "
               "account. Detach from the pool in BOINC Manager, attach your projects with your own email, and then "
               "click Refresh."),
            QMessageBox::Ok,
            QMessageBox::Ok);
    }

    // If BOINC reports multiple eligible CPIDs, offer to temporarily override CPID selection.
    if (m_researcher_model->hasSplitCpid()) {
        const QString existing_force = QString::fromStdString(gArgs.GetArg("-forcecpid", ""));
        const QString suggested = recommendForceCpid(m_researcher_model->buildProjectTable(false));

        if (!suggested.isEmpty() && toLowerTrimmed(existing_force) != suggested) {
            QMessageBox msg(this);
            msg.setWindowTitle(windowTitle());
            msg.setIcon(QMessageBox::Question);
            msg.setText(tr("Multiple CPIDs were detected across your BOINC projects.\n\n"
                           "Gridcoin can temporarily override CPID selection using the forcecpid setting."));
            msg.setInformativeText(tr("Suggested forcecpid: %1").arg(suggested));

            QPushButton* set_btn = msg.addButton(tr("Set forcecpid"), QMessageBox::AcceptRole);
            QPushButton* ignore_btn = msg.addButton(tr("Not now"), QMessageBox::RejectRole);
            Q_UNUSED(ignore_btn);

            msg.exec();

            if (msg.clickedButton() == set_btn) {
                updateRwSetting("forcecpid", util::SettingsValue(suggested.toStdString()));
                gArgs.ForceSetArg("-forcecpid", suggested.toStdString());

                refresh();
            }
        }
    }
}

bool ResearcherWizardProjectsPage::isComplete() const
{
    return m_researcher_model->hasEligibleProjects();
}

void ResearcherWizardProjectsPage::refresh()
{
    m_researcher_model->reload();

    ui->emailLabel->setText(m_researcher_model->email());
    ui->boincPathLabel->setText(m_researcher_model->formatBoincPath());
    ui->selectedCpidLabel->setText(m_researcher_model->formatCpid());

    const int icon_size = ui->selectedCpidIconLabel->width();

    if (m_researcher_model->hasEligibleProjects()) {
        ui->selectedCpidIconLabel->setPixmap(
            QIcon(":/icons/round_green_check").pixmap(icon_size, icon_size));
    } else {
        ui->selectedCpidIconLabel->setPixmap(
            QIcon(":/icons/white_and_red_x").pixmap(icon_size, icon_size));
    }

    m_table_model->refresh();

    emit completeChanged();
}
