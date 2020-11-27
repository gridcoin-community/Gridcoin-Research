#include "guiutil.h"
#include "bitcoinaddressvalidator.h"
#include "walletmodel.h"
#include "bitcoinunits.h"
#include "util.h"
#include "init.h"
#include <codecvt>

#include <QString>
#include <QDateTime>
#include <QDoubleValidator>
#include <QFont>
#include <QLineEdit>
#include <QTextDocument> // For Qt::escape
#include <QUrlQuery>
#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QDesktopServices>
#include <QThread>

#ifdef WIN32
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0501
#ifdef _WIN32_IE
#undef _WIN32_IE
#endif
#define _WIN32_IE 0x0501
#define WIN32_LEAN_AND_MEAN 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "shlwapi.h"
#include "shlobj.h"
#include "shellapi.h"
#endif

namespace GUIUtil {

QString dateTimeStr(const QDateTime &date)
{
    return date.date().toString(Qt::SystemLocaleShortDate) + QString(" ") + date.toString("hh:mm");
}

QString dateTimeStr(qint64 nTime)
{
    return dateTimeStr(QDateTime::fromTime_t((qint32)nTime));
}

QString formatPingTime(double dPingTime)
{
    return (dPingTime == std::numeric_limits<int64_t>::max()/1e6 || dPingTime == 0) ? QObject::tr("N/A") : QString(QObject::tr("%1 ms")).arg(QString::number((int)(dPingTime * 1000), 10));
}

QString formatTimeOffset(int64_t nTimeOffset)
{
  return QString(QObject::tr("%1 s")).arg(QString::number((int)nTimeOffset, 10));
}

QString formatBytes(uint64_t bytes)
{
    if(bytes < 1024)
        return QString(QObject::tr("%1 B")).arg(bytes);
    if(bytes < 1024 * 1024)
        return QString(QObject::tr("%1 KB")).arg(bytes / 1024);
    if(bytes < 1024 * 1024 * 1024)
        return QString(QObject::tr("%1 MB")).arg(bytes / 1024 / 1024);

    return QString(QObject::tr("%1 GB")).arg(bytes / 1024 / 1024 / 1024);
}

QString formatDurationStr(int secs)
{
    QStringList strList;
    int days = secs / 86400;
    int hours = (secs % 86400) / 3600;
    int mins = (secs % 3600) / 60;
    int seconds = secs % 60;

    if (days)
        strList.append(QString(QObject::tr("%1 d")).arg(days));
    if (hours)
        strList.append(QString(QObject::tr("%1 h")).arg(hours));
    if (mins)
        strList.append(QString(QObject::tr("%1 m")).arg(mins));
    if (seconds || (!days && !hours && !mins))
        strList.append(QString(QObject::tr("%1 s")).arg(seconds));

    return strList.join(" ");
}

QString formatServicesStr(quint64 mask)
{
    QStringList strList;

    // Just scan the last 8 bits for now.
    for (int i = 0; i < 8; i++) {
        uint64_t check = 1 << i;
        if (mask & check)
        {
            switch (check)
            {
            case NODE_NETWORK:
                strList.append("NETWORK");
                break;
            // These are for Bitcoin and are unused right now.
            //case NODE_GETUTXO:
            //    strList.append("GETUTXO");
            //    break;
            //case NODE_BLOOM:
            //    strList.append("BLOOM");
            //   break;
            //case NODE_WITNESS:
            //    strList.append("WITNESS");
            //    break;
            //case NODE_XTHIN:
            //    strList.append("XTHIN");
            //    break;
            default:
                strList.append(QString("%1[%2]").arg("UNKNOWN").arg(check));
            }
        }
    }

    if (strList.size())
        return strList.join(" & ");
    else
        return QObject::tr("None");
}

QFont bitcoinAddressFont()
{
    QFont font("Monospace");
    font.setStyleHint(QFont::Monospace);
    return font;
}

void setupAddressWidget(QLineEdit *widget, QWidget *parent)
{
    widget->setMaxLength(BitcoinAddressValidator::MaxAddressLength);
    widget->setValidator(new BitcoinAddressValidator(parent));
    widget->setFont(bitcoinAddressFont());
}

void setupAmountWidget(QLineEdit *widget, QWidget *parent)
{
    QDoubleValidator *amountValidator = new QDoubleValidator(parent);
    amountValidator->setDecimals(8);
    amountValidator->setBottom(0.0);
    widget->setValidator(amountValidator);
    widget->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
}

bool parseBitcoinURI(const QUrl &uri, SendCoinsRecipient *out)
{
    // NovaCoin: check prefix
    if(uri.scheme() != QString("gridcoin"))
        return false;

    SendCoinsRecipient rv;
    rv.address = uri.path();
    rv.amount = 0;
    QUrlQuery uriQuery(uri);
    QList<QPair<QString, QString> > items = uriQuery.queryItems();
    for (QList<QPair<QString, QString> >::iterator i = items.begin(); i != items.end(); i++)
    {
        bool fShouldReturnFalse = false;
        if (i->first.startsWith("req-"))
        {
            i->first.remove(0, 4);
            fShouldReturnFalse = true;
        }

        if (i->first == "label")
        {
            rv.label = i->second;
            fShouldReturnFalse = false;
        }
        else if (i->first == "amount")
        {
            if(!i->second.isEmpty())
            {
                if(!BitcoinUnits::parse(BitcoinUnits::BTC, i->second, &rv.amount))
                {
                    return false;
                }
            }
            fShouldReturnFalse = false;
        }

        if (fShouldReturnFalse)
            return false;
    }
    if(out)
    {
        *out = rv;
    }
    return true;
}

bool parseBitcoinURI(QString uri, SendCoinsRecipient *out)
{
    // Convert gridcoin:// to gridcoin:
    //
    //    Cannot handle this later, because bitcoin:// will cause Qt to see the part after // as host,
    //    which will lower-case it (and thus invalidate the address).

    if(uri.startsWith("gridcoin://"))
    {
        uri.replace(0, 11, "gridcoin:");
    }
    QUrl uriInstance(uri);
    return parseBitcoinURI(uriInstance, out);
}

QString HtmlEscape(const QString& str, bool fMultiLine)
{
    QString escaped = str.toHtmlEscaped();
    if(fMultiLine)
    {
        escaped = escaped.replace("\n", "<br>\n");
    }
    return escaped;
}

QString HtmlEscape(const std::string& str, bool fMultiLine)
{
    return HtmlEscape(QString::fromStdString(str), fMultiLine);
}

void copyEntryData(QAbstractItemView *view, int column, int role)
{
    if(!view || !view->selectionModel())
        return;
    QModelIndexList selection = view->selectionModel()->selectedRows(column);

    if(!selection.isEmpty())
    {
        // Copy first item
        QApplication::clipboard()->setText(selection.at(0).data(role).toString());
    }
}

QList<QModelIndex> getEntryData(QAbstractItemView *view, int column)
{
    if(!view || !view->selectionModel())
        return QList<QModelIndex>();
    return view->selectionModel()->selectedRows(column);
}

fs::path qstringToBoostPath(const QString &path)
{
    return fs::path(path.toStdString());
}

QString boostPathToQString(const fs::path &path)
{
    return QString::fromStdString(path.string());
}

QString getDefaultDataDirectory()
{
    return boostPathToQString(GetDefaultDataDir());
}

QString getSaveFileName(QWidget *parent, const QString &caption,
                                 const QString &dir,
                                 const QString &filter,
                                 QString *selectedSuffixOut)
{
    QString selectedFilter;

    // Default to user documents location
    QString myDir = dir.isEmpty()
                    ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                    : dir;
    QString result = QFileDialog::getSaveFileName(parent, caption, myDir, filter, &selectedFilter);

    /* Extract first suffix from filter pattern "Description (*.foo)" or "Description (*.foo *.bar ...) */
    QRegExp filter_re(".* \\(\\*\\.(.*)[ \\)]");
    QString selectedSuffix;
    if(filter_re.exactMatch(selectedFilter))
    {
        selectedSuffix = filter_re.cap(1);
    }

    /* Add suffix if needed */
    QFileInfo info(result);
    if(!result.isEmpty())
    {
        if(info.suffix().isEmpty() && !selectedSuffix.isEmpty())
        {
            /* No suffix specified, add selected suffix */
            if(!result.endsWith("."))
                result.append(".");
            result.append(selectedSuffix);
        }
    }

    /* Return selected suffix if asked to */
    if(selectedSuffixOut)
    {
        *selectedSuffixOut = selectedSuffix;
    }
    return result;
}

Qt::ConnectionType blockingGUIThreadConnection()
{
    if(QThread::currentThread() != QCoreApplication::instance()->thread())
    {
        return Qt::BlockingQueuedConnection;
    }
    else
    {
        return Qt::DirectConnection;
    }
}

bool checkPoint(const QPoint &p, const QWidget *w)
{
    QWidget *atW = qApp->widgetAt(w->mapToGlobal(p));
    if (!atW) return false;
    return atW->topLevelWidget() == w;
}

bool isObscured(QWidget *w)
{
    return !(checkPoint(QPoint(0, 0), w)
        && checkPoint(QPoint(w->width() - 1, 0), w)
        && checkPoint(QPoint(0, w->height() - 1), w)
        && checkPoint(QPoint(w->width() - 1, w->height() - 1), w)
        && checkPoint(QPoint(w->width() / 2, w->height() / 2), w));
}

void openDebugLogfile()
{
    fs::path pathDebug = GetDataDir() / "debug.log";

    /* Open debug.log with the associated application */
    if (fs::exists(pathDebug))
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(pathDebug.string())));
}

