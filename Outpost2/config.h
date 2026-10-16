#pragma once
#include <QObject>
#include <QString>

// =============================================================================
// Config  —  reads outpost2.ini settings, exposes them to QML
// =============================================================================
//
// [Game]
//   GameSpeed    1..10   initial game speed
//   ScrollRate   px/tick  camera scroll speed (edge + arrow keys)
//   ScrollWindow 0/1      keep scrolling when the cursor leaves the window (OP2)
//
// Loaded once at startup (path hardcoded in main.cpp for now). Values are
// read-only here; the in-app UI can still change them at runtime.

class Config : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString outpost2 READ outpost2 CONSTANT)
    Q_PROPERTY(int gameSpeed READ gameSpeed CONSTANT)
    Q_PROPERTY(int scrollRate READ scrollRate CONSTANT)
    Q_PROPERTY(bool scrollWindow READ scrollWindow CONSTANT)
    Q_PROPERTY(bool zoom READ zoom CONSTANT)
    Q_PROPERTY(bool stationaryShadows READ stationaryShadows CONSTANT)
    Q_PROPERTY(bool mobileShadows READ mobileShadows CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString uiSkinDir READ uiSkinDir CONSTANT)
    Q_PROPERTY(QString uiAssetsDir READ uiAssetsDir CONSTANT)
    Q_PROPERTY(bool useSkin READ useSkin CONSTANT)
public:
    explicit Config(const QString& iniPath, QObject* parent = nullptr);

    QString outpost2() const { return outpost2_; }
    // Directory of the active HUD-bitmap "skin" (demo/retail interface graphics).
    QString uiSkinDir() const { return uiSkinDir_; }
    // Directory of shared, skin-independent UI assets (replaceable button graphics, etc.).
    QString uiAssetsDir() const { return uiAssetsDir_; }
    // Draw the OP2 HUD bitmaps? false = use the hand-drawn frame only (pre-skin look).
    bool useSkin() const { return useSkin_; }
    int gameSpeed() const { return gameSpeed_; }
    int scrollRate() const { return scrollRate_; }
    bool scrollWindow() const { return scrollWindow_; }
    bool zoom() const { return zoom_; }
    bool stationaryShadows() const { return stationaryShadows_; }
    bool mobileShadows() const { return mobileShadows_; }
    QString name() const { return name_; }

private:
    QString outpost2_ = QStringLiteral("D:/opu/op2remake");
    int gameSpeed_ = 10;
    int scrollRate_ = 10;
    bool scrollWindow_ = true;
    bool zoom_ = true;
    bool stationaryShadows_ = true;
    bool mobileShadows_ = true;
    QString name_ = QStringLiteral("Player");
    QString uiSkinDir_;    // empty = no HUD bitmaps (fall back to the hand-drawn frame)
    QString uiAssetsDir_;  // shared UI assets (buttons); empty = none
    bool useSkin_ = true;
};
