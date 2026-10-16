#include <QCoreApplication>
#include <QFont>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>

#include "config.h"
#include "gameclock.h"
#include "logger.h"
#include "render/op2artimageprovider.h"
#include "render/uiimageprovider.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // OP2's UI font: the original creates Arial via CreateFontIndirect (LOGFONT.lfFaceName="Arial",
    // heights -13 regular/bold and -15 bold, TrueType) and rasterises glyphs through GDI. Match it:
    // default the whole UI to Arial, drawn with the NATIVE (GDI) text rasteriser so small text has
    // the same crisp pixel look as the original (not Qt's antialiased distance-field rendering).
    QGuiApplication::setFont(QFont(QStringLiteral("Arial")));
    QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);

    // Open the file log first so startup (catalog/art load, etc.) is captured.
    Logger::init(QCoreApplication::applicationDirPath());

    QQmlApplicationEngine engine;

    // Config + GameClock are exposed to QML as context properties `config` and
    // `gameClock`. outpost2.ini sits next to the exe (copied there by the build).
    Config config(QCoreApplication::applicationDirPath() + QStringLiteral("/outpost2.ini"));
    engine.rootContext()->setContextProperty("config", &config);

    GameClock gameClock;
    gameClock.setGameSpeed(config.gameSpeed()); // initial speed from the ini
    engine.rootContext()->setContextProperty("gameClock", &gameClock);

    // Expose op2_art sprites to QML: Image { source: "image://op2art/<group>/<frame>" }
    // (e.g. group 50 = the OP2 button: frame 0 normal, frame 1 pressed). Engine takes ownership.
    auto* artProvider = new Op2ArtImageProvider();
    artProvider->load(config.outpost2() + QStringLiteral("/OPU/base/sprites/op2_art.prt"),
                      config.outpost2() + QStringLiteral("/OP2_ART.BMP"));
    engine.addImageProvider(QStringLiteral("op2art"), artProvider);

    // Expose OP2 interface (HUD) bitmaps to QML: Image { source: "image://ui/<name>" }
    // The provider color-keys each piece's #141414 background to transparent so the
    // masked-out areas don't block the game view. SkinDir (demo/retail) comes from the
    // ini — pointing at the user's own files, so no HUD art is bundled. Engine owns it.
    auto* uiProvider = new UiImageProvider();
    uiProvider->setSkinDir(config.uiSkinDir());
    uiProvider->setCommonDir(config.uiAssetsDir()); // shared button graphics etc.
    engine.addImageProvider(QStringLiteral("ui"), uiProvider);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("Outpost2", "Main");

    gameClock.start();

    return QGuiApplication::exec();
}