ToolTipToRichTextFilter::ToolTipToRichTextFilter(int size_threshold, QObject *parent) :
    QObject(parent), size_threshold(size_threshold)
{

}

bool ToolTipToRichTextFilter::eventFilter(QObject *obj, QEvent *evt)
{
    if(evt->type() == QEvent::ToolTipChange)
    {
        QWidget *widget = static_cast<QWidget*>(obj);
        QString tooltip = widget->toolTip();
        if(tooltip.size() > size_threshold && !tooltip.startsWith("<qt>") && !Qt::mightBeRichText(tooltip))
        {
            // Prefix <qt/> to make sure Qt detects this as rich text
            // Escape the current message as HTML and replace \n by <br>
            tooltip = "<qt>" + HtmlEscape(tooltip, true) + "<qt/>";
            widget->setToolTip(tooltip);
            return true;
        }
    }
    return QObject::eventFilter(obj, evt);
}

WindowContextHelpButtonHintFilter::WindowContextHelpButtonHintFilter(QObject *parent) :
    QObject(parent)
{

}

bool WindowContextHelpButtonHintFilter::eventFilter (QObject *obj, QEvent *event)
{
    if (event->type () == QEvent::Create)
    {
        if (obj->isWidgetType ())
        {
            auto w = static_cast<QWidget *> (obj);
            w->setWindowFlags (w->windowFlags () & (~Qt::WindowContextHelpButtonHint));
        }
    }
    return QObject::eventFilter (obj, event);
}


