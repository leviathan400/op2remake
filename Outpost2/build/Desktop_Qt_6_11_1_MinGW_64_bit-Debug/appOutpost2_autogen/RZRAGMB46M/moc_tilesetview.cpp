/****************************************************************************
** Meta object code from reading C++ file 'tilesetview.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../render/tilesetview.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'tilesetview.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.1. It"
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
struct qt_meta_tag_ZN11TilesetViewE_t {};
} // unnamed namespace

template <> constexpr inline auto TilesetView::qt_create_metaobjectdata<qt_meta_tag_ZN11TilesetViewE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "TilesetView",
        "QML.Element",
        "auto",
        "hoverChanged",
        "",
        "edgeScrollOutsideChanged",
        "scrollSpeedChanged",
        "zoomEnabledChanged",
        "shadowsEnabledChanged",
        "gridEnabledChanged",
        "labelsEnabledChanged",
        "cameraChanged",
        "advanceAnimation",
        "exportViewport",
        "zoomAt",
        "deltaY",
        "x",
        "y",
        "hoverTileX",
        "hoverTileY",
        "edgeScrollOutside",
        "scrollSpeed",
        "zoomEnabled",
        "shadowsEnabled",
        "gridEnabled",
        "labelsEnabled"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'hoverChanged'
        QtMocHelpers::SignalData<void()>(3, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'edgeScrollOutsideChanged'
        QtMocHelpers::SignalData<void()>(5, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'scrollSpeedChanged'
        QtMocHelpers::SignalData<void()>(6, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'zoomEnabledChanged'
        QtMocHelpers::SignalData<void()>(7, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'shadowsEnabledChanged'
        QtMocHelpers::SignalData<void()>(8, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'gridEnabledChanged'
        QtMocHelpers::SignalData<void()>(9, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'labelsEnabledChanged'
        QtMocHelpers::SignalData<void()>(10, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'cameraChanged'
        QtMocHelpers::SignalData<void()>(11, 4, QMC::AccessPublic, QMetaType::Void),
        // Method 'advanceAnimation'
        QtMocHelpers::MethodData<void()>(12, 4, QMC::AccessPublic, QMetaType::Void),
        // Method 'exportViewport'
        QtMocHelpers::MethodData<QString()>(13, 4, QMC::AccessPublic, QMetaType::QString),
        // Method 'zoomAt'
        QtMocHelpers::MethodData<void(qreal, qreal, qreal)>(14, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QReal, 15 }, { QMetaType::QReal, 16 }, { QMetaType::QReal, 17 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'hoverTileX'
        QtMocHelpers::PropertyData<int>(18, QMetaType::Int, QMC::DefaultPropertyFlags, 0),
        // property 'hoverTileY'
        QtMocHelpers::PropertyData<int>(19, QMetaType::Int, QMC::DefaultPropertyFlags, 0),
        // property 'edgeScrollOutside'
        QtMocHelpers::PropertyData<bool>(20, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 1),
        // property 'scrollSpeed'
        QtMocHelpers::PropertyData<int>(21, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 2),
        // property 'zoomEnabled'
        QtMocHelpers::PropertyData<bool>(22, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 3),
        // property 'shadowsEnabled'
        QtMocHelpers::PropertyData<bool>(23, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 4),
        // property 'gridEnabled'
        QtMocHelpers::PropertyData<bool>(24, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 5),
        // property 'labelsEnabled'
        QtMocHelpers::PropertyData<bool>(25, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 6),
    };
    QtMocHelpers::UintData qt_enums {
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
    });
    return QtMocHelpers::metaObjectData<TilesetView, void>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject TilesetView::staticMetaObject = { {
    QMetaObject::SuperData::link<QQuickPaintedItem::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11TilesetViewE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11TilesetViewE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN11TilesetViewE_t>.metaTypes,
    nullptr
} };

void TilesetView::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<TilesetView *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->hoverChanged(); break;
        case 1: _t->edgeScrollOutsideChanged(); break;
        case 2: _t->scrollSpeedChanged(); break;
        case 3: _t->zoomEnabledChanged(); break;
        case 4: _t->shadowsEnabledChanged(); break;
        case 5: _t->gridEnabledChanged(); break;
        case 6: _t->labelsEnabledChanged(); break;
        case 7: _t->cameraChanged(); break;
        case 8: _t->advanceAnimation(); break;
        case 9: { QString _r = _t->exportViewport();
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 10: _t->zoomAt((*reinterpret_cast<std::add_pointer_t<qreal>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<qreal>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<qreal>>(_a[3]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::hoverChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::edgeScrollOutsideChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::scrollSpeedChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::zoomEnabledChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::shadowsEnabledChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::gridEnabledChanged, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::labelsEnabledChanged, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::cameraChanged, 7))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<int*>(_v) = _t->hoverTileX(); break;
        case 1: *reinterpret_cast<int*>(_v) = _t->hoverTileY(); break;
        case 2: *reinterpret_cast<bool*>(_v) = _t->edgeScrollOutside(); break;
        case 3: *reinterpret_cast<int*>(_v) = _t->scrollSpeed(); break;
        case 4: *reinterpret_cast<bool*>(_v) = _t->zoomEnabled(); break;
        case 5: *reinterpret_cast<bool*>(_v) = _t->shadowsEnabled(); break;
        case 6: *reinterpret_cast<bool*>(_v) = _t->gridEnabled(); break;
        case 7: *reinterpret_cast<bool*>(_v) = _t->labelsEnabled(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 2: _t->setEdgeScrollOutside(*reinterpret_cast<bool*>(_v)); break;
        case 3: _t->setScrollSpeed(*reinterpret_cast<int*>(_v)); break;
        case 4: _t->setZoomEnabled(*reinterpret_cast<bool*>(_v)); break;
        case 5: _t->setShadowsEnabled(*reinterpret_cast<bool*>(_v)); break;
        case 6: _t->setGridEnabled(*reinterpret_cast<bool*>(_v)); break;
        case 7: _t->setLabelsEnabled(*reinterpret_cast<bool*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *TilesetView::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *TilesetView::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11TilesetViewE_t>.strings))
        return static_cast<void*>(this);
    return QQuickPaintedItem::qt_metacast(_clname);
}

int TilesetView::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QQuickPaintedItem::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 11)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 11;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void TilesetView::hoverChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void TilesetView::edgeScrollOutsideChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void TilesetView::scrollSpeedChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void TilesetView::zoomEnabledChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void TilesetView::shadowsEnabledChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void TilesetView::gridEnabledChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void TilesetView::labelsEnabledChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void TilesetView::cameraChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}
QT_WARNING_POP
