#ifndef BITCOIN_QT_GUIUTIL_H
#define BITCOIN_QT_GUIUTIL_H

#include <QString>
#include <QObject>
#include <QMessageBox>
#include <memory>
#include "fs.h"
#include "qt/guiipcinfo.h"
#include "support/allocators/secure.h" // For SecureString.

class ArgsManager;
class QSettings;

QT_BEGIN_NAMESPACE
class QFont;
class QLineEdit;
class QWidget;
class QDateTime;
class QUrl;
class QAbstractItemView;
QT_END_NAMESPACE
class SendCoinsRecipient;

/** Utility functions used by the Bitcoin Qt UI.
 */
namespace GUIUtil
{
    // Create human-readable string from date
    QString dateTimeStr(const QDateTime &datetime);
    QString dateTimeStr(qint64 nTime);

    // Format Ping Time
    QString formatPingTime(double dPingTime);

    // Format Node Time Offset
    QString formatTimeOffset(int64_t nTimeOffset);

    QString formatNiceTimeOffset(qint64 secs);

    // Format Bytes
    QString formatBytes(uint64_t bytes);

    //Format Duration
    QString formatDurationStr(int secs);

    // Format Services
    QString formatServicesStr(quint64 mask);

    // Render Bitcoin addresses in monospace font
    QFont bitcoinAddressFont();

    // Set up widgets for address and amounts
    void setupAddressWidget(QLineEdit *widget, QWidget *parent);
    void setupAmountWidget(QLineEdit *widget, QWidget *parent);

    // Parse "gridcoin:" URI into recipient object, return true on successful parsing
    // See Bitcoin URI definition discussion here: https://bitcointalk.org/index.php?topic=33490.0
    bool parseBitcoinURI(const QUrl &uri, SendCoinsRecipient *out);
    bool parseBitcoinURI(QString uri, SendCoinsRecipient *out);

    // HTML escaping for rich text controls
    QString HtmlEscape(const QString& str, bool fMultiLine=false);
    QString HtmlEscape(const std::string& str, bool fMultiLine=false);

    //! The UTF-8 bytes of \p text, the same bytes as text.toStdString(), in a
    //! SecureString. The QByteArray used for the conversion is cleansed before
    //! return, and the bytes are in the string's secure buffer whatever their
    //! length, so moving the result moves that buffer and copies nothing. Like
    //! any SecureString, it throws std::bad_alloc past the secure pool's 256 KiB.
    SecureString SecureStringFromQString(const QString& text);

    /** Copy a field of the currently selected entry of a view to the clipboard. Does nothing if nothing
        is selected.
       @param[in] column  Data column to extract from the model
       @param[in] role    Data role to extract from the model
       @see  TransactionView::copyLabel, TransactionView::copyAmount, TransactionView::copyAddress
     */
    void copyEntryData(QAbstractItemView *view, int column, int role=Qt::EditRole);

    QList<QModelIndex> getEntryData(QAbstractItemView *view, int column);

    fs::path qstringToBoostPath(const QString &path);

    QString boostPathToQString(const fs::path &path);

    QString getDefaultDataDirectory();

    //! Namespace a QSettings key per node (datadir) so multiple wallet instances
    //! on one machine don't overwrite each other's GUI preferences. Returns \p key
    //! unchanged for the default datadir (preserving the existing flat keys, so the
    //! primary node needs no migration), and prefixes a per-datadir group
    //! ("node_<sanitized datadir>/") otherwise. Combined with the network-scoped
    //! application name, this isolates each (network, datadir) node. MUST be called
    //! only after the datadir is finalized (GetDataDir() valid) -- see
    //! OptionsModel::readNodeSettings and the note in bitcoin.cpp.
    QString nodeSettingsKey(const QString& key);

    /** Extract the first suffix from a filter pattern "Description (*.foo)" or
        "Description (*.foo *.bar ...)"; empty if none.

      @param[in] filter  Filter specification such as "Comma Separated Files (*.csv)"
      @returns the first suffix, without its "*." prefix
     */
    QString ExtractFirstSuffixFromFilter(const QString& filter);

