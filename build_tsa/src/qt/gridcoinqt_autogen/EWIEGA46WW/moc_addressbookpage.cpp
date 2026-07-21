/****************************************************************************
** Meta object code from reading C++ file 'addressbookpage.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/qt/addressbookpage.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'addressbookpage.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN15AddressBookPageE_t {};
} // unnamed namespace

template <> constexpr inline auto AddressBookPage::qt_create_metaobjectdata<qt_meta_tag_ZN15AddressBookPageE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "AddressBookPage",
        "signMessage",
        "",
        "addr",
        "verifyMessage",
        "done",
        "retval",
        "exportClicked",
        "changeFilter",
        "needle",
        "resizeTableColumns",
        "neighbor_pair_adjust",
        "index",
        "old_size",
        "new_size",
        "on_deleteButton_clicked",
        "on_newAddressButton_clicked",
        "on_addExistingButton_clicked",
        "on_copyToClipboardButton_clicked",
        "on_signMessageButton_clicked",
        "on_verifyMessageButton_clicked",
        "selectionChanged",
        "on_showQRCodeButton_clicked",
        "contextualMenu",
        "point",
        "onCopyLabelAction",
        "onEditAction",
        "selectNewAddress",
        "QModelIndex",
        "parent",
        "begin",
        "end",
        "addressBookSectionResized"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'signMessage'
        QtMocHelpers::SignalData<void(QString)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'verifyMessage'
        QtMocHelpers::SignalData<void(QString)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Slot 'done'
        QtMocHelpers::SlotData<void(int)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 6 },
        }}),
        // Slot 'exportClicked'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'changeFilter'
        QtMocHelpers::SlotData<void(const QString &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 },
        }}),
        // Slot 'resizeTableColumns'
        QtMocHelpers::SlotData<void(const bool &, const int &, const int &, const int &)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 11 }, { QMetaType::Int, 12 }, { QMetaType::Int, 13 }, { QMetaType::Int, 14 },
        }}),
        // Slot 'resizeTableColumns'
        QtMocHelpers::SlotData<void(const bool &, const int &, const int &)>(10, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::Bool, 11 }, { QMetaType::Int, 12 }, { QMetaType::Int, 13 },
        }}),
        // Slot 'resizeTableColumns'
        QtMocHelpers::SlotData<void(const bool &, const int &)>(10, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::Bool, 11 }, { QMetaType::Int, 12 },
        }}),
        // Slot 'resizeTableColumns'
        QtMocHelpers::SlotData<void(const bool &)>(10, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::Bool, 11 },
        }}),
        // Slot 'resizeTableColumns'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void),
        // Slot 'on_deleteButton_clicked'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_newAddressButton_clicked'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_addExistingButton_clicked'
        QtMocHelpers::SlotData<void()>(17, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_copyToClipboardButton_clicked'
        QtMocHelpers::SlotData<void()>(18, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_signMessageButton_clicked'
        QtMocHelpers::SlotData<void()>(19, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_verifyMessageButton_clicked'
        QtMocHelpers::SlotData<void()>(20, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'selectionChanged'
        QtMocHelpers::SlotData<void()>(21, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_showQRCodeButton_clicked'
        QtMocHelpers::SlotData<void()>(22, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'contextualMenu'
        QtMocHelpers::SlotData<void(const QPoint &)>(23, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QPoint, 24 },
        }}),
        // Slot 'onCopyLabelAction'
        QtMocHelpers::SlotData<void()>(25, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onEditAction'
        QtMocHelpers::SlotData<void()>(26, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'selectNewAddress'
        QtMocHelpers::SlotData<void(const QModelIndex &, int, int)>(27, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 28, 29 }, { QMetaType::Int, 30 }, { QMetaType::Int, 31 },
        }}),
        // Slot 'addressBookSectionResized'
        QtMocHelpers::SlotData<void(int, int, int)>(32, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 12 }, { QMetaType::Int, 13 }, { QMetaType::Int, 14 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<AddressBookPage, qt_meta_tag_ZN15AddressBookPageE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject AddressBookPage::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15AddressBookPageE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15AddressBookPageE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN15AddressBookPageE_t>.metaTypes,
    nullptr
} };

void AddressBookPage::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<AddressBookPage *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->signMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->verifyMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->done((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->exportClicked(); break;
        case 4: _t->changeFilter((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->resizeTableColumns((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4]))); break;
        case 6: _t->resizeTableColumns((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        case 7: _t->resizeTableColumns((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 8: _t->resizeTableColumns((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 9: _t->resizeTableColumns(); break;
        case 10: _t->on_deleteButton_clicked(); break;
        case 11: _t->on_newAddressButton_clicked(); break;
        case 12: _t->on_addExistingButton_clicked(); break;
        case 13: _t->on_copyToClipboardButton_clicked(); break;
        case 14: _t->on_signMessageButton_clicked(); break;
        case 15: _t->on_verifyMessageButton_clicked(); break;
        case 16: _t->selectionChanged(); break;
        case 17: _t->on_showQRCodeButton_clicked(); break;
        case 18: _t->contextualMenu((*reinterpret_cast< std::add_pointer_t<QPoint>>(_a[1]))); break;
        case 19: _t->onCopyLabelAction(); break;
        case 20: _t->onEditAction(); break;
        case 21: _t->selectNewAddress((*reinterpret_cast< std::add_pointer_t<QModelIndex>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        case 22: _t->addressBookSectionResized((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (AddressBookPage::*)(QString )>(_a, &AddressBookPage::signMessage, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (AddressBookPage::*)(QString )>(_a, &AddressBookPage::verifyMessage, 1))
            return;
    }
}

const QMetaObject *AddressBookPage::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AddressBookPage::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15AddressBookPageE_t>.strings))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int AddressBookPage::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 23)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 23;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 23)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 23;
    }
    return _id;
}

// SIGNAL 0
void AddressBookPage::signMessage(QString _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void AddressBookPage::verifyMessage(QString _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}
QT_WARNING_POP
