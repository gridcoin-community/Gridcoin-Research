/****************************************************************************
** Meta object code from reading C++ file 'researcherwizardsummarypage.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/qt/researcher/researcherwizardsummarypage.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'researcherwizardsummarypage.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN27ResearcherWizardSummaryPageE_t {};
} // unnamed namespace

template <> constexpr inline auto ResearcherWizardSummaryPage::qt_create_metaobjectdata<qt_meta_tag_ZN27ResearcherWizardSummaryPageE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ResearcherWizardSummaryPage",
        "reviewBeaconAuthButtonClicked",
        "",
        "renewBeaconButtonClicked",
        "onTabChanged",
        "index",
        "refreshSummary",
        "refreshOverallStatus",
        "refreshProjects",
        "reloadProjects",
        "on_reviewBeaconAuthButton_clicked",
        "on_renewBeaconButton_clicked"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'reviewBeaconAuthButtonClicked'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'renewBeaconButtonClicked'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onTabChanged'
        QtMocHelpers::SlotData<void(int)>(4, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 5 },
        }}),
        // Slot 'refreshSummary'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'refreshOverallStatus'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'refreshProjects'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'reloadProjects'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_reviewBeaconAuthButton_clicked'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_renewBeaconButton_clicked'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ResearcherWizardSummaryPage, qt_meta_tag_ZN27ResearcherWizardSummaryPageE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ResearcherWizardSummaryPage::staticMetaObject = { {
    QMetaObject::SuperData::link<QWizardPage::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN27ResearcherWizardSummaryPageE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN27ResearcherWizardSummaryPageE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN27ResearcherWizardSummaryPageE_t>.metaTypes,
    nullptr
} };

void ResearcherWizardSummaryPage::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ResearcherWizardSummaryPage *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->reviewBeaconAuthButtonClicked(); break;
        case 1: _t->renewBeaconButtonClicked(); break;
        case 2: _t->onTabChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->refreshSummary(); break;
        case 4: _t->refreshOverallStatus(); break;
        case 5: _t->refreshProjects(); break;
        case 6: _t->reloadProjects(); break;
        case 7: _t->on_reviewBeaconAuthButton_clicked(); break;
        case 8: _t->on_renewBeaconButton_clicked(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ResearcherWizardSummaryPage::*)()>(_a, &ResearcherWizardSummaryPage::reviewBeaconAuthButtonClicked, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ResearcherWizardSummaryPage::*)()>(_a, &ResearcherWizardSummaryPage::renewBeaconButtonClicked, 1))
            return;
    }
}

const QMetaObject *ResearcherWizardSummaryPage::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ResearcherWizardSummaryPage::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN27ResearcherWizardSummaryPageE_t>.strings))
        return static_cast<void*>(this);
    return QWizardPage::qt_metacast(_clname);
}

int ResearcherWizardSummaryPage::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWizardPage::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 9)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 9;
    }
    return _id;
}

// SIGNAL 0
void ResearcherWizardSummaryPage::reviewBeaconAuthButtonClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ResearcherWizardSummaryPage::renewBeaconButtonClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
QT_WARNING_POP
