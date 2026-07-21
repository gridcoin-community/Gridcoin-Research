/****************************************************************************
** Meta object code from reading C++ file 'signverifymessagedialog.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/qt/signverifymessagedialog.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'signverifymessagedialog.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN23SignVerifyMessageDialogE_t {};
} // unnamed namespace

template <> constexpr inline auto SignVerifyMessageDialog::qt_create_metaobjectdata<qt_meta_tag_ZN23SignVerifyMessageDialogE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "SignVerifyMessageDialog",
        "on_addressBookButton_SM_clicked",
        "",
        "on_pasteButton_SM_clicked",
        "on_signMessageButton_SM_clicked",
        "on_copySignatureButton_SM_clicked",
        "on_clearButton_SM_clicked",
        "on_addressBookButton_VM_clicked",
        "on_verifyMessageButton_VM_clicked",
        "on_clearButton_VM_clicked"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'on_addressBookButton_SM_clicked'
        QtMocHelpers::SlotData<void()>(1, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_pasteButton_SM_clicked'
        QtMocHelpers::SlotData<void()>(3, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_signMessageButton_SM_clicked'
        QtMocHelpers::SlotData<void()>(4, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_copySignatureButton_SM_clicked'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_clearButton_SM_clicked'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_addressBookButton_VM_clicked'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_verifyMessageButton_VM_clicked'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_clearButton_VM_clicked'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<SignVerifyMessageDialog, qt_meta_tag_ZN23SignVerifyMessageDialogE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject SignVerifyMessageDialog::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23SignVerifyMessageDialogE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23SignVerifyMessageDialogE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN23SignVerifyMessageDialogE_t>.metaTypes,
    nullptr
} };

void SignVerifyMessageDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<SignVerifyMessageDialog *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->on_addressBookButton_SM_clicked(); break;
        case 1: _t->on_pasteButton_SM_clicked(); break;
        case 2: _t->on_signMessageButton_SM_clicked(); break;
        case 3: _t->on_copySignatureButton_SM_clicked(); break;
        case 4: _t->on_clearButton_SM_clicked(); break;
        case 5: _t->on_addressBookButton_VM_clicked(); break;
        case 6: _t->on_verifyMessageButton_VM_clicked(); break;
        case 7: _t->on_clearButton_VM_clicked(); break;
        default: ;
        }
    }
    (void)_a;
}

const QMetaObject *SignVerifyMessageDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SignVerifyMessageDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23SignVerifyMessageDialogE_t>.strings))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int SignVerifyMessageDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 8)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 8;
    }
    return _id;
}
QT_WARNING_POP