    /** Get save filename, mimics QFileDialog::getSaveFileName, except that it appends a default suffix
        when no suffix is provided by the user.

      @param[in] parent  Parent window (or 0)
      @param[in] caption Window caption (or empty, for default)
      @param[in] dir     Starting directory (or empty, to default to documents directory)
      @param[in] filter  Filter specification such as "Comma Separated Files (*.csv)"
      @param[out] selectedSuffixOut  Pointer to return the suffix (file type) that was selected (or 0).
                  Can be useful when choosing the save file format based on suffix.
     */
    QString getSaveFileName(QWidget *parent=0, const QString &caption=QString(),
                                   const QString &dir=QString(), const QString &filter=QString(),
                                   QString *selectedSuffixOut=0);

    /** Get connection type to call object slot in GUI thread with invokeMethod. The call will be blocking.

       @returns If called from the GUI thread, return a Qt::DirectConnection.
                If called from another thread, return a Qt::BlockingQueuedConnection.
    */
    Qt::ConnectionType blockingGUIThreadConnection();

    // Determine whether a widget is hidden behind other windows
    bool isObscured(QWidget *w);

    //! What to do with a file that lives on the core's side.
    enum class OpenAction { OpenFile, ShowAdminMessage, Missing };

    //! core_other_account -> ShowAdminMessage; else the file exists -> OpenFile;
    //! else Missing. The file is not looked at when the core runs under another
    //! account, since this account cannot be expected to read it.
    OpenAction DecideCoreFileOpen(bool core_other_account, const fs::path& path);

    //! The message that tells the user how to read a core file this account
    //! cannot read. Names the path twice (where it is, and in the command).
    QString AdminReadMessage(const QString& what, const QString& path);

    //! A message box for text that carries a core-reported path: plain text,
    //! never rich text.
    std::unique_ptr<QMessageBox> MakeCoreFileBox(QMessageBox::Icon icon, QWidget* parent, const QString& title,
                                                 const QString& text);

    //! Open a file at a path the core reported with the associated application,
    //! or tell the user why it cannot be opened from here. what is the display
    //! name of the file ("debug log", "config file").
    void openCoreFile(const GuiIpcInfo& info, const QString& what, const std::string& path, QWidget* parent);

    //! Open the core's debug log.
    void openDebugLogfile(const GuiIpcInfo& info, QWidget* parent);

    /** Qt event filter that intercepts ToolTipChange events, and replaces the tooltip with a rich text
      representation if needed. This assures that Qt can word-wrap long tooltip messages.
      Tooltips longer than the provided size threshold (in characters) are wrapped.
     */
    class ToolTipToRichTextFilter : public QObject
    {
        Q_OBJECT

    public:
        explicit ToolTipToRichTextFilter(int size_threshold, QObject* parent = nullptr);

    protected:
        bool eventFilter(QObject *obj, QEvent *evt);

    private:
        int size_threshold;
    };

    /** Qt event filter that suppress context help question mark for all windows.
     */

    class WindowContextHelpButtonHintFilter : public QObject
    {
        Q_OBJECT

    public:
        explicit WindowContextHelpButtonHintFilter(QObject* parent = nullptr);

    protected:
        bool eventFilter(QObject *obj, QEvent *evt);
    };


    bool GetStartOnSystemStartup();
    bool SetStartOnSystemStartup(bool fAutoStart, bool fStartMin);

    /** Help message for Bitcoin-Qt, shown with --help. */
    class HelpMessageBox : public QMessageBox
    {
        Q_OBJECT

    public:
        HelpMessageBox(QWidget* parent = nullptr);

        /** Show message box or print help message to standard output, based on operating system. */
        void showAndPrint();

        /** Print help message to console */
        void printToConsole();

    private:
        QString header;
        QString options;
    };

    /**
     * Returns the start-moment of the day in local time.
     *
     * QDateTime::QDateTime(const QDate& date) is deprecated since Qt 5.15.
     * QDate::startOfDay() was introduced in Qt 5.14.
     */
    QDateTime StartOfDay(const QDate& date);

    //! If the command line sets -multiprocess/-nomultiprocess, -ipcdir or -ipcserviceuser, store
    //! them in \p settings ("launchMultiprocess" bool, "launchIpcDir", "launchIpcServiceUser"
    //! strings); otherwise SoftSet them from \p settings. A negated or empty -ipcdir or
    //! -ipcserviceuser means the default: it removes the stored value, and an empty stored value
    //! is not SoftSet. Used on macOS, where the login item (SMAppService.mainApp) cannot carry
    //! arguments.
    void ApplyPersistedLaunchMode(QSettings& settings, ArgsManager& args);

} // namespace GUIUtil

#endif // BITCOIN_QT_GUIUTIL_H
