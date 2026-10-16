import QtQuick

// =============================================================================
// SelectionPanel — the "7th view": the selected object's stats + actions
// =============================================================================
//
// Shows the object the player selected in the game view (left-click): portrait
// + category icon, armor class, HP bar, status, weapon, and the building's
// operating requirements from building.txt. The IDLE/ACTIVATE action routes
// through TilesetView.setSelectedIdle → the sim command layer. Fields we have
// now are real; storage/usage are placeholders until the economy sim (Phase 3).
// Spec: docs/SELECTION_PANEL.md.

Rectangle {
    id: panel
    required property var view
    readonly property var si: view.selectedInfo

    color: "#000000"

    Column {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 6
        spacing: 6

        // Title: "Name  #player-index" (e.g. "Structure Factory #1-01").
        Text {
            width: parent.width
            text: (panel.si.name || "") + "  #" + (panel.si.owner || 0) +
                  "-" + ("0" + (panel.si.idx || 0)).slice(-2)
            color: Theme.panelGreen
            font.pixelSize: Theme.fontTitle
            font.bold: true
            elide: Text.ElideRight
        }

        // Portrait box (OP2 art group 54, 60×60) + armor class + HP bar, laid
        // out side-by-side like OP2.
        Row {
            width: parent.width
            spacing: 8

            Item {
                width: 60
                height: 60
                // The frame/border bitmap.
                Image {
                    anchors.fill: parent
                    source: "image://op2art/54/0"
                    smooth: false
                }
                // The object's category icon (ui/tech/Tech_*.png), drawn at its
                // native 45×45 (NO scaling) inside the frame. iconName = file stem.
                Image {
                    anchors.centerIn: parent
                    fillMode: Image.Pad   // never stretch — draw 1:1
                    smooth: false
                    visible: source.toString() !== "" // source is a url, not a string
                    source: panel.si.iconName
                            ? ("file:///" + config.uiAssetsDir + "/tech/" +
                               panel.si.iconName + ".png")
                            : ""
                }
            }

            Column {
                width: parent.width - 68
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                // Armor class — OP2 renders it in the same bright green as the rest
                // of the panel (measured #00ff00 from the reference), not muted.
                Text {
                    width: parent.width
                    text: (panel.si.armorName && panel.si.armorName !== "")
                          ? panel.si.armorName
                          : ("Armor Class " + (panel.si.armor || 0))
                    color: Theme.panelGreen
                    font.pixelSize: Theme.fontBody
                    elide: Text.ElideRight
                }
                // HP value as TEXT above the bar (OP2 layout: "cur/max" then a thin
                // bar below — not the number centred on a thick bar).
                Text {
                    text: (panel.si.hp || 0) + "/" + (panel.si.maxHp || 0)
                    color: Theme.panelGreen
                    font.pixelSize: Theme.fontBody
                }
                Rectangle {
                    width: parent.width
                    height: 6          // OP2's HP bar measures ~5–6px tall
                    color: "#1c1c1c"
                    border.color: "#2a2a2a"
                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.margins: 1
                        width: (parent.width - 2) * Math.max(0, Math.min(1,
                                (panel.si.maxHp > 0 ? panel.si.hp / panel.si.maxHp : 0)))
                        color: Theme.panelGreen
                    }
                }
            }
        }

        // Stat grid — OP2's green. Status keeps its state colour (Active green,
        // Idle gray, Disabled red); sim-blocked values show "None".
        Grid {
            width: parent.width
            columns: 2
            columnSpacing: 10
            rowSpacing: 1   // OP2's stat rows are tightly packed (~11px pitch)
            Text { text: "Status:"; color: Theme.panelGreen; font.pixelSize: Theme.fontBody }
            Text {
                text: panel.si.status || ""
                color: panel.si.status === "Active" ? Theme.panelGreen
                     : panel.si.status === "Idle"   ? Theme.grayText : Theme.statusRed
                font.pixelSize: Theme.fontBody
            }
            Text { visible: (panel.si.weapon || "") !== ""
                   text: "Weapon:"; color: Theme.panelGreen; font.pixelSize: Theme.fontBody }
            Text { visible: (panel.si.weapon || "") !== ""
                   text: panel.si.weapon || ""; color: Theme.panelGreen; font.pixelSize: Theme.fontBody }
            // Operating requirements from building.txt (0 → "None", like OP2).
            Text { visible: panel.si.isBuilding === true
                   text: "Power:"; color: Theme.panelGreen; font.pixelSize: Theme.fontBody }
            Text { visible: panel.si.isBuilding === true
                   text: (panel.si.power || 0) > 0 ? panel.si.power : "None"
                   color: Theme.panelGreen; font.pixelSize: Theme.fontBody }
            Text { visible: panel.si.isBuilding === true
                   text: "Workers:"; color: Theme.panelGreen; font.pixelSize: Theme.fontBody }
            Text { visible: panel.si.isBuilding === true
                   text: (panel.si.workers || 0) > 0 ? panel.si.workers : "None"
                   color: Theme.panelGreen; font.pixelSize: Theme.fontBody }
            Text { visible: panel.si.isBuilding === true
                   text: "Scientists:"; color: Theme.panelGreen; font.pixelSize: Theme.fontBody }
            Text { visible: panel.si.isBuilding === true
                   text: (panel.si.scientists || 0) > 0 ? panel.si.scientists : "None"
                   color: Theme.panelGreen; font.pixelSize: Theme.fontBody }
        }

        Text {
            visible: panel.si.isBuilding === true
            width: parent.width
            wrapMode: Text.WordWrap
            text: "(required amounts from building.txt; live usage + storage pending the economy sim)"
            color: Theme.footnoteGreen
            font.pixelSize: 9
        }

        // --- Debug section: developer info about the selected object. For now
        // just the owning player; more fields as the sim grows.
        Item { width: 1; height: 6 } // spacer
        Text {
            text: "Debug"
            color: Theme.hudGreen
            font.pixelSize: Theme.fontTitle
            font.bold: true
        }
        Grid {
            width: parent.width
            columns: 2
            columnSpacing: 10
            rowSpacing: 1
            Text { text: "Owner:"; color: Theme.grayText; font.pixelSize: Theme.fontBody }
            Text {
                text: "Player " + (panel.si.owner || 0) +
                      (panel.si.friendly ? "  (friendly)" : "  (enemy)")
                color: panel.si.friendly ? Theme.hudGreen : Theme.statusRed
                font.pixelSize: Theme.fontBody
            }
            Text { text: "Kind:"; color: Theme.grayText; font.pixelSize: Theme.fontBody }
            Text { text: panel.si.kind || ""; color: Theme.hudGreen; font.pixelSize: Theme.fontBody }
            Text { text: "ID:"; color: Theme.grayText; font.pixelSize: Theme.fontBody }
            // World UnitId handle: slot index + generation.
            Text {
                text: (panel.si.idx || 0) + "  (gen " + (panel.si.gen || 0) + ")"
                color: Theme.lightText; font.pixelSize: Theme.fontBody
            }
            Text { text: "Location:"; color: Theme.grayText; font.pixelSize: Theme.fontBody }
            // 1-based tile coords (matches the status-bar hover readout).
            Text {
                text: ((panel.si.cellX || 0) + 1) + ", " + ((panel.si.cellY || 0) + 1)
                color: Theme.lightText; font.pixelSize: Theme.fontBody
            }
        }
    }

    // Action button(s) at the bottom. Only structures/GuardPosts can be idled.
    Item {
        visible: panel.si.isBuilding === true
        width: Theme.gameButtonWidth
        height: Theme.gameButtonHeight
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 10
        Image {
            anchors.fill: parent
            // Long button graphic from file (group50-0/-1.png), color-keyed by the ui provider.
            source: "image://ui/group50-" + (idleBtnMouse.pressed ? 1 : 0) + ".png"
        }
        Text {
            anchors.centerIn: parent
            // Idle building → button reactivates it; active building → button idles it.
            text: panel.si.idle ? "ACTIVATE" : "IDLE"
            color: Theme.buttonGold
            font.pixelSize: Theme.fontBody
            font.bold: true
        }
        MouseArea {
            id: idleBtnMouse
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: panel.view.setSelectedIdle(!panel.si.idle)
        }
    }
}
