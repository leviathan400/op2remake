/****************************************************************************
** Generated QML type registration code
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <QtQml/qqml.h>
#include <QtQml/qqmlmoduleregistration.h>

#if __has_include(<minimapview.h>)
#  include <minimapview.h>
#endif
#if __has_include(<tilesetview.h>)
#  include <tilesetview.h>
#endif


#if !defined(QT_STATIC)
#define Q_QMLTYPE_EXPORT Q_DECL_EXPORT
#else
#define Q_QMLTYPE_EXPORT
#endif
Q_QMLTYPE_EXPORT void qml_register_types_Outpost2()
{
    qmlRegisterModule("Outpost2", 254, 0);
    QT_WARNING_PUSH QT_WARNING_DISABLE_DEPRECATED
    qmlRegisterTypesAndRevisions<MinimapView>("Outpost2", 254);
    qmlRegisterAnonymousType<QQuickItem, 254>("Outpost2", 254);
    qmlRegisterTypesAndRevisions<TilesetView>("Outpost2", 254);
    QT_WARNING_POP
    qmlRegisterModule("Outpost2", 254, 254);
}

static const QQmlModuleRegistration outpost2Registration("Outpost2", qml_register_types_Outpost2);
