/****************************************************************************
** Meta object code from reading C++ file 'sendcoinsdialog.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/qt/sendcoinsdialog.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'sendcoinsdialog.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN15SendCoinsDialogE_t {};
} // unnamed namespace

template <> constexpr inline auto SendCoinsDialog::qt_create_metaobjectdata<qt_meta_tag_ZN15SendCoinsDialogE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "SendCoinsDialog",
        "clear",
        "",
        "reject",
        "accept",
        "addEntry",
        "SendCoinsEntry*",
        "updateRemoveEnabled",
        "setBalance",
        "balance",
        "stake",
        "unconfirmedBalance",
        "immatureBalance",
        "on_sendButton_clicked",
        "removeEntry",
        "entry",
        "updateDisplayUnit",
        "toggleCoinControl",
        "coinControlFeatureChanged",
        "coinControlButtonClicked",
        "coinControlResetButtonClicked",
        "coinControlConsolidateWizardButtonClicked",
        "coinControlChangeChecked",
        "coinControlChangeEdited",
        "coinControlUpdateLabels",
        "coinControlUpdateStatus",
        "coinControlClipboardQuantity",
        "coinControlClipboardAmount",
        "coinControlClipboardFee",
        "coinControlClipboardAfterFee",
        "coinControlClipboardBytes",
        "coinControlClipboardLowOutput",
        "coinControlClipboardChange",
        "selectedConsolidationRecipient",
        "SendCoinsRecipient",
        "consolidationRecipient",
        "updateIcons",
        "updateCoinControlIcon"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'clear'
        QtMocHelpers::SlotData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'reject'
        QtMocHelpers::SlotData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'accept'
        QtMocHelpers::SlotData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'addEntry'
        QtMocHelpers::SlotData<SendCoinsEntry *()>(5, 2, QMC::AccessPublic, 0x80000000 | 6),
        // Slot 'updateRemoveEnabled'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'setBalance'
        QtMocHelpers::SlotData<void(qint64, qint64, qint64, qint64)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 9 }, { QMetaType::LongLong, 10 }, { QMetaType::LongLong, 11 }, { QMetaType::LongLong, 12 },
        }}),
        // Slot 'on_sendButton_clicked'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'removeEntry'
        QtMocHelpers::SlotData<void(SendCoinsEntry *)>(14, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 6, 15 },
        }}),
        // Slot 'updateDisplayUnit'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'toggleCoinControl'
        QtMocHelpers::SlotData<void()>(17, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'coinControlFeatureChanged'
        QtMocHelpers::SlotData<void(bool)>(18, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 2 },
        }}),
        // Slot 'coinControlButtonClicked'
        QtMocHelpers::SlotData<void()>(19, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'coinControlResetButtonClicked'
        QtMocHelpers::SlotData<void()>(20, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'coinControlConsolidateWizardButtonClicked'
        QtMocHelpers::SlotData<void()>(21, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'coinControlChangeChecked'
        QtMocHelpers::SlotData<void(int)>(22, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 2 },
        }}),
        // Slot 'coinControlChangeEdited'
        QtMocHelpers::SlotData<void(const QString &)>(23, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 2 },
        }}),
        // Slot 'coinControlUpdateLabels'
        QtMocHelpers::SlotData<void()>(24, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'coinControlUpdateStatus'
        QtMocHelpers::SlotData<void()>(25, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'coinControlClipboardQuantity'
        QtMocHelpers::SlotData<void()>(26, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'coinControlClipboardAmount'
        QtMocHelpers::SlotData<void()>(27, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'coinControlClipboardFee'
        QtMocHelpers::SlotData<void()>(28, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'coinControlClipboardAfterFee'
        QtMocHelpers::SlotData<void()>(29, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'coinControlClipboardBytes'
        QtMocHelpers::SlotData<void()>(30, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'coinControlClipboardLowOutput'
        QtMocHelpers::SlotData<void()>(31, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'coinControlClipboardChange'
        QtMocHelpers::SlotData<void()>(32, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'selectedConsolidationRecipient'
        QtMocHelpers::SlotData<void(SendCoinsRecipient)>(33, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 34, 35 },
        }}),
        // Slot 'updateIcons'
        QtMocHelpers::SlotData<void()>(36, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'updateCoinControlIcon'
        QtMocHelpers::SlotData<void()>(37, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<SendCoinsDialog, qt_meta_tag_ZN15SendCoinsDialogE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject SendCoinsDialog::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15SendCoinsDialogE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15SendCoinsDialogE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN15SendCoinsDialogE_t>.metaTypes,
    nullptr
} };

void SendCoinsDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<SendCoinsDialog *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->clear(); break;
        case 1: _t->reject(); break;
        case 2: _t->accept(); break;
        case 3: { SendCoinsEntry* _r = _t->addEntry();
            if (_a[0]) *reinterpret_cast< SendCoinsEntry**>(_a[0]) = std::move(_r); }  break;
        case 4: _t->updateRemoveEnabled(); break;
        case 5: _t->setBalance((*reinterpret_cast< std::add_pointer_t<qint64>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[4]))); break;
        case 6: _t->on_sendButton_clicked(); break;
        case 7: _t->removeEntry((*reinterpret_cast< std::add_pointer_t<SendCoinsEntry*>>(_a[1]))); break;
        case 8: _t->updateDisplayUnit(); break;
        case 9: _t->toggleCoinControl(); break;
        case 10: _t->coinControlFeatureChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 11: _t->coinControlButtonClicked(); break;
        case 12: _t->coinControlResetButtonClicked(); break;
        case 13: _t->coinControlConsolidateWizardButtonClicked(); break;
        case 14: _t->coinControlChangeChecked((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 15: _t->coinControlChangeEdited((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 16: _t->coinControlUpdateLabels(); break;
        case 17: _t->coinControlUpdateStatus(); break;
        case 18: _t->coinControlClipboardQuantity(); break;
        case 19: _t->coinControlClipboardAmount(); break;
        case 20: _t->coinControlClipboardFee(); break;
        case 21: _t->coinControlClipboardAfterFee(); break;
        case 22: _t->coinControlClipboardBytes(); break;
        case 23: _t->coinControlClipboardLowOutput(); break;
        case 24: _t->coinControlClipboardChange(); break;
        case 25: _t->selectedConsolidationRecipient((*reinterpret_cast< std::add_pointer_t<SendCoinsRecipient>>(_a[1]))); break;
        case 26: _t->updateIcons(); break;
        case 27: _t->updateCoinControlIcon(); break;
        default: ;
        }
    }
}

const QMetaObject *SendCoinsDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SendCoinsDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15SendCoinsDialogE_t>.strings))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int SendCoinsDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 28)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 28;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 28)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 28;
    }
    return _id;
}
QT_WARNING_POP
