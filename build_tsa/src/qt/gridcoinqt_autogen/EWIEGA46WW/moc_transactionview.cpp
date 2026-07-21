/****************************************************************************
** Meta object code from reading C++ file 'transactionview.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/qt/transactionview.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'transactionview.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN15TransactionViewE_t {};
} // unnamed namespace

template <> constexpr inline auto TransactionView::qt_create_metaobjectdata<qt_meta_tag_ZN15TransactionViewE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "TransactionView",
        "doubleClicked",
        "",
        "QModelIndex",
        "contextualMenu",
        "dateRangeChanged",
        "copyAddress",
        "editLabel",
        "copyLabel",
        "copyAmount",
        "copyTxID",
        "updateIcons",
        "theme",
        "txnViewSectionResized",
        "index",
        "old_size",
        "new_size",
        "reportViewport",
        "captureAnchor",
        "restoreAnchor",
        "showDetails",
        "chooseDate",
        "idx",
        "chooseType",
        "changedPrefix",
        "prefix",
        "changedAmount",
        "amount",
        "exportClicked",
        "focusTransaction",
        "resizeTableColumns",
        "neighbor_pair_adjust"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'doubleClicked'
        QtMocHelpers::SignalData<void(const QModelIndex &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 2 },
        }}),
        // Slot 'contextualMenu'
        QtMocHelpers::SlotData<void(const QPoint &)>(4, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QPoint, 2 },
        }}),
        // Slot 'dateRangeChanged'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'copyAddress'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'editLabel'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'copyLabel'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'copyAmount'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'copyTxID'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'updateIcons'
        QtMocHelpers::SlotData<void(const QString &)>(11, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 12 },
        }}),
        // Slot 'txnViewSectionResized'
        QtMocHelpers::SlotData<void(int, int, int)>(13, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 14 }, { QMetaType::Int, 15 }, { QMetaType::Int, 16 },
        }}),
        // Slot 'reportViewport'
        QtMocHelpers::SlotData<void()>(17, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'captureAnchor'
        QtMocHelpers::SlotData<void()>(18, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'restoreAnchor'
        QtMocHelpers::SlotData<void()>(19, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'showDetails'
        QtMocHelpers::SlotData<void()>(20, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'chooseDate'
        QtMocHelpers::SlotData<void(int)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 22 },
        }}),
        // Slot 'chooseType'
        QtMocHelpers::SlotData<void(int)>(23, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 22 },
        }}),
        // Slot 'changedPrefix'
        QtMocHelpers::SlotData<void(const QString &)>(24, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 25 },
        }}),
        // Slot 'changedAmount'
        QtMocHelpers::SlotData<void(const QString &)>(26, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 27 },
        }}),
        // Slot 'exportClicked'
        QtMocHelpers::SlotData<void()>(28, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'focusTransaction'
        QtMocHelpers::SlotData<void(const QModelIndex &)>(29, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 2 },
        }}),
        // Slot 'resizeTableColumns'
        QtMocHelpers::SlotData<void(const bool &, const int &, const int &, const int &)>(30, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 31 }, { QMetaType::Int, 14 }, { QMetaType::Int, 15 }, { QMetaType::Int, 16 },
        }}),
        // Slot 'resizeTableColumns'
        QtMocHelpers::SlotData<void(const bool &, const int &, const int &)>(30, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::Bool, 31 }, { QMetaType::Int, 14 }, { QMetaType::Int, 15 },
        }}),
        // Slot 'resizeTableColumns'
        QtMocHelpers::SlotData<void(const bool &, const int &)>(30, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::Bool, 31 }, { QMetaType::Int, 14 },
        }}),
        // Slot 'resizeTableColumns'
        QtMocHelpers::SlotData<void(const bool &)>(30, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::Bool, 31 },
        }}),
        // Slot 'resizeTableColumns'
        QtMocHelpers::SlotData<void()>(30, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<TransactionView, qt_meta_tag_ZN15TransactionViewE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject TransactionView::staticMetaObject = { {
    QMetaObject::SuperData::link<QFrame::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15TransactionViewE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15TransactionViewE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN15TransactionViewE_t>.metaTypes,
    nullptr
} };

void TransactionView::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<TransactionView *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->doubleClicked((*reinterpret_cast< std::add_pointer_t<QModelIndex>>(_a[1]))); break;
        case 1: _t->contextualMenu((*reinterpret_cast< std::add_pointer_t<QPoint>>(_a[1]))); break;
        case 2: _t->dateRangeChanged(); break;
        case 3: _t->copyAddress(); break;
        case 4: _t->editLabel(); break;
        case 5: _t->copyLabel(); break;
        case 6: _t->copyAmount(); break;
        case 7: _t->copyTxID(); break;
        case 8: _t->updateIcons((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 9: _t->txnViewSectionResized((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        case 10: _t->reportViewport(); break;
        case 11: _t->captureAnchor(); break;
        case 12: _t->restoreAnchor(); break;
        case 13: _t->showDetails(); break;
        case 14: _t->chooseDate((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 15: _t->chooseType((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 16: _t->changedPrefix((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 17: _t->changedAmount((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 18: _t->exportClicked(); break;
        case 19: _t->focusTransaction((*reinterpret_cast< std::add_pointer_t<QModelIndex>>(_a[1]))); break;
        case 20: _t->resizeTableColumns((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4]))); break;
        case 21: _t->resizeTableColumns((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        case 22: _t->resizeTableColumns((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 23: _t->resizeTableColumns((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 24: _t->resizeTableColumns(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (TransactionView::*)(const QModelIndex & )>(_a, &TransactionView::doubleClicked, 0))
            return;
    }
}

const QMetaObject *TransactionView::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *TransactionView::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15TransactionViewE_t>.strings))
        return static_cast<void*>(this);
    return QFrame::qt_metacast(_clname);
}

int TransactionView::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QFrame::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 25)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 25;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 25)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 25;
    }
    return _id;
}

// SIGNAL 0
void TransactionView::doubleClicked(const QModelIndex & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}
QT_WARNING_POP
