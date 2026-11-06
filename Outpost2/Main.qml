import QtQuick
import QtQuick.Layouts

// =============================================================================
// Main — the application window + HUD frame
// =============================================================================
//
// Owns the window layout (top bar / left bar / game view + status bar /
// sidebar) and the command-bar state. The sidebar panels live in their own
// files — DebugPanel, ResourcesPanel, SpacePanel, SelectionPanel — each taking
// the TilesetView as `view`; shared colors/metrics come from the Theme
// singleton. Panel data is mostly sim-blocked (see docs/COMMAND_PANELS.md).

Window {
    id: root
    width: 1344
    height: 672
    visible: true
    title: qsTr("Outpost 2: Divided Destiny")
    color: "#202020"

    // Which command-bar menu button is selected (0-5), or -1 = none.
    property int selectedCmd: -1

    // Draw the OP2 HUD bitmap "skin"? Needs both the master toggle ([UI] UseSkin) on and a
    // skin directory set ([UI] SkinDir). Off → the hand-drawn frame stands alone (pre-skin look).
    readonly property bool hudSkin: config.useSkin && config.uiSkinDir !== ""

    // Opening a different command resets every panel's sub-page (OP2 behaviour:
    // a report always opens on its first page). Panels own their page state.
    onSelectedCmdChanged: {
        resourcesPanel.page = "summary"
        spacePanel.page = "satellites"
    }

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

    // Drive animation off the game tick so it follows the game speed. The frame
    // cadence is fixed; ticksThisFrame is how many game ticks elapsed this frame
    // (scales with game speed), and animation advances by exactly that much.
    Connections {
        target: gameClock
        function onTicked() { tilesetView.advanceAnimation(gameClock.ticksThisFrame) }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // --- Top bar: full width (title strip / window controls area) ------
        Rectangle {
            id: topBar
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.topBarHeight
            color: "#3a3a3a"

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: "OUTPOST 2"
                color: Theme.hudGreen
                font.pixelSize: Theme.fontHeading
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
                Layout.preferredWidth: Theme.leftBarWidth
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
                        // Left-click → SELECT the object under the cursor (shows the detail
                        // panel / "7th view"); clicking empty ground deselects, or, with one of
                        // your vehicles selected, sends it there (TilesetView::selectAt).
                        // Right-click → DESELECT the current object.
                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            onTapped: (eventPoint) => {
                                tilesetView.selectAt(eventPoint.position.x, eventPoint.position.y)
                                // Selecting an object shows its detail panel, overriding any open
                                // command report (OP2 behaviour). The object stays selected if a
                                // report is later reopened.
                                if (tilesetView.selectedValid)
                                    root.selectedCmd = -1
                            }
                        }
                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: tilesetView.clearSelection()
                        }
                    }

                    // OP2 delayed-hover name tooltip — appears next to the cursor after ~1s over an
                    // object (the corner box + HP bar are drawn by TilesetView). Green-on-black with
                    // a green border; clamped inside the game view.
                    Rectangle {
                        id: hoverTip
                        visible: tilesetView.hoverObjActive && tilesetView.hoverObjName !== ""
                        z: 50
                        width: tipText.implicitWidth + 10
                        height: tipText.implicitHeight + 4
                        x: Math.min(tilesetView.hoverCursorX + 18, gameView.width - width - 4)
                        y: Math.max(2, Math.min(tilesetView.hoverCursorY - height / 2,
                                               gameView.height - height - 2))
                        color: "#000000"
                        border.color: Theme.panelGreen
                        border.width: 1
                        Text {
                            id: tipText
                            anchors.centerIn: parent
                            text: tilesetView.hoverObjName
                            color: Theme.panelGreen
                            font.pixelSize: Theme.fontHeading
                        }
                    }
                }

                // Status bar — coords on the left, resources on the right.
                Rectangle {
                    id: statusBar
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.statusBarHeight
                    color: "#2b2b2b"

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        // Tile coords, plus the hovered building's name + HP (when over one).
                        text: tilesetView.hoverTileX + "," + tilesetView.hoverTileY +
                              (tilesetView.hoverInfo !== "" ? "    " + tilesetView.hoverInfo : "")
                        color: Theme.hudGreen
                        font.pixelSize: Theme.fontTitle
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Common: 0    Rare: 0"
                        color: "#c0a040"
                        font.pixelSize: Theme.fontTitle
                    }
                }
            }

            // --- Right: fixed-width sidebar (full height of the body) ------
            Rectangle {
                id: sidebar
                Layout.preferredWidth: Theme.sidebarWidth
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
                    width: Theme.minimapWidth
                    height: Theme.minimapHeight
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

                // Command bar — OP2's 6 menu buttons (art groups 40-45, fixed 42px tall).
                // Per button: frame 0 = normal, 1 = green ("clicked"/selected), 2 = disabled.
                // Click toggles selection; the selected button opens its report panel below.
                Row {
                    id: commandBar
                    anchors.top: minimap.bottom
                    anchors.topMargin: 6
                    anchors.horizontalCenter: minimap.horizontalCenter
                    spacing: 0
                    Repeater {
                        // OP2 left-to-right (verified against the art + confirmed by the user):
                        //   Gear=41, Microscope=44, Truck=43, Comms=42, Space=45, Game=40.
                        // (Indices 0-5 here line up with the cmdPanel.titles array.)
                        model: [41, 44, 43, 42, 45, 40]
                        delegate: Image {
                            // frame: green when selected, else normal (disabled=2 wired later)
                            source: "image://op2art/" + modelData + "/" +
                                    (root.selectedCmd === index ? 1 : 0)
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.selectedCmd =
                                    (root.selectedCmd === index ? -1 : index)
                            }
                        }
                    }
                }

                // Command panel — swaps in below the command bar when a button is selected
                // (OP2's lower-sidebar report pages). Fills the rest of the sidebar; hidden
                // when nothing is selected. Each report's body is its own file (DebugPanel /
                // ResourcesPanel / SpacePanel); the small Factories/Labs and Comms lists
                // live inline below.
                Rectangle {
                    id: cmdPanel
                    visible: root.selectedCmd >= 0
                    z: 10
                    anchors.top: commandBar.bottom
                    anchors.topMargin: 4
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    color: "#000000"

                    // Title per command button (index = position in the command bar). OP2 mapping:
                    // Gear→Factories, Science→Labs, Truck→Resources, Comms→Communications,
                    // Spaceship→Space Program, Sierra→Game. Resources + Space have sub-pages,
                    // so their titles come from the panel that owns the page state.
                    readonly property var titles: ["Factories Report", "Labs: Current Research",
                        "Resources Report", "Communications: Messages",
                        "Space Program: Satellites", "Game Options"]

                    Text {
                        id: cmdTitle
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 4
                        text: root.selectedCmd < 0 ? ""
                              : root.selectedCmd === 2 ? resourcesPanel.title
                              : root.selectedCmd === 4 ? spacePanel.title
                              : cmdPanel.titles[root.selectedCmd]
                        color: Theme.panelGreen
                        font.pixelSize: Theme.fontTitle
                        font.bold: true
                    }

                    // Game menu (index 5): 4 OP2-style buttons (ui/group50-*.png: 0 normal / 1 pressed).
                    Column {
                        id: gameButtons
                        visible: root.selectedCmd === 5
                        anchors.top: cmdTitle.bottom
                        anchors.topMargin: 12
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 6
                        Repeater {
                            model: ["MISSION OBJECTIVES", "ONLINE MANUAL", "PREFERENCES", "END MISSION"]
                            delegate: Item {
                                width: Theme.gameButtonWidth
                                height: Theme.gameButtonHeight
                                Image {
                                    anchors.fill: parent
                                    // Long button graphic (group 50), loaded from a file so it
                                    // can be customised: ui/group50-0.png (normal) / -1.png (pressed).
                                    source: "image://ui/group50-" + (gbtnMouse.pressed ? 1 : 0) + ".png"
                                }
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: Theme.buttonGold
                                    font.pixelSize: Theme.fontBody
                                    font.bold: true
                                }
                                MouseArea {
                                    id: gbtnMouse
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: console.log("Game menu: " + modelData) // TODO: wire actions
                                }
                            }
                        }
                    }

                    // Developer/debug controls under the Game menu (below END MISSION).
                    DebugPanel {
                        visible: root.selectedCmd === 5
                        view: tilesetView
                        anchors.top: gameButtons.bottom
                        anchors.topMargin: 16
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: 8
                        anchors.rightMargin: 6
                        anchors.bottomMargin: 6
                    }

                    // Building list reports — Factories (index 0 = Gear) and Labs (index 1 = Science).
                    // Both list the local player's buildings + a status line (production/research is
                    // sim-blocked → "... Nothing" for now). One ListView drives both; the model
                    // switches on the selected button.
                    ListView {
                        id: reportList
                        visible: root.selectedCmd === 0 || root.selectedCmd === 1
                        anchors.top: cmdTitle.bottom
                        anchors.topMargin: 10
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: 8
                        anchors.rightMargin: 6
                        anchors.bottomMargin: 6
                        clip: true
                        spacing: 8
                        model: root.selectedCmd === 0 ? tilesetView.factoriesReport
                             : root.selectedCmd === 1 ? tilesetView.labsReport : []
                        delegate: Item {
                            width: ListView.view ? ListView.view.width : 0
                            height: repCol.height + 2
                            // Hover highlight (OP2 boxes the entry under the cursor).
                            Rectangle {
                                anchors.fill: parent
                                color: "#0c2a0c"
                                border.color: "#1f5f1f"
                                visible: repMouse.containsMouse
                            }
                            Column {
                                id: repCol
                                y: 1
                                width: parent.width
                                spacing: 1
                                Text {
                                    width: parent.width
                                    text: modelData.title
                                    color: Theme.panelGreen
                                    font.pixelSize: Theme.fontBody
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: "    " + modelData.status
                                    color: Theme.statusGreen
                                    font.pixelSize: Theme.fontBody
                                }
                            }
                            // Double-click → centre the map on this building and select it (its full
                            // info shows on the selection panel, as if clicked in the game view).
                            MouseArea {
                                id: repMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onDoubleClicked: {
                                    tilesetView.focusUnit(modelData.idx, modelData.gen)
                                    root.selectedCmd = -1   // close the report → show pane 7
                                }
                            }
                        }
                    }
                    Text {
                        // Empty-state for the Factories / Labs reports.
                        visible: (root.selectedCmd === 0 && tilesetView.factoriesReport.length === 0) ||
                                 (root.selectedCmd === 1 && tilesetView.labsReport.length === 0)
                        anchors.top: cmdTitle.bottom
                        anchors.topMargin: 14
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 6
                        text: root.selectedCmd === 0 ? "No factories." : "No labs."
                        color: Theme.emptyGreen
                        font.pixelSize: Theme.fontBody
                    }

                    // Resources Report (index 2 = Truck) — its own navigator; see ResourcesPanel.qml.
                    ResourcesPanel {
                        id: resourcesPanel
                        visible: root.selectedCmd === 2
                        view: tilesetView
                        anchors.top: cmdTitle.bottom
                        anchors.topMargin: 10
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: 8
                        anchors.rightMargin: 6
                        anchors.bottomMargin: 6
                        onCloseRequested: root.selectedCmd = -1 // focus-jump → show pane 7
                    }

                    // Communications: Messages (index 3 = Comms). Header is live (current Mark from
                    // the GameClock); the message log is newest-first, colour-coded green (info) /
                    // red (warning). The list is empty until the event/message system exists.
                    Item {
                        id: commsPanel
                        visible: root.selectedCmd === 3
                        anchors.top: cmdTitle.bottom
                        anchors.topMargin: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: 8
                        anchors.rightMargin: 6
                        anchors.bottomMargin: 6

                        Text {
                            id: commsHeader
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            text: gameClock.mark + ": Current Mark"
                            color: Theme.panelGreen
                            font.pixelSize: Theme.fontBody
                            font.bold: true
                        }
                        ListView {
                            anchors.top: commsHeader.bottom
                            anchors.topMargin: 4
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            clip: true
                            spacing: 1
                            model: tilesetView.commsMessages
                            delegate: Text {
                                width: ListView.view ? ListView.view.width : 0
                                // "<mark>: <text>" — red for warnings/deaths, green for info.
                                text: modelData.mark + ": " + modelData.text
                                color: modelData.warn ? Theme.warnRed : Theme.panelGreen
                                font.pixelSize: Theme.fontBody
                                elide: Text.ElideRight
                            }
                        }
                        Text {
                            visible: tilesetView.commsMessages.length === 0
                            anchors.top: commsHeader.bottom
                            anchors.topMargin: 10
                            anchors.left: parent.left
                            anchors.right: parent.right
                            wrapMode: Text.WordWrap
                            text: "(no messages yet — event/message system pending)"
                            color: Theme.emptyGreen
                            font.pixelSize: Theme.fontSmall
                        }
                    }

                    // Space Program (index 4 = Spaceship) — two pages; see SpacePanel.qml.
                    SpacePanel {
                        id: spacePanel
                        visible: root.selectedCmd === 4
                        view: tilesetView
                        anchors.top: cmdTitle.bottom
                        anchors.topMargin: 10
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: 8
                        anchors.rightMargin: 6
                        anchors.bottomMargin: 6
                    }
                }

                // Selection detail panel — the "7th view" (see SelectionPanel.qml). Shows
                // when something is selected AND no command report is open.
                SelectionPanel {
                    visible: tilesetView.selectedValid && root.selectedCmd === -1
                    view: tilesetView
                    z: 9
                    anchors.top: commandBar.bottom
                    anchors.topMargin: 4
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                }
            }
        }
    }

    // --- OP2 HUD bitmap overlay (interface "skin") -----------------------------
    // Draw the real OP2 interface frame pieces over the layout. The "ui" image
    // provider color-keys each piece's #141414 background to transparent, so the
    // game view / sidebar show through the masked-out areas instead of being blocked.
    // Corner pieces are fixed-size, anchored to their corner; edge/border pieces
    // (added later) stretch along their axis so the window can be any size — OP2's
    // own resize model. Shown only when a skin is active (root.hudSkin); otherwise
    // the hand-drawn frame stands alone.

    // Bottom-right corner (468x48).
    Image {
        id: hudBottomRight
        visible: root.hudSkin
        source: "image://ui/Bitmap_170"
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        z: 100
        smooth: false   // pixel-art frame — no smoothing
    }

    // Bottom-left corner.
    Image {
        id: hudBottomLeft
        visible: root.hudSkin
        source: "image://ui/Bitmap_169"
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        z: 100
        smooth: false
    }
}
