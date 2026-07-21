/****************************************************************************
** Meta object code from reading C++ file 'bitcoingui.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/qt/bitcoingui.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'bitcoingui.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.9.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN10BitcoinGUIE_t {};
} // unnamed namespace

template <> constexpr inline auto BitcoinGUI::qt_create_metaobjectdata<qt_meta_tag_ZN10BitcoinGUIE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "BitcoinGUI",
        "setNumConnections",
        "",
        "count",
        "setNumBlocks",
        "nTotalBlocks",
        "setDifficulty",
        "difficulty",
        "setMinerStatus",
        "staking",
        "net_weight",
        "coin_weight",
        "etts_days",
        "setEncryptionStatus",
        "status",
        "update",
        "title",
        "version",
        "upgrade_type",
        "message",
        "error",
        "modal",
        "handleURI",
        "strURI",
        "setOptionsStyleSheet",
        "qssFileName",
        "gotoOverviewPage",
        "gotoHistoryPage",
        "gotoAddressBookPage",
        "gotoReceiveCoinsPage",
        "gotoSendCoinsPage",
        "gotoVotingPage",
        "gotoSignMessageTab",
        "addr",
        "gotoVerifyMessageTab",
        "gotoMultisignDialog",
        "gotoPSGTPoolPage",
        "handlePSGTPoolChanged",
        "revision_hash",
        "change_type",
        "reason",
        "optionsClicked",
        "themeToggled",
        "researcherClicked",
        "aboutClicked",
        "openConfigClicked",
        "bxClicked",
        "websiteClicked",
        "exchangeClicked",
        "boincClicked",
        "boincStatsClicked",
        "chatClicked",
        "diagnosticsClicked",
        "peersClicked",
        "resetblockchainClicked",
        "setPrivacy",
        "openWikiClicked",
        "openFaqClicked",
        "openGuidesClicked",
        "tryQuit",
        "trayIconActivated",
        "QSystemTrayIcon::ActivationReason",
        "incomingTransaction",
        "QModelIndex",
        "parent",
        "start",
        "end",
        "encryptWallet",
        "backupWallet",
        "changePassphrase",
        "unlockWallet",
        "lockWallet",
        "showNormalIfMinimized",
        "fToggleHidden",
        "updateStakingIcon",
        "updateScraperIcon",
        "scraperEventtype",
        "updateBeaconIcon",
        "GetEstimatedStakingFrequency",
        "nEstimateTime",
        "handleNewPoll",
        "extracted",
        "QStringList&",
        "expiring_polls",
        "QString&",
        "notification",
        "handleExpiredPoll"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'setNumConnections'
        QtMocHelpers::SlotData<void(int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 },
        }}),
        // Slot 'setNumBlocks'
        QtMocHelpers::SlotData<void(int, int)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 }, { QMetaType::Int, 5 },
        }}),
        // Slot 'setDifficulty'
        QtMocHelpers::SlotData<void(double)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 7 },
        }}),
        // Slot 'setMinerStatus'
        QtMocHelpers::SlotData<void(bool, double, double, double)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 9 }, { QMetaType::Double, 10 }, { QMetaType::Double, 11 }, { QMetaType::Double, 12 },
        }}),
        // Slot 'setEncryptionStatus'
        QtMocHelpers::SlotData<void(int)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 14 },
        }}),
        // Slot 'update'
        QtMocHelpers::SlotData<void(const QString &, const QString &, const int &, const QString &)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 16 }, { QMetaType::QString, 17 }, { QMetaType::Int, 18 }, { QMetaType::QString, 19 },
        }}),
        // Slot 'error'
        QtMocHelpers::SlotData<void(const QString &, const QString &, bool)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 16 }, { QMetaType::QString, 19 }, { QMetaType::Bool, 21 },
        }}),
        // Slot 'handleURI'
        QtMocHelpers::SlotData<void(QString)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 23 },
        }}),
        // Slot 'setOptionsStyleSheet'
        QtMocHelpers::SlotData<void(QString)>(24, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 25 },
        }}),
        // Slot 'gotoOverviewPage'
        QtMocHelpers::SlotData<void()>(26, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'gotoHistoryPage'
        QtMocHelpers::SlotData<void()>(27, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'gotoAddressBookPage'
        QtMocHelpers::SlotData<void()>(28, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'gotoReceiveCoinsPage'
        QtMocHelpers::SlotData<void()>(29, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'gotoSendCoinsPage'
        QtMocHelpers::SlotData<void()>(30, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'gotoVotingPage'
        QtMocHelpers::SlotData<void()>(31, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'gotoSignMessageTab'
        QtMocHelpers::SlotData<void(QString)>(32, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 33 },
        }}),
        // Slot 'gotoSignMessageTab'
        QtMocHelpers::SlotData<void()>(32, 2, QMC::AccessPrivate | QMC::MethodCloned, QMetaType::Void),
        // Slot 'gotoVerifyMessageTab'
        QtMocHelpers::SlotData<void(QString)>(34, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 33 },
        }}),
        // Slot 'gotoVerifyMessageTab'
        QtMocHelpers::SlotData<void()>(34, 2, QMC::AccessPrivate | QMC::MethodCloned, QMetaType::Void),
        // Slot 'gotoMultisignDialog'
        QtMocHelpers::SlotData<void()>(35, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'gotoPSGTPoolPage'
        QtMocHelpers::SlotData<void()>(36, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'handlePSGTPoolChanged'
        QtMocHelpers::SlotData<void(QString, quint8, int)>(37, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 38 }, { QMetaType::UChar, 39 }, { QMetaType::Int, 40 },
        }}),
        // Slot 'optionsClicked'
        QtMocHelpers::SlotData<void()>(41, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'themeToggled'
        QtMocHelpers::SlotData<void()>(42, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'researcherClicked'
        QtMocHelpers::SlotData<void()>(43, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'aboutClicked'
        QtMocHelpers::SlotData<void()>(44, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'openConfigClicked'
        QtMocHelpers::SlotData<void()>(45, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'bxClicked'
        QtMocHelpers::SlotData<void()>(46, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'websiteClicked'
        QtMocHelpers::SlotData<void()>(47, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'exchangeClicked'
        QtMocHelpers::SlotData<void()>(48, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'boincClicked'
        QtMocHelpers::SlotData<void()>(49, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'boincStatsClicked'
        QtMocHelpers::SlotData<void()>(50, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'chatClicked'
        QtMocHelpers::SlotData<void()>(51, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'diagnosticsClicked'
        QtMocHelpers::SlotData<void()>(52, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'peersClicked'
        QtMocHelpers::SlotData<void()>(53, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'resetblockchainClicked'
        QtMocHelpers::SlotData<void()>(54, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'setPrivacy'
        QtMocHelpers::SlotData<void()>(55, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'openWikiClicked'
        QtMocHelpers::SlotData<void()>(56, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'openFaqClicked'
        QtMocHelpers::SlotData<void()>(57, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'openGuidesClicked'
        QtMocHelpers::SlotData<void()>(58, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'tryQuit'
        QtMocHelpers::SlotData<bool()>(59, 2, QMC::AccessPrivate, QMetaType::Bool),
        // Slot 'trayIconActivated'
        QtMocHelpers::SlotData<void(QSystemTrayIcon::ActivationReason)>(60, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 61, 40 },
        }}),
        // Slot 'incomingTransaction'
        QtMocHelpers::SlotData<void(const QModelIndex &, int, int)>(62, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 63, 64 }, { QMetaType::Int, 65 }, { QMetaType::Int, 66 },
        }}),
        // Slot 'encryptWallet'
        QtMocHelpers::SlotData<void()>(67, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'backupWallet'
        QtMocHelpers::SlotData<void()>(68, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'changePassphrase'
        QtMocHelpers::SlotData<void()>(69, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'unlockWallet'
        QtMocHelpers::SlotData<void()>(70, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'lockWallet'
        QtMocHelpers::SlotData<void()>(71, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'showNormalIfMinimized'
        QtMocHelpers::SlotData<void(bool)>(72, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 73 },
        }}),
        // Slot 'showNormalIfMinimized'
        QtMocHelpers::SlotData<void()>(72, 2, QMC::AccessPrivate | QMC::MethodCloned, QMetaType::Void),
        // Slot 'updateStakingIcon'
        QtMocHelpers::SlotData<void(bool, double, double, double)>(74, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 9 }, { QMetaType::Double, 10 }, { QMetaType::Double, 11 }, { QMetaType::Double, 12 },
        }}),
        // Slot 'updateScraperIcon'
        QtMocHelpers::SlotData<void(int, int)>(75, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 76 }, { QMetaType::Int, 14 },
        }}),
        // Slot 'updateBeaconIcon'
        QtMocHelpers::SlotData<void()>(77, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'GetEstimatedStakingFrequency'
        QtMocHelpers::SlotData<QString(unsigned int)>(78, 2, QMC::AccessPrivate, QMetaType::QString, {{
            { QMetaType::UInt, 79 },
        }}),
        // Slot 'handleNewPoll'
        QtMocHelpers::SlotData<void()>(80, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'extracted'
        QtMocHelpers::SlotData<void(QStringList &, QString &)>(81, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 82, 83 }, { 0x80000000 | 84, 85 },
        }}),
        // Slot 'handleExpiredPoll'
        QtMocHelpers::SlotData<void()>(86, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<BitcoinGUI, qt_meta_tag_ZN10BitcoinGUIE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject BitcoinGUI::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10BitcoinGUIE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10BitcoinGUIE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN10BitcoinGUIE_t>.metaTypes,
    nullptr
} };

void BitcoinGUI::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<BitcoinGUI *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->setNumConnections((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 1: _t->setNumBlocks((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 2: _t->setDifficulty((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 3: _t->setMinerStatus((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[4]))); break;
        case 4: _t->setEncryptionStatus((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 5: _t->update((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4]))); break;
        case 6: _t->error((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[3]))); break;
        case 7: _t->handleURI((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 8: _t->setOptionsStyleSheet((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 9: _t->gotoOverviewPage(); break;
        case 10: _t->gotoHistoryPage(); break;
        case 11: _t->gotoAddressBookPage(); break;
        case 12: _t->gotoReceiveCoinsPage(); break;
        case 13: _t->gotoSendCoinsPage(); break;
        case 14: _t->gotoVotingPage(); break;
        case 15: _t->gotoSignMessageTab((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 16: _t->gotoSignMessageTab(); break;
        case 17: _t->gotoVerifyMessageTab((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 18: _t->gotoVerifyMessageTab(); break;
        case 19: _t->gotoMultisignDialog(); break;
        case 20: _t->gotoPSGTPoolPage(); break;
        case 21: _t->handlePSGTPoolChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<quint8>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        case 22: _t->optionsClicked(); break;
        case 23: _t->themeToggled(); break;
        case 24: _t->researcherClicked(); break;
        case 25: _t->aboutClicked(); break;
        case 26: _t->openConfigClicked(); break;
        case 27: _t->bxClicked(); break;
        case 28: _t->websiteClicked(); break;
        case 29: _t->exchangeClicked(); break;
        case 30: _t->boincClicked(); break;
        case 31: _t->boincStatsClicked(); break;
        case 32: _t->chatClicked(); break;
        case 33: _t->diagnosticsClicked(); break;
        case 34: _t->peersClicked(); break;
        case 35: _t->resetblockchainClicked(); break;
        case 36: _t->setPrivacy(); break;
        case 37: _t->openWikiClicked(); break;
        case 38: _t->openFaqClicked(); break;
        case 39: _t->openGuidesClicked(); break;
        case 40: { bool _r = _t->tryQuit();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 41: _t->trayIconActivated((*reinterpret_cast< std::add_pointer_t<QSystemTrayIcon::ActivationReason>>(_a[1]))); break;
        case 42: _t->incomingTransaction((*reinterpret_cast< std::add_pointer_t<QModelIndex>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        case 43: _t->encryptWallet(); break;
        case 44: _t->backupWallet(); break;
        case 45: _t->changePassphrase(); break;
        case 46: _t->unlockWallet(); break;
        case 47: _t->lockWallet(); break;
        case 48: _t->showNormalIfMinimized((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 49: _t->showNormalIfMinimized(); break;
        case 50: _t->updateStakingIcon((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[4]))); break;
        case 51: _t->updateScraperIcon((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 52: _t->updateBeaconIcon(); break;
        case 53: { QString _r = _t->GetEstimatedStakingFrequency((*reinterpret_cast< std::add_pointer_t<uint>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 54: _t->handleNewPoll(); break;
        case 55: _t->extracted((*reinterpret_cast< std::add_pointer_t<QStringList&>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString&>>(_a[2]))); break;
        case 56: _t->handleExpiredPoll(); break;
        default: ;
        }
    }
}

const QMetaObject *BitcoinGUI::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *BitcoinGUI::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10BitcoinGUIE_t>.strings))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int BitcoinGUI::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 57)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 57;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 57)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 57;
    }
    return _id;
}
namespace {
struct qt_meta_tag_ZN23ToolbarButtonIconFilterE_t {};
} // unnamed namespace

template <> constexpr inline auto ToolbarButtonIconFilter::qt_create_metaobjectdata<qt_meta_tag_ZN23ToolbarButtonIconFilterE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ToolbarButtonIconFilter"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ToolbarButtonIconFilter, qt_meta_tag_ZN23ToolbarButtonIconFilterE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ToolbarButtonIconFilter::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23ToolbarButtonIconFilterE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23ToolbarButtonIconFilterE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN23ToolbarButtonIconFilterE_t>.metaTypes,
    nullptr
} };

void ToolbarButtonIconFilter::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ToolbarButtonIconFilter *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *ToolbarButtonIconFilter::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ToolbarButtonIconFilter::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23ToolbarButtonIconFilterE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ToolbarButtonIconFilter::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    return _id;
}
QT_WARNING_POP
