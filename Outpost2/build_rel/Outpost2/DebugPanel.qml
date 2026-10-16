import QtQuick

// =============================================================================
// DebugPanel — developer controls under the Game menu
// =============================================================================
//
// Game speed, scroll settings, render toggles, PNG export, scene-load and
// survey buttons. A Flickable so the full set scrolls within the sidebar
// panel on short windows. `view` is the TilesetView the toggles drive;
// `gameClock` and `config` are context properties from main.cpp.
// (Deliberately NOT OP2-styled — these are developer tools, not game UI.)

Flickable {
    id: panel
    required property var view

    contentWidth: width
    contentHeight: debugCol.height
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    // One ON/OFF toggle line: "<label>: ON" — green when on, gray when off.
    component ToggleLine: Text {
        id: line
        property string label
        property bool value
        signal toggled()
        text: line.label + ": " + (line.value ? "ON" : "OFF")
        color: line.value ? Theme.hudGreen : Theme.grayText
        font.pixelSize: Theme.fontBody
        MouseArea {
            anchors.fill: parent
            anchors.margins: -3
            cursorShape: Qt.PointingHandCursor
            onClicked: line.toggled()
        }
    }

    // A −/+ value stepper row (game speed, scroll speed).
    component Stepper: Row {
        id: stepper
        property int value
        signal decrement()
        signal increment()
        spacing: 8
        Text {
            text: "Speed:"
            color: Theme.grayText
            font.pixelSize: Theme.fontBody
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: "−" // minus
            color: Theme.lightText
            font.pixelSize: 14
            font.bold: true
            anchors.verticalCenter: parent.verticalCenter
            MouseArea {
                anchors.fill: parent
                anchors.margins: -4
                cursorShape: Qt.PointingHandCursor
                onClicked: stepper.decrement()
            }
        }
        Text {
            text: stepper.value
            color: Theme.lightText
            font.pixelSize: Theme.fontBody
            width: 18
            horizontalAlignment: Text.AlignHCenter
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: "+"
            color: Theme.lightText
            font.pixelSize: 14
            font.bold: true
            anchors.verticalCenter: parent.verticalCenter
            MouseArea {
                anchors.fill: parent
                anchors.margins: -4
                cursorShape: Qt.PointingHandCursor
                onClicked: stepper.increment()
            }
        }
    }

    // A dev action button.
    component DevButton: Rectangle {
        id: btn
        property string label
        property int fontSize: Theme.fontBody
        signal clicked()
        width: Theme.devButtonWidth
        height: Theme.devButtonHeight
        radius: 3
        color: btnMouse.pressed ? Theme.devBtnPressed : Theme.devBtnBg
        border.color: Theme.devBtnBorder
        Text {
            anchors.centerIn: parent
            text: btn.label
            color: Theme.lightText
            font.pixelSize: btn.fontSize
        }
        MouseArea {
            id: btnMouse
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: btn.clicked()
        }
    }

    Column {
        id: debugCol
        width: parent.width
        spacing: 14

        Text {
            text: "Debug"
            color: Theme.hudGreen
            font.pixelSize: Theme.fontHeading
            font.bold: true
        }

        // Game time readout — Mark (user-facing), tick (internal), and real-world
        // elapsed duration. Driven by the GameClock context property from main.cpp.
        Column {
            spacing: 2
            Text {
                text: "Mark: " + gameClock.mark
                color: Theme.hudGreen
                font.pixelSize: 14
                font.bold: true
            }
            Text {
                text: "Tick: " + gameClock.tick
                color: Theme.grayText
                font.pixelSize: Theme.fontBody
            }
            Text {
                text: "Time: " + gameClock.elapsed
                color: Theme.grayText
                font.pixelSize: Theme.fontBody
            }
            // Game speed stepper (1–10) — also bound to the number-key shortcuts.
            // Changing this scales tick generation, so all animation follows it.
            Stepper {
                value: gameClock.gameSpeed
                onDecrement: gameClock.gameSpeed = Math.max(1, gameClock.gameSpeed - 1)
                onIncrement: gameClock.gameSpeed = Math.min(10, gameClock.gameSpeed + 1)
            }
        }

        // Scroll settings — edge-scroll toggles + scroll speed.
        Column {
            spacing: 4
            Text {
                text: "Scroll"
                color: Theme.hudGreen
                font.pixelSize: Theme.fontHeading
                font.bold: true
            }
            // ON/OFF: window/mouse edge-scroll. OFF = arrow keys only.
            ToggleLine {
                label: "By Window/Mouse"
                value: panel.view.edgeScrollEnabled
                onToggled: panel.view.edgeScrollEnabled = !panel.view.edgeScrollEnabled
            }
            // ON/OFF: keep scrolling past the window edge (OP2 style).
            ToggleLine {
                label: "Past window"
                value: panel.view.edgeScrollOutside
                onToggled: panel.view.edgeScrollOutside = !panel.view.edgeScrollOutside
            }
            Stepper {
                value: panel.view.scrollSpeed
                onDecrement: panel.view.scrollSpeed = Math.max(2, panel.view.scrollSpeed - 2)
                onIncrement: panel.view.scrollSpeed = Math.min(40, panel.view.scrollSpeed + 2)
            }
        }

        // Render settings — shadows, grid, labels, animation and overlay toggles,
        // then the export / scene-load / survey dev actions.
        Column {
            spacing: 4
            Text {
                text: "Render"
                color: Theme.hudGreen
                font.pixelSize: Theme.fontHeading
                font.bold: true
            }
            ToggleLine {
                label: "Bldg Shadows"
                value: panel.view.shadowsEnabled
                onToggled: panel.view.shadowsEnabled = !panel.view.shadowsEnabled
            }
            ToggleLine {
                label: "Unit Shadows"
                value: panel.view.mobileShadowsEnabled
                onToggled: panel.view.mobileShadowsEnabled = !panel.view.mobileShadowsEnabled
            }
            ToggleLine {
                label: "Grid"
                value: panel.view.gridEnabled
                onToggled: panel.view.gridEnabled = !panel.view.gridEnabled
            }
            ToggleLine {
                label: "Labels"
                value: panel.view.labelsEnabled
                onToggled: panel.view.labelsEnabled = !panel.view.labelsEnabled
            }
            ToggleLine {
                label: "Struct Anim"
                value: panel.view.ambientStructureEnabled
                onToggled: panel.view.ambientStructureEnabled = !panel.view.ambientStructureEnabled
            }
            ToggleLine {
                label: "Unit Anim"
                value: panel.view.ambientUnitEnabled
                onToggled: panel.view.ambientUnitEnabled = !panel.view.ambientUnitEnabled
            }
            ToggleLine {
                label: "Resource Anim"
                value: panel.view.ambientResourceEnabled
                onToggled: panel.view.ambientResourceEnabled = !panel.view.ambientResourceEnabled
            }
            ToggleLine {
                label: "Overlay"
                value: panel.view.overlayEnabled
                onToggled: panel.view.overlayEnabled = !panel.view.overlayEnabled
            }

            // Gap before the export buttons so they stand apart from the toggles.
            Item { width: 1; height: 12 }

            // Export the current viewport to a PNG under exports/.
            DevButton {
                label: exportLabel.savedFor > 0 ? "Saved!" : "Export PNG"
                onClicked: {
                    if (panel.view.exportViewport() !== "") {
                        exportLabel.savedFor = 1
                        exportLabel.restart()
                    }
                }
                Timer {
                    id: exportLabel
                    property int savedFor: 0
                    interval: 1500; repeat: false
                    onTriggered: savedFor = 0
                }
            }

            Item { width: 1; height: 6 }

            // Export the ENTIRE map (every tile/building/unit at 1:1), not just
            // the visible viewport.
            DevButton {
                label: exportMapLabel.savedFor > 0 ? "Saved!" : "Export Map"
                onClicked: {
                    if (panel.view.exportFullMap() !== "") {
                        exportMapLabel.savedFor = 1
                        exportMapLabel.restart()
                    }
                }
                Timer {
                    id: exportMapLabel
                    property int savedFor: 0
                    interval: 1500; repeat: false
                    onTriggered: savedFor = 0
                }
            }

            // Gap, then scene-switch buttons: the C++ catalog parade (plain .map)
            // and two .opm missions. Each calls TilesetView.loadMap at runtime.
            Item { width: 1; height: 16 }

            Repeater {
                model: [
                    { label: "C++ Parade",   map: "PlainsAndPasses/opu_12.map" },
                    { label: "Showcase OPM", map: "PlainsAndPasses/opu_12_showcase.opm" },
                    { label: "Colony OPM",   map: "PlainsAndPasses/opu_12_colony.opm" }
                ]
                delegate: Column {
                    spacing: 0
                    DevButton {
                        label: modelData.label
                        onClicked: panel.view.loadMap(modelData.map)
                    }
                    Item { width: 1; height: 6 } // small gap between the load buttons
                }
            }

            // Gap, then a dev button to test the EDWARD-Satellite survey:
            // reveals every mining beacon on the map at once.
            Item { width: 1; height: 16 }

            DevButton {
                width: 130
                fontSize: Theme.fontSmall
                label: surveyLabel.count >= 0 ? ("Revealed " + surveyLabel.count)
                                              : "Survey Mines (EDWARD)"
                onClicked: {
                    surveyLabel.count = panel.view.surveyAllMines(0)
                    surveyLabel.restart()
                }
                Timer {
                    id: surveyLabel
                    property int count: -1   // -1 = idle (show the button label)
                    interval: 1500; repeat: false
                    onTriggered: count = -1
                }
            }
        }
    }
}
