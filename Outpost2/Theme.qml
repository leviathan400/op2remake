pragma Singleton
import QtQuick

// =============================================================================
// Theme  —  the OP2 palette + shared HUD metrics, in one place
// =============================================================================
//
// Single authority for the colors and dimensions the panels repeat. The greens
// are measured from the original UI: panelGreen (#00ff00) is OP2's report/
// panel text green; hudGreen (#7fbf4f) is the softer green of our hand-drawn
// frame chrome; subGreen/statusGreen are the dimmer second-line greens OP2
// uses for indented detail lines. Change a value here and every panel follows.

QtObject {
    // --- palette: OP2 panel text ------------------------------------------
    readonly property color panelGreen:    "#00ff00" // report/panel text (measured from OP2)
    readonly property color subGreen:      "#00b400" // dimmer indented sub-row text
    readonly property color statusGreen:   "#00cc00" // report status lines
    readonly property color hudGreen:      "#7fbf4f" // hand-drawn frame accent / headings
    readonly property color emptyGreen:    "#6a9a6a" // empty-state hints
    readonly property color footnoteGreen: "#5a7a5a" // tiny footnotes
    readonly property color grayText:      "#9aa0a6" // idle status / secondary text
    readonly property color lightText:     "#cccccc" // debug values / dev-button labels
    readonly property color warnRed:       "#ff3030" // warnings / Not Operational
    readonly property color statusRed:     "#d65555" // Disabled status / enemy owner
    readonly property color buttonGold:    "#d8b040" // OP2 long-button label text

    // OP2's red-bordered nav buttons (Resources / Space bottom navigation).
    readonly property color redBtnBg:     "#140000"
    readonly property color redBtnBorder: "#b03030"
    readonly property color redBtnText:   "#d05050"

    // Dev/debug buttons (deliberately NOT OP2-styled — they're developer tools).
    readonly property color devBtnBg:      "#2b2f36"
    readonly property color devBtnPressed: "#3a3f46"
    readonly property color devBtnBorder:  "#4a4f57"

    // --- HUD frame metrics (measured from the original OP2 UI) -------------
    readonly property int topBarHeight: 22
    readonly property int leftBarWidth: 12
    readonly property int sidebarWidth: 212
    readonly property int statusBarHeight: 42
    readonly property int minimapWidth: 192
    readonly property int minimapHeight: 128

    // --- shared control sizes ----------------------------------------------
    readonly property int gameButtonWidth: 175 // OP2 long button (ui/group50-*.png)
    readonly property int gameButtonHeight: 25
    readonly property int devButtonWidth: 96
    readonly property int devButtonHeight: 22

    // --- type scale (OP2 panel text is small and dense) ---------------------
    readonly property int fontSmall: 10
    readonly property int fontBody: 11
    readonly property int fontTitle: 12
    readonly property int fontHeading: 13
}
