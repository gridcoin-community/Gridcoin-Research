/****************************************************************************
** Meta object code from reading C++ file 'rpcconsole.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/qt/rpcconsole.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'rpcconsole.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN10RPCConsoleE_t {};
} // unnamed namespace

template <> constexpr inline auto RPCConsole::qt_create_metaobjectdata<qt_meta_tag_ZN10RPCConsoleE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "RPCConsole",
        "stopExecutor",
        "",
        "cmdRequest",
        "command",
        "on_lineEdit_returnPressed",
        "on_tabWidget_currentChanged",
        "index",
        "on_openDebugLogfileButton_clicked",
        "on_showCLOptionsButton_clicked",
        "on_graphRangeSlider_valueChanged",
        "value",
        "updateTrafficStats",
        "totalBytesIn",
        "totalBytesOut",
        "resizeEvent",
        "QResizeEvent*",
        "event",
        "showEvent",
        "QShowEvent*",
        "hideEvent",
        "QHideEvent*",
        "on_clearTrafficGraphButton_clicked",
        "showPeersTableContextMenu",
        "point",
        "showBanTableContextMenu",
        "showOrHideBanTableIfRequired",
        "clearSelectedNode",
        "clear",
        "message",
        "category",
        "html",
        "setNumConnections",
        "count",
        "setNumBlocks",
        "countOfPeers",
        "displayScraperLogMessage",
        "string",
        "browseHistory",
        "offset",
        "scrollToEnd",
        "peerSelected",
        "QItemSelection",
        "selected",
        "deselected",
        "peerLayoutAboutToChange",
        "peerLayoutChanged",
        "disconnectSelectedNode",
        "banSelectedNode",
        "bantime",
        "unbanSelectedNode",
        "showPeersTab"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'stopExecutor'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'cmdRequest'
        QtMocHelpers::SignalData<void(const QString &)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 4 },
        }}),
        // Slot 'on_lineEdit_returnPressed'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_tabWidget_currentChanged'
        QtMocHelpers::SlotData<void(int)>(6, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 7 },
        }}),
        // Slot 'on_openDebugLogfileButton_clicked'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_showCLOptionsButton_clicked'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_graphRangeSlider_valueChanged'
        QtMocHelpers::SlotData<void(int)>(10, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 11 },
        }}),
        // Slot 'updateTrafficStats'
        QtMocHelpers::SlotData<void(quint64, quint64)>(12, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::ULongLong, 13 }, { QMetaType::ULongLong, 14 },
        }}),
        // Slot 'resizeEvent'
        QtMocHelpers::SlotData<void(QResizeEvent *)>(15, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 16, 17 },
        }}),
        // Slot 'showEvent'
        QtMocHelpers::SlotData<void(QShowEvent *)>(18, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 19, 17 },
        }}),
        // Slot 'hideEvent'
        QtMocHelpers::SlotData<void(QHideEvent *)>(20, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 21, 17 },
        }}),
        // Slot 'on_clearTrafficGraphButton_clicked'
        QtMocHelpers::SlotData<void()>(22, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'showPeersTableContextMenu'
        QtMocHelpers::SlotData<void(const QPoint &)>(23, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QPoint, 24 },
        }}),
        // Slot 'showBanTableContextMenu'
        QtMocHelpers::SlotData<void(const QPoint &)>(25, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QPoint, 24 },
        }}),
        // Slot 'showOrHideBanTableIfRequired'
        QtMocHelpers::SlotData<void()>(26, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'clearSelectedNode'
        QtMocHelpers::SlotData<void()>(27, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'clear'
        QtMocHelpers::SlotData<void()>(28, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'message'
        QtMocHelpers::SlotData<void(int, const QString &, bool)>(29, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 30 }, { QMetaType::QString, 29 }, { QMetaType::Bool, 31 },
        }}),
        // Slot 'message'
        QtMocHelpers::SlotData<void(int, const QString &)>(29, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::Int, 30 }, { QMetaType::QString, 29 },
        }}),
        // Slot 'setNumConnections'
        QtMocHelpers::SlotData<void(int)>(32, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 33 },
        }}),
        // Slot 'setNumBlocks'
        QtMocHelpers::SlotData<void(int, int)>(34, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 33 }, { QMetaType::Int, 35 },
        }}),
        // Slot 'displayScraperLogMessage'
        QtMocHelpers::SlotData<void(const QString &)>(36, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 37 },
        }}),
        // Slot 'browseHistory'
        QtMocHelpers::SlotData<void(int)>(38, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 39 },
        }}),
        // Slot 'scrollToEnd'
        QtMocHelpers::SlotData<void()>(40, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'peerSelected'
        QtMocHelpers::SlotData<void(const QItemSelection &, const QItemSelection &)>(41, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 42, 43 }, { 0x80000000 | 42, 44 },
        }}),
        // Slot 'peerLayoutAboutToChange'
        QtMocHelpers::SlotData<void()>(45, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'peerLayoutChanged'
        QtMocHelpers::SlotData<void()>(46, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'disconnectSelectedNode'
        QtMocHelpers::SlotData<void()>(47, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'banSelectedNode'
        QtMocHelpers::SlotData<void(int)>(48, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 49 },
        }}),
        // Slot 'unbanSelectedNode'
        QtMocHelpers::SlotData<void()>(50, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'showPeersTab'
        QtMocHelpers::SlotData<void()>(51, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<RPCConsole, qt_meta_tag_ZN10RPCConsoleE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject RPCConsole::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10RPCConsoleE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10RPCConsoleE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN10RPCConsoleE_t>.metaTypes,
    nullptr
} };

void RPCConsole::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<RPCConsole *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->stopExecutor(); break;
        case 1: _t->cmdRequest((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->on_lineEdit_returnPressed(); break;
        case 3: _t->on_tabWidget_currentChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 4: _t->on_openDebugLogfileButton_clicked(); break;
        case 5: _t->on_showCLOptionsButton_clicked(); break;
        case 6: _t->on_graphRangeSlider_valueChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 7: _t->updateTrafficStats((*reinterpret_cast< std::add_pointer_t<quint64>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<quint64>>(_a[2]))); break;
        case 8: _t->resizeEvent((*reinterpret_cast< std::add_pointer_t<QResizeEvent*>>(_a[1]))); break;
        case 9: _t->showEvent((*reinterpret_cast< std::add_pointer_t<QShowEvent*>>(_a[1]))); break;
        case 10: _t->hideEvent((*reinterpret_cast< std::add_pointer_t<QHideEvent*>>(_a[1]))); break;
        case 11: _t->on_clearTrafficGraphButton_clicked(); break;
        case 12: _t->showPeersTableContextMenu((*reinterpret_cast< std::add_pointer_t<QPoint>>(_a[1]))); break;
        case 13: _t->showBanTableContextMenu((*reinterpret_cast< std::add_pointer_t<QPoint>>(_a[1]))); break;
        case 14: _t->showOrHideBanTableIfRequired(); break;
        case 15: _t->clearSelectedNode(); break;
        case 16: _t->clear(); break;
        case 17: _t->message((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[3]))); break;
        case 18: _t->message((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 19: _t->setNumConnections((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 20: _t->setNumBlocks((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 21: _t->displayScraperLogMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 22: _t->browseHistory((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 23: _t->scrollToEnd(); break;
        case 24: _t->peerSelected((*reinterpret_cast< std::add_pointer_t<QItemSelection>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QItemSelection>>(_a[2]))); break;
        case 25: _t->peerLayoutAboutToChange(); break;
        case 26: _t->peerLayoutChanged(); break;
        case 27: _t->disconnectSelectedNode(); break;
        case 28: _t->banSelectedNode((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 29: _t->unbanSelectedNode(); break;
        case 30: _t->showPeersTab(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (RPCConsole::*)()>(_a, &RPCConsole::stopExecutor, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (RPCConsole::*)(const QString & )>(_a, &RPCConsole::cmdRequest, 1))
            return;
    }
}

const QMetaObject *RPCConsole::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *RPCConsole::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10RPCConsoleE_t>.strings))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int RPCConsole::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 31)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 31;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 31)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 31;
    }
    return _id;
}

// SIGNAL 0
void RPCConsole::stopExecutor()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void RPCConsole::cmdRequest(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}
QT_WARNING_POP
