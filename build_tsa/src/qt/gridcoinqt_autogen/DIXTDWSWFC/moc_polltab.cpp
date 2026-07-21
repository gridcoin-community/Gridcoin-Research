/****************************************************************************
** Meta object code from reading C++ file 'polltab.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/qt/voting/polltab.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'polltab.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN7PollTabE_t {};
} // unnamed namespace

template <> constexpr inline auto PollTab::qt_create_metaobjectdata<qt_meta_tag_ZN7PollTabE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "PollTab",
        "newVoteReceivedAndPollMarkedDirty",
        "",
        "changeViewMode",
        "ViewId",
        "view_id",
        "refresh",
        "filter",
        "needle",
        "sort",
        "column",
        "updateIcons",
        "theme",
        "finishRefresh",
        "showVoteRowDialog",
        "row",
        "showVoteDialog",
        "PollItem",
        "poll_item",
        "showDetailsRowDialog",
        "showDetailsDialog",
        "showPreferredDialog",
        "QModelIndex",
        "index",
        "showTableContextMenu",
        "pos"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'newVoteReceivedAndPollMarkedDirty'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'changeViewMode'
        QtMocHelpers::SlotData<void(const ViewId)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 4, 5 },
        }}),
        // Slot 'refresh'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'filter'
        QtMocHelpers::SlotData<void(const QString &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 8 },
        }}),
        // Slot 'sort'
        QtMocHelpers::SlotData<void(const int)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 10 },
        }}),
        // Slot 'updateIcons'
        QtMocHelpers::SlotData<void(const QString &)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 12 },
        }}),
        // Slot 'finishRefresh'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'showVoteRowDialog'
        QtMocHelpers::SlotData<void(int)>(14, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 15 },
        }}),
        // Slot 'showVoteDialog'
        QtMocHelpers::SlotData<void(const PollItem &)>(16, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 17, 18 },
        }}),
        // Slot 'showDetailsRowDialog'
        QtMocHelpers::SlotData<void(int)>(19, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 15 },
        }}),
        // Slot 'showDetailsDialog'
        QtMocHelpers::SlotData<void(const PollItem &)>(20, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 17, 18 },
        }}),
        // Slot 'showPreferredDialog'
        QtMocHelpers::SlotData<void(const QModelIndex &)>(21, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 22, 23 },
        }}),
        // Slot 'showTableContextMenu'
        QtMocHelpers::SlotData<void(const QPoint &)>(24, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QPoint, 25 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<PollTab, qt_meta_tag_ZN7PollTabE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject PollTab::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7PollTabE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7PollTabE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN7PollTabE_t>.metaTypes,
    nullptr
} };

void PollTab::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<PollTab *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->newVoteReceivedAndPollMarkedDirty(); break;
        case 1: _t->changeViewMode((*reinterpret_cast< std::add_pointer_t<ViewId>>(_a[1]))); break;
        case 2: _t->refresh(); break;
        case 3: _t->filter((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->sort((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 5: _t->updateIcons((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->finishRefresh(); break;
        case 7: _t->showVoteRowDialog((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 8: _t->showVoteDialog((*reinterpret_cast< std::add_pointer_t<PollItem>>(_a[1]))); break;
        case 9: _t->showDetailsRowDialog((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 10: _t->showDetailsDialog((*reinterpret_cast< std::add_pointer_t<PollItem>>(_a[1]))); break;
        case 11: _t->showPreferredDialog((*reinterpret_cast< std::add_pointer_t<QModelIndex>>(_a[1]))); break;
        case 12: _t->showTableContextMenu((*reinterpret_cast< std::add_pointer_t<QPoint>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (PollTab::*)()>(_a, &PollTab::newVoteReceivedAndPollMarkedDirty, 0))
            return;
    }
}

const QMetaObject *PollTab::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *PollTab::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7PollTabE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int PollTab::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 13)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 13;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 13)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 13;
    }
    return _id;
}

// SIGNAL 0
void PollTab::newVoteReceivedAndPollMarkedDirty()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
