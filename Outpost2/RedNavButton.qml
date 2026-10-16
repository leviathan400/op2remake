import QtQuick

// OP2's red-bordered navigation button (the Resources Report's STRUCTURES /
// DETAILS / SUMMARY buttons and the Space Program's STARSHIP ⇄ SATELLITES
// toggle). Width is overridable per use; the styling is the shared part.
Rectangle {
    id: btn
    property string label
    signal clicked()

    width: 84
    height: 16
    color: Theme.redBtnBg
    border.color: Theme.redBtnBorder

    Text {
        anchors.centerIn: parent
        text: btn.label
        color: Theme.redBtnText
        font.pixelSize: Theme.fontSmall
        font.bold: true
    }
    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: btn.clicked()
    }
}
