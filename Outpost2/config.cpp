#include "config.h"

#include <QSettings>
#include <QtGlobal>

Config::Config(const QString& iniPath, QObject* parent) : QObject(parent) {
    QSettings ini(iniPath, QSettings::IniFormat);

    outpost2_ = ini.value(QStringLiteral("Game/Outpost2"), outpost2_).toString();
    gameSpeed_ = qBound(1, ini.value(QStringLiteral("Game/GameSpeed"), 10).toInt(), 10);
    scrollRate_ = qBound(1, ini.value(QStringLiteral("Game/ScrollRate"), 10).toInt(), 100);
    scrollWindow_ = ini.value(QStringLiteral("Game/ScrollWindow"), 1).toInt() != 0;
    zoom_ = ini.value(QStringLiteral("Game/Zoom"), 1).toInt() != 0;
    stationaryShadows_ = ini.value(QStringLiteral("Game/StationaryShadows"), 1).toInt() != 0;
    mobileShadows_ = ini.value(QStringLiteral("Game/MobileShadows"), 1).toInt() != 0;
    name_ = ini.value(QStringLiteral("Game/Name"), QStringLiteral("Player")).toString();

    // [UI] SkinDir: directory of OP2 interface (HUD) bitmaps to color-key and draw.
    // Points at the user's own files (e.g. the repo's ui/demo) — not bundled in builds.
    uiSkinDir_ = ini.value(QStringLiteral("UI/SkinDir"), uiSkinDir_).toString();
    // [UI] AssetsDir: shared UI assets (replaceable button graphics, etc.).
    uiAssetsDir_ = ini.value(QStringLiteral("UI/AssetsDir"), uiAssetsDir_).toString();
    // [UI] UseSkin: 1 = draw the HUD bitmaps, 0 = hand-drawn frame only (pre-skin look).
    useSkin_ = ini.value(QStringLiteral("UI/UseSkin"), 1).toInt() != 0;
}