struct AutoStartupArguments
{
    std::string link_name_suffix;
    fs::path data_dir;
    std::string arguments;
};

AutoStartupArguments GetAutoStartupArguments(bool fStartMin = true)
{
    // This helper function checks for the presence of certain startup arguments
    // to the current running instance that should be relevant for automatic restart
    // (currently testnet, datadir, scraper, explorer). It adds -testnet
    // to the link name as a suffix if -testnet is specified otherwise adds mainnet,
    // and then adds the other three as arguments if they were specified for the
    // running instance. This allows two different automatic startups, one for
    // mainnet, and the other for testnet, and each of them can have different datadir,
    // scraper, and/or explorer arguments.

    AutoStartupArguments result;

    // We do NOT want testnet appended here for the path in the autostart
    // shortcut, so use false in GetDataDir().
    result.data_dir = GetDataDir(false);

    result.arguments = {};

    if (fTestNet)
    {
        result.link_name_suffix += "-testnet";
        result.arguments = "-testnet";
    }
    else
    {
        result.link_name_suffix += "-mainnet";
    }

    if (fStartMin)
    {
        result.arguments += " -min";
    }

    for (const auto& flag : { "-scraper", "-explorer" })
    {
        if (GetBoolArg(flag))
        {
            (result.arguments += " ") += flag;
        }
    }

    return result;
}

