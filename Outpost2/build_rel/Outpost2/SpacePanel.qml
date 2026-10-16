import QtQuick

// =============================================================================
// SpacePanel — OP2's Space Program report (command index 4, the Spaceship button)
// =============================================================================
//
// Two pages (owned here as `page`): "satellites" (default) lists satellite
// operational status; "starship" (via the bottom STARSHIP button) lists the
// starship-operation components + research state and their space.txt costs.
// Main resets `page` when the selected command changes and binds the sidebar
// title to `title`. All values are sim-blocked until the space-program sim.

Item {
    id: panel
    required property var view

    property string page: "satellites"  // or "starship"
    readonly property string title: page === "starship" ? "Space Program: Starship"
                                                        : "Space Program: Satellites"

    // Content area; the bottom strip is reserved for the page-toggle button.
    Item {
        anchors.fill: parent
        anchors.bottomMargin: 24

        // Satellites page — label + indented operational status (green / red).
        Column {
            visible: panel.page === "satellites"
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 5
            Repeater {
                model: panel.view.spaceReport
                delegate: Column {
                    width: parent.width
                    spacing: 0
                    Text {
                        text: modelData.name + ":"
                        color: Theme.panelGreen
                        font.pixelSize: Theme.fontBody
                    }
                    Text {
                        text: "    " + (modelData.operational ? "Operational" : "Not Operational")
                        color: modelData.operational ? Theme.panelGreen : Theme.warnRed
                        font.pixelSize: Theme.fontBody
                    }
                }
            }
        }

        // Starship page — components + research state (red until done, then green).
        Column {
            visible: panel.page === "starship"
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 5
            Repeater {
                model: panel.view.starshipReport
                delegate: Column {
                    width: parent.width
                    spacing: 0
                    Text { text: modelData.name
                           color: modelData.done ? Theme.panelGreen : Theme.warnRed
                           font.pixelSize: Theme.fontBody }
                    Text { text: "    " + modelData.status
                           color: modelData.done ? Theme.panelGreen : Theme.warnRed
                           font.pixelSize: Theme.fontBody }
                    // Cost from space.txt (loaded into memory): Common / Rare ore + build.
                    Text { text: "    Cost: " + modelData.common + "c  " + modelData.rare +
                                 "r   Build: " + modelData.build
                           color: Theme.subGreen; font.pixelSize: Theme.fontSmall }
                }
            }
        }
    }

    // Bottom toggle button (STARSHIP ⇄ SATELLITES), OP2 red-bordered style.
    RedNavButton {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 2
        anchors.horizontalCenter: parent.horizontalCenter
        width: 120
        label: panel.page === "satellites" ? "STARSHIP" : "SATELLITES"
        onClicked: panel.page = (panel.page === "satellites" ? "starship" : "satellites")
    }
}
