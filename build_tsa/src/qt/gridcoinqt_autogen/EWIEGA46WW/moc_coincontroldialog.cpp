/****************************************************************************
** Meta object code from reading C++ file 'coincontroldialog.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/qt/coincontroldialog.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'coincontroldialog.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN17CoinControlDialogE_t {};
} // unnamed namespace

template <> constexpr inline auto CoinControlDialog::qt_create_metaobjectdata<qt_meta_tag_ZN17CoinControlDialogE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "CoinControlDialog",
        "selectedConsolidationRecipientSignal",
        "",
        "SendCoinsRecipient",
        "consolidationRecipient",
        "filterInputsByValue",
        "less",
        "CAmount",
        "inputFilterValue",
        "inputSelectionLimit",
        "showMenu",
        "copyAmount",
        "copyLabel",
        "copyAddress",
        "copyTransactionHash",
        "clipboardQuantity",
        "clipboardAmount",
        "clipboardFee",
        "clipboardAfterFee",
        "clipboardBytes",
        "clipboardLowOutput",
        "clipboardChange",
        "treeModeRadioButton",
        "listModeRadioButton",
        "viewItemChanged",
        "QTreeWidgetItem*",
        "headerSectionClicked",
        "buttonBoxClicked",
        "QAbstractButton*",
        "buttonSelectAllClicked",
        "maxMinOutputValueChanged",
        "buttonFilterModeClicked",
        "buttonFilterClicked",
        "buttonConsolidateClicked",
        "selectedConsolidationAddressSlot",
        "std::pair<QString,QString>",
        "address"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'selectedConsolidationRecipientSignal'
        QtMocHelpers::SignalData<void(SendCoinsRecipient)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Slot 'filterInputsByValue'
        QtMocHelpers::SlotData<bool(const bool &, const CAmount &, const unsigned int &)>(5, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Bool, 6 }, { 0x80000000 | 7, 8 }, { QMetaType::UInt, 9 },
        }}),
        // Slot 'showMenu'
        QtMocHelpers::SlotData<void(const QPoint &)>(10, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QPoint, 2 },
        }}),
        // Slot 'copyAmount'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'copyLabel'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'copyAddress'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'copyTransactionHash'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'clipboardQuantity'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'clipboardAmount'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'clipboardFee'
        QtMocHelpers::SlotData<void()>(17, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'clipboardAfterFee'
        QtMocHelpers::SlotData<void()>(18, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'clipboardBytes'
        QtMocHelpers::SlotData<void()>(19, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'clipboardLowOutput'
        QtMocHelpers::SlotData<void()>(20, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'clipboardChange'
        QtMocHelpers::SlotData<void()>(21, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'treeModeRadioButton'
        QtMocHelpers::SlotData<void(bool)>(22, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 2 },
        }}),
        // Slot 'listModeRadioButton'
        QtMocHelpers::SlotData<void(bool)>(23, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 2 },
        }}),
        // Slot 'viewItemChanged'
        QtMocHelpers::SlotData<void(QTreeWidgetItem *, int)>(24, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 25, 2 }, { QMetaType::Int, 2 },
        }}),
        // Slot 'headerSectionClicked'
        QtMocHelpers::SlotData<void(int)>(26, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 2 },
        }}),
        // Slot 'buttonBoxClicked'
        QtMocHelpers::SlotData<void(QAbstractButton *)>(27, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 28, 2 },
        }}),
        // Slot 'buttonSelectAllClicked'
        QtMocHelpers::SlotData<void()>(29, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'maxMinOutputValueChanged'
        QtMocHelpers::SlotData<void()>(30, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'buttonFilterModeClicked'
        QtMocHelpers::SlotData<void()>(31, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'buttonFilterClicked'
        QtMocHelpers::SlotData<void()>(32, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'buttonConsolidateClicked'
        QtMocHelpers::SlotData<void()>(33, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'selectedConsolidationAddressSlot'
        QtMocHelpers::SlotData<void(std::pair<QString,QString>)>(34, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 35, 36 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<CoinControlDialog, qt_meta_tag_ZN17CoinControlDialogE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject CoinControlDialog::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17CoinControlDialogE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17CoinControlDialogE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN17CoinControlDialogE_t>.metaTypes,
    nullptr
} };

void CoinControlDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<CoinControlDialog *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->selectedConsolidationRecipientSignal((*reinterpret_cast< std::add_pointer_t<SendCoinsRecipient>>(_a[1]))); break;
        case 1: { bool _r = _t->filterInputsByValue((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<CAmount>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<uint>>(_a[3])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 2: _t->showMenu((*reinterpret_cast< std::add_pointer_t<QPoint>>(_a[1]))); break;
        case 3: _t->copyAmount(); break;
        case 4: _t->copyLabel(); break;
        case 5: _t->copyAddress(); break;
        case 6: _t->copyTransactionHash(); break;
        case 7: _t->clipboardQuantity(); break;
        case 8: _t->clipboardAmount(); break;
        case 9: _t->clipboardFee(); break;
        case 10: _t->clipboardAfterFee(); break;
        case 11: _t->clipboardBytes(); break;
        case 12: _t->clipboardLowOutput(); break;
        case 13: _t->clipboardChange(); break;
        case 14: _t->treeModeRadioButton((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 15: _t->listModeRadioButton((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 16: _t->viewItemChanged((*reinterpret_cast< std::add_pointer_t<QTreeWidgetItem*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 17: _t->headerSectionClicked((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 18: _t->buttonBoxClicked((*reinterpret_cast< std::add_pointer_t<QAbstractButton*>>(_a[1]))); break;
        case 19: _t->buttonSelectAllClicked(); break;
        case 20: _t->maxMinOutputValueChanged(); break;
        case 21: _t->buttonFilterModeClicked(); break;
        case 22: _t->buttonFilterClicked(); break;
        case 23: _t->buttonConsolidateClicked(); break;
        case 24: _t->selectedConsolidationAddressSlot((*reinterpret_cast< std::add_pointer_t<std::pair<QString,QString>>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 18:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QAbstractButton* >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (CoinControlDialog::*)(SendCoinsRecipient )>(_a, &CoinControlDialog::selectedConsolidationRecipientSignal, 0))
            return;
    }
}

const QMetaObject *CoinControlDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *CoinControlDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17CoinControlDialogE_t>.strings))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int CoinControlDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 25)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 25;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 25)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 25;
    }
    return _id;
}

// SIGNAL 0
void CoinControlDialog::selectedConsolidationRecipientSignal(SendCoinsRecipient _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}
QT_WARNING_POP
