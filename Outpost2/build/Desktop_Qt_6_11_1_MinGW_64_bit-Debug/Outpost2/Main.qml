import QtQuick
import QtQuick.Layouts

Window {
    id: root
    width: 1344
    height: 672
    visible: true
    title: qsTr("Outpost 2: Divided Destiny")
    color: "#202020"

    // --- HUD frame dimensions (measured from the original OP2 UI) ----------
    // The HUD is a fixed beveled frame: a full-width top bar, a thin left
    // border bar, a fixed-width sidebar on the right, and a status bar along
    // the bottom of the game-view column. The game view flexes to fill the
    // remaining space.
    readonly property int topBarHeight: 22
    readonly property int leftBarWidth: 12
    readonly property int sidebarWidth: 212
    readonly property int statusBarHeight: 42

    // Minimap is a fixed-size panel at the top of the sidebar.
    readonly property int minimapWidth: 192
    readonly property int minimapHeight: 128

    // Game-speed hotkeys: 1-9 select speeds 1-9, 0 selects speed 10 (default).
    Shortcut { sequence: "1"; onActivated: gameClock.gameSpeed = 1 }
    Shortcut { sequence: "2"; onActivated: gameClock.gameSpeed = 2 }
    Shortcut { sequence: "3"; onActivated: gameClock.gameSpeed = 3 }
    Shortcut { sequence: "4"; onActivated: gameClock.gameSpeed = 4 }
    Shortcut { sequence: "5"; onActivated: gameClock.gameSpeed = 5 }
    Shortcut { sequence: "6"; onActivated: gameClock.gameSpeed = 6 }
    Shortcut { sequence: "7"; onActivated: gameClock.gameSpeed = 7 }
    Shortcut { sequence: "8"; onActivated: gameClock.gameSpeed = 8 }
    Shortcut { sequence: "9"; onActivated: gameClock.gameSpeed = 9 }
    Shortcut { sequence: "0"; onActivated: gameClock.gameSpeed = 10 }

    // Drive sprite animation off the game tick so it follows the game speed.
    Connections {
        target: gameClock
        function onTicked() { tilesetView.advanceAnimation() }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // --- Top bar: full width (title strip / window controls area) ------
        Rectangle {
            id: topBar
            Layout.fillWidth: true
            Layout.preferredHeight: root.topBarHeight
            color: "#3a3a3a"

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: "OUTPOST 2"
                color: "#7fbf4f"
                font.pixelSize: 13
                font.bold: true
            }
        }

        // --- Body row: left bar | (game view + status bar) | sidebar -------
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Left border bar — thin fixed-width HUD edge.
            Rectangle {
                id: leftBar
                Layout.preferredWidth: root.leftBarWidth
                Layout.fillHeight: true
                color: "#303030"
            }

            // Center column: game view (flexes) + status bar (fixed height).
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // Game view — the map/world viewport.
                Rectangle {
                    id: gameView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#101418"

                    // Map renderer: loads + draws the map. RTS edge-scroll and
                    // arrow keys pan the camera; reports hovered tile to the
                    // status bar. focus:true so it receives arrow keys.
                    TilesetView {
                        id: tilesetView
                        anchors.fill: parent
                        focus: true
                        // Initial values from outpost2.ini (the sidebar can
                        // still change them at runtime).
                        Component.onCompleted: {
                            scrollSpeed = config.scrollRate
                            edgeScrollOutside = config.scrollWindow
                            zoomEnabled = config.zoom
                        }
                        // Mouse wheel toggles the one zoom-out level.
                        WheelHandler {
                            onWheel: (event) => tilesetView.zoomAt(
                                event.angleDelta.y, event.x, event.y)
                        }
                    }
                }

                // Status bar — coords on the left, resources on the right.
                Rectangle {
                    id: statusBar
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.statusBarHeight
                    color: "#2b2b2b"

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        text: tilesetView.hoverTileX + "," + tilesetView.hoverTileY
                        color: "#7fbf4f"
                        font.pixelSize: 12
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Common: 0    Rare: 0"
                        color: "#c0a040"
                        font.pixelSize: 12
                    }
                }
            }

            // --- Right: fixed-width sidebar (full height of the body) ------
            Rectangle {
                id: sidebar
                Layout.preferredWidth: root.sidebarWidth
                Layout.fillHeight: true
                color: "#303030"

                // Thin divider against the game view, like the original bevel.
                Rectangle {
                    anchors.left: parent.left
                    width: 1
                    height: parent.height
                    color: "#101010"
                }

                // Minimap — fixed 192x128 panel, centered at the top.
                Rectangle {
                    id: minimap
                    width: root.minimapWidth
                    height: root.minimapHeight
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 8
                    color: "#0c0f12"
                    border.color: "#101010"
                    border.width: 1

                    // Renders the downscaled map + the camera view box.
                    MinimapView {
                        anchors.fill: parent
                        anchors.margins: 1
                        source: tilesetView
                    }
                }

                // Game time readout — Mark (user-facing), tick (internal),
                // and real-world elapsed duration. Driven by the GameClock
                // context property from main.cpp.
                Column {
                    id: timeReadout
                    anchors.top: minimap.bottom
                    anchors.topMargin: 8
                    anchors.left: minimap.left
                    spacing: 2

                    Text {
                        text: "Mark: " + gameClock.mark
                        color: "#7fbf4f"
                        font.pixelSize: 14
                        font.bold: true
                    }
                    Text {
                        text: "Tick: " + gameClock.tick
                        color: "#9aa0a6"
                        font.pixelSize: 11
                    }
                    Text {
                        text: "Time: " + gameClock.elapsed
                        color: "#9aa0a6"
                        font.pixelSize: 11
                    }
                    Text {
                        text: "Speed: " + gameClock.gameSpeed
                        color: "#9aa0a6"
                        font.pixelSize: 11
                    }
                }

                // Scroll settings — edge-scroll-past-window toggle + scroll speed.
                Column {
                    id: scrollSettings
                    anchors.top: timeReadout.bottom
                    anchors.topMargin: 14
                    anchors.left: minimap.left
                    spacing: 4

                    Text {
                        text: "Scroll"
                        color: "#7fbf4f"
                        font.pixelSize: 13
                        font.bold: true
                    }

                    // ON/OFF: keep scrolling past the window edge (OP2 style).
                    Text {
                        text: "Past window: " + (tilesetView.edgeScrollOutside ? "ON" : "OFF")
                        color: tilesetView.edgeScrollOutside ? "#7fbf4f" : "#9aa0a6"
                        font.pixelSize: 11
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -3
                            cursorShape: Qt.PointingHandCursor
                            onClicked: tilesetView.edgeScrollOutside = !tilesetView.edgeScrollOutside
                        }
                    }

                    // Scroll speed stepper.
                    Row {
                        spacing: 8
                        Text {
                            text: "Speed:"
                            color: "#9aa0a6"
                            font.pixelSize: 11
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: "−" // minus
                            color: "#cccccc"
                            font.pixelSize: 14
                            font.bold: true
                            anchors.verticalCenter: parent.verticalCenter
                            MouseArea {
                                anchors.fill: parent
                                anchors.margins: -4
                                cursorShape: Qt.PointingHandCursor
                                onClicked: tilesetView.scrollSpeed = Math.max(2, tilesetView.scrollSpeed - 2)
                            }
                        }
                        Text {
                            text: tilesetView.scrollSpeed
                            color: "#cccccc"
                            font.pixelSize: 11
                            width: 18
                            horizontalAlignment: Text.AlignHCenter
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: "+"
                            color: "#cccccc"
                            font.pixelSize: 14
                            font.bold: true
                            anchors.verticalCenter: parent.verticalCenter
                            MouseArea {
                                anchors.fill: parent
                                anchors.margins: -4
                                cursorShape: Qt.PointingHandCursor
                                onClicked: tilesetView.scrollSpeed = Math.min(40, tilesetView.scrollSpeed + 2)
                            }
                        }
                    }
                }

                // Render settings — building shadow toggle.
                Column {
                    id: renderSettings
                    anchors.top: scrollSettings.bottom
                    anchors.topMargin: 14
                    anchors.left: minimap.left
                    spacing: 4

                    Text {
                        text: "Render"
                        color: "#7fbf4f"
                        font.pixelSize: 13
                        font.bold: true
                    }

                    Text {
                        text: "Shadows: " + (tilesetView.shadowsEnabled ? "ON" : "OFF")
                        color: tilesetView.shadowsEnabled ? "#7fbf4f" : "#9aa0a6"
                        font.pixelSize: 11
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -3
                            cursorShape: Qt.PointingHandCursor
                            onClicked: tilesetView.shadowsEnabled = !tilesetView.shadowsEnabled
                        }
                    }

                    Text {
                        text: "Grid: " + (tilesetView.gridEnabled ? "ON" : "OFF")
                        color: tilesetView.gridEnabled ? "#7fbf4f" : "#9aa0a6"
                        font.pixelSize: 11
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -3
                            cursorShape: Qt.PointingHandCursor
                            onClicked: tilesetView.gridEnabled = !tilesetView.gridEnabled
                        }
                    }

                    Text {
                        text: "Labels: " + (tilesetView.labelsEnabled ? "ON" : "OFF")
                        color: tilesetView.labelsEnabled ? "#7fbf4f" : "#9aa0a6"
                        font.pixelSize: 11
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -3
                            cursorShape: Qt.PointingHandCursor
                            onClicked: tilesetView.labelsEnabled = !tilesetView.labelsEnabled
                        }
                    }

                    // Export the current viewport to a PNG under exports/.
                    Rectangle {
                        width: 96
                        height: 22
                        radius: 3
                        color: exportMouse.pressed ? "#3a3f46" : "#2b2f36"
                        border.color: "#4a4f57"
                        Text {
                            anchors.centerIn: parent
                            text: exportLabel.savedFor > 0 ? "Saved!" : "Export PNG"
                            color: "#cccccc"
                            font.pixelSize: 11
                        }
                        Timer {
                            id: exportLabel
                            property int savedFor: 0
                            interval: 1500; repeat: false
                            onTriggered: savedFor = 0
                        }
                        MouseArea {
                            id: exportMouse
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (tilesetView.exportViewport() !== "") {
                                    exportLabel.savedFor = 1
                                    exportLabel.restart()
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
