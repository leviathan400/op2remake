cmake_minimum_required(VERSION 3.16)

project(Outpost2 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

# Deterministic simulation core (no Qt — see sim/CMakeLists.txt firewall note).
add_subdirectory(sim)

# Map-format loaders (depends on op2sim; no Qt).
add_subdirectory(maps)

find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.10)

qt_add_executable(appOutpost2
    main.cpp
    config.h
    config.cpp
    gameclock.h
    gameclock.cpp
    logger.h
    logger.cpp
    render/tileset.h
    render/tileset.cpp
    render/tilesetview.h
    render/tilesetview.cpp           # core: view lifecycle, paint, camera/input, setters
    render/tilesetview_showcase.cpp  # catalog parade (placeAllBuildings/Units)
    render/tilesetview_mission.cpp   # .opm → World render (placeFromWorld, GuardPosts)
    render/tilesetview_anim.cpp      # animation advance (buildings, turrets, sprites)
    render/minimapview.h
    render/minimapview.cpp
    render/artfile.h
    render/artfile.cpp
    render/buildingcatalog.h
    render/buildingcatalog.cpp
    render/unitcatalog.h
    render/unitcatalog.cpp
    render/missionloader.h
    render/missionloader.cpp
    render/op2artimageprovider.h
    render/op2artimageprovider.cpp
    render/uiimageprovider.h
    render/uiimageprovider.cpp
)

# Theme.qml is a singleton (shared palette/metrics) — must be marked before
# qt_add_qml_module so the generated qmldir registers it as one.
set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(appOutpost2
    URI Outpost2
    # TilesetView derives from QQuickPaintedItem (QtQuick); declaring the
    # dependency lets qmltyperegistrar resolve the base type.
    DEPENDENCIES QtQuick
    QML_FILES
        Main.qml
        Theme.qml           # singleton: OP2 palette + HUD metrics
        RedNavButton.qml    # OP2 red-bordered nav button (Resources/Space)
        DebugPanel.qml      # dev controls under the Game menu
        ResourcesPanel.qml  # Resources Report navigator (summary/status/structures)
        SpacePanel.qml      # Space Program (satellites/starship pages)
        SelectionPanel.qml  # the "7th view": selected-object stats + actions
)

# So the generated QML type-registration (#include <tilesetview.h>) finds our
# render headers by basename.
target_include_directories(appOutpost2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/render
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appOutpost2 PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appOutpost2
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appOutpost2
    PRIVATE Qt6::Quick
    PRIVATE op2sim
    PRIVATE op2maps
)

# OP2Utility.dll is implicitly linked (via op2maps) → place it next to the exe.
add_custom_command(TARGET appOutpost2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/third_party/OP2UtilityC/OP2Utility.dll
        $<TARGET_FILE_DIR:appOutpost2>
    VERBATIM)

# outpost2.ini (repo root) sits next to the exe so the app can read it.
add_custom_command(TARGET appOutpost2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/../outpost2.ini
        $<TARGET_FILE_DIR:appOutpost2>
    VERBATIM)

# building_anim_catalog.tsv (repo root) — building→sprite-group table the
# renderer reads to place every building. Sits next to the exe.
add_custom_command(TARGET appOutpost2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/../building_anim_catalog.tsv
        $<TARGET_FILE_DIR:appOutpost2>
    VERBATIM)

# unit_anim_catalog.tsv (repo root) — unit→rotation-sprite table (chassis/turret/
# flying, 16 directions each). Sits next to the exe.
add_custom_command(TARGET appOutpost2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/../unit_anim_catalog.tsv
        $<TARGET_FILE_DIR:appOutpost2>
    VERBATIM)

install(TARGETS appOutpost2
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