#ifdef WIN32
fs::path static StartupShortcutLegacyPath()
{
    return GetSpecialFolderPath(CSIDL_STARTUP) / "Gridcoin.lnk";
}

fs::path static StartupShortcutPath()
{
    std::string link_name_suffix = GetAutoStartupArguments().link_name_suffix;
    std::string link_name_root = "Gridcoin";

    return GetSpecialFolderPath(CSIDL_STARTUP) / (link_name_root + link_name_suffix + ".lnk");
}

bool GetStartOnSystemStartup()
{
    // check for Gridcoin.lnk
    return fs::exists(StartupShortcutPath());
}

bool SetStartOnSystemStartup(bool fAutoStart, bool fStartMin)
{
    // Remove the legacy shortcut unconditionally.
    fs::remove(StartupShortcutLegacyPath());

    // If the shortcut exists already, remove it for updating
    fs::remove(StartupShortcutPath());

    // Get auto startup arguments
    AutoStartupArguments autostartup = GetAutoStartupArguments(fStartMin);

    if (fAutoStart)
    {
        CoInitialize(NULL);

        // Get a pointer to the IShellLink interface.
        IShellLinkW* psl = NULL;
        HRESULT hres = CoCreateInstance(CLSID_ShellLink, NULL,
                                CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                                reinterpret_cast<void**>(&psl));

        if (SUCCEEDED(hres))
        {
            // Get the current executable path
            WCHAR pszExePath[MAX_PATH];
            GetModuleFileNameW(NULL, pszExePath, sizeof(pszExePath));

            std::wstring autostartup_arguments;
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

            if (!autostartup.data_dir.empty())
            {
                autostartup_arguments = converter.from_bytes("-datadir=\"")
                        + autostartup.data_dir.wstring()
                        + converter.from_bytes("\" ");
            }

            autostartup_arguments += converter.from_bytes(autostartup.arguments);

            LPCTSTR pszArgs = autostartup_arguments.c_str();

            // Set the path to the shortcut target
            psl->SetPath(pszExePath);
            PathRemoveFileSpecW(pszExePath);
            psl->SetWorkingDirectory(pszExePath);

            if (fStartMin)
            {
                psl->SetShowCmd(SW_SHOWMINNOACTIVE);
            }
            else
            {
                psl->SetShowCmd(SW_SHOWNORMAL);
            }

            psl->SetArguments(pszArgs);

            // Query IShellLink for the IPersistFile interface for
            // saving the shortcut in persistent storage.
            IPersistFile* ppf = NULL;
            hres = psl->QueryInterface(IID_IPersistFile,
                                       reinterpret_cast<void**>(&ppf));
            if (SUCCEEDED(hres))
            {
                // Save the link by calling IPersistFile::Save.
                hres = ppf->Save(StartupShortcutPath().wstring().c_str(), TRUE);
                ppf->Release();
                psl->Release();
                CoUninitialize();
                return true;
            }
            psl->Release();
        }
        CoUninitialize();
        return false;
    }
    return true;
}

#elif defined(Q_OS_LINUX)

// Follow the Desktop Application Autostart Spec:
//  http://standards.freedesktop.org/autostart-spec/autostart-spec-latest.html

fs::path static GetAutostartDir()
{
    char* pszConfigHome = getenv("XDG_CONFIG_HOME");
    if (pszConfigHome) return fs::path(pszConfigHome) / "autostart";
    char* pszHome = getenv("HOME");
    if (pszHome) return fs::path(pszHome) / ".config" / "autostart";
    return fs::path();
}

fs::path static GetAutostartLegacyFilePath()
{
    return GetAutostartDir() / "gridcoin.desktop";
}

fs::path static GetAutostartFilePath()
{
    std::string link_name_suffix = GetAutoStartupArguments().link_name_suffix;
    std::string link_name_root = "gridcoin";

    return GetAutostartDir() / (link_name_root + link_name_suffix + ".desktop");
}

bool GetStartOnSystemStartup()
{
    fsbridge::ifstream optionFile(GetAutostartFilePath());
    if (!optionFile.good())
        return false;
    // Scan through file for "Hidden=true":
    std::string line;
    while (!optionFile.eof())
    {
        getline(optionFile, line);
        if (line.find("Hidden") != std::string::npos &&
            line.find("true") != std::string::npos)
            return false;
    }
    optionFile.close();

    return true;
}

bool SetStartOnSystemStartup(bool fAutoStart, bool fStartMin)
{
    // Remove legacy autostart path if it exists.
    if (fs::exists(GetAutostartLegacyFilePath()))
    {
        fs::remove(GetAutostartLegacyFilePath());
    }

    if (!fAutoStart)
        fs::remove(GetAutostartFilePath());
    else
    {
        char pszExePath[MAX_PATH+1];
        memset(pszExePath, 0, sizeof(pszExePath));
        if (readlink("/proc/self/exe", pszExePath, sizeof(pszExePath)-1) == -1)
            return false;

        AutoStartupArguments autostartup = GetAutoStartupArguments(fStartMin);

        fs::create_directories(GetAutostartDir());

        fsbridge::ofstream optionFile(GetAutostartFilePath(), std::ios_base::out|std::ios_base::trunc);
        if (!optionFile.good())
            return false;
        // Write a bitcoin.desktop file to the autostart directory:
        optionFile << "[Desktop Entry]\n";
        optionFile << "Type=Application\n";
        optionFile << "Name=Gridcoin" + autostartup.link_name_suffix + "\n";
        optionFile << "Exec=" << static_cast<fs::path>(pszExePath);

        if (!autostartup.data_dir.empty())
        {
            optionFile << " -datadir=" << autostartup.data_dir;
        }

        optionFile << " " << autostartup.arguments << "\n";
        optionFile << "Terminal=false\n";
        optionFile << "Hidden=false\n";
        optionFile.close();
    }
    return true;
}
#else

// TODO: OSX startup stuff; see:
// https://developer.apple.com/library/mac/#documentation/MacOSX/Conceptual/BPSystemStartup/Articles/CustomLogin.html

bool GetStartOnSystemStartup() { return false; }
bool SetStartOnSystemStartup(bool fAutoStart, bool fStartMin) { return false; }

#endif

HelpMessageBox::HelpMessageBox(QWidget *parent) :
    QMessageBox(parent)
{
    header = "gridcoinresearch " + tr("version") + " " +
        QString::fromStdString(FormatFullVersion()) + "\n\n" +
        tr("Usage:") + "\n" +
        "  gridcoin-qt [" + tr("command-line options") + "]                     " + "\n";

    coreOptions = QString::fromStdString(HelpMessage());

    uiOptions = tr("UI options") + ":\n" +
        "  -lang=<lang>           " + tr("Set language, for example \"de_DE\" (default: system locale)") + "\n" +
        "  -min                   " + tr("Start minimized") + "\n" +
        "  -splash                " + tr("Show splash screen on startup (default: 1)") + "\n";

    setWindowTitle(tr("Gridcoin-Qt"));
    setTextFormat(Qt::PlainText);
    // setMinimumWidth is ignored for QMessageBox so put in non-breaking spaces to make it wider.
    setText(header + QString(QChar(0x2003)).repeated(50));
    setDetailedText(coreOptions + "\n" + uiOptions);

    setStandardButtons(QMessageBox::Ok);
    setDefaultButton(QMessageBox::Ok);
    setEscapeButton(QMessageBox::Ok);
}

void HelpMessageBox::printToConsole()
{
    // On other operating systems, the expected action is to print the message to the console.
    QString strUsage = header + "\n" + coreOptions + "\n" + uiOptions;
    fprintf(stdout, "%s", strUsage.toStdString().c_str());
}

void HelpMessageBox::showOrPrint()
{
#if defined(WIN32)
        // On Windows, show a message box, as there is no stderr/stdout in windowed applications
        exec();
#else
        // On other operating systems, print help text to console
        printToConsole();
#endif
}

} // namespace GUIUtil
