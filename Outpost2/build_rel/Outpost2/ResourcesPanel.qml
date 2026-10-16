import QtQuick

// =============================================================================
// ResourcesPanel — OP2's Resources Report (command index 2, the Truck button)
// =============================================================================
//
// A small navigator: the Summary's lines drill down into per-category Status
// pages (click "Colonists" / "Morale Level"), and each Status has a STRUCTURES
// list reached via the red bottom-nav buttons. The panel OWNS its sub-page
// state (`page`); Main resets it to "summary" when the selected command
// changes (OP2 behaviour) and binds the sidebar title to `title`.
//
// Live now: mine/smelter counts (Summary), Nursery/University/GORF operational
// + structure Active/Idle status. The rest is sim-blocked (0/placeholder)
// until the economy sim. Spec: docs/COMMAND_PANELS.md.

Item {
    id: panel
    required property var view

    // Current page: "summary", "colonists", "colonistStruct", "morale", "moraleStruct".
    property string page: "summary"
    readonly property var titles: ({
        "summary": "Resources Report",
        "colonists": "Colonists Status (Alt+C)",
        "colonistStruct": "Colonists Structures",
        "morale": "Morale Status (Alt+M)",
        "moraleStruct": "Morale Structures"
    })
    readonly property string title: titles[page]

    readonly property var rr: view.resourcesReport
    readonly property var cr: view.colonistsReport
    readonly property var mr: view.moraleReport

    // The rows for the current page. Summary rows with `go` drill down on click.
    readonly property var rows:
          page === "colonists" ? colonistRows
        : page === "morale"    ? moraleRows
        : summaryRows

    readonly property var summaryRows: [
        { k: "main", l: "Colonists",            v: rr.colonists, go: "colonists" },
        { k: "sub",  l: "Available Workers",    v: rr.workers },
        { k: "sub",  l: "Available Scientists", v: rr.scientists },
        { k: "gap" },
        { k: "main", l: "Morale Level",         v: rr.morale, go: "morale" },
        { k: "sub",  l: "Trend",                v: "(" + rr.trend + ")" },
        { k: "gap" },
        { k: "main", l: "Power Use",            v: rr.powerUsed + "/" + rr.powerMax },
        { k: "sub",  l: "Unpowered Structures", v: rr.unpowered },
        { k: "gap" },
        { k: "main", l: "Food Stored",          v: rr.foodStored },
        { k: "sub",  l: "Production",           v: rr.foodProduction },
        { k: "gap" },
        { k: "main", l: "Common Metals",        v: rr.commonMetals },
        { k: "sub2", l: "Mines:" + rr.commonMines, r: "Smelters:" + rr.commonSmelters },
        { k: "gap" },
        { k: "main", l: "Rare Metals",          v: rr.rareMetals },
        { k: "sub2", l: "Mines:" + rr.rareMines,   r: "Smelters:" + rr.rareSmelters }
    ]
    readonly property var colonistRows: [
        { k: "main", l: "Total Population",         v: cr.totalPopulation },
        { k: "main", l: "Total Residence Capacity", v: cr.residenceCapacity },
        { k: "gap" },
        { k: "main", l: "Children",                 v: cr.children },
        { k: "gap" },
        { k: "main", l: "Workers",                  v: cr.workers },
        { k: "sub",  l: "Assigned",                 v: cr.workersAssigned + "/" + cr.workersAssignedMax },
        { k: "sub",  l: "Available",                v: cr.workersAvailable },
        { k: "gap" },
        { k: "main", l: "Scientists",               v: cr.scientists },
        { k: "sub",  l: "Assigned as Scientists",   v: cr.sciAsScientists + "/" + cr.sciAsScientistsMax },
        { k: "sub",  l: "Assigned as Workers",      v: cr.sciAsWorkers },
        { k: "sub",  l: "Assigned to Research",     v: cr.sciToResearch },
        { k: "sub",  l: "Available",                v: cr.sciAvailable }
    ]
    readonly property var moraleRows: [
        { k: "main", l: "Residence Demand",      v: mr.residenceDemand + "%" },
        { k: "main", l: "Net Food Production",   v: mr.netFood },
        { k: "main", l: "Disabled Bldg. Ratio",  v: mr.disabledRatio + "%" },
        { k: "main", l: "Unoccupied Colonists",  v: mr.unoccupied + "%" },
        { k: "main", l: "Scientists as Workers", v: mr.sciAsWorkers + "%" },
        { k: "main", l: "Rec. Facility Demand",  v: mr.recDemand + "%" },
        { k: "main", l: "Med. Center Demand",    v: mr.medDemand + "%" },
        { k: "gap" },
        { k: "op",   l: "Nursery",    on: mr.nurseryOp },
        { k: "op",   l: "University", on: mr.universityOp },
        { k: "op",   l: "GORF",       on: mr.gorfOp },
        { k: "gap" },
        { k: "main", l: "Disaster Warning Systems", v: mr.disasterWarn },
        { k: "main", l: "DIRT: Avg Damage Prev.",   v: mr.dirtPrevent + "%" }
    ]

    // Content area; the bottom strip is reserved for the red nav buttons.
    Item {
        id: content
        anchors.fill: parent
        anchors.bottomMargin: 24

        // Morale level header (morale page only): "<level> (<trend>)  <n>" + bar.
        Column {
            id: moraleHeader
            visible: panel.page === "morale"
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 2
            Item {
                width: parent.width; height: 13
                Text { anchors.left: parent.left
                       text: panel.mr.level + " (" + panel.mr.trend + ")"
                       color: Theme.panelGreen; font.pixelSize: Theme.fontBody }
                Text { anchors.right: parent.right
                       text: panel.mr.morale
                       color: Theme.panelGreen; font.pixelSize: Theme.fontBody }
            }
            Rectangle {
                width: parent.width; height: 6; color: "#1c1c1c"; border.color: "#2a2a2a"
                Rectangle { anchors.left: parent.left; anchors.top: parent.top
                            anchors.bottom: parent.bottom; anchors.margins: 1
                            width: (parent.width - 2) * Math.max(0, Math.min(1, panel.mr.morale / 100))
                            color: Theme.panelGreen }
            }
            Item { width: 1; height: 4 }
        }

        // Rows view (summary / colonists / morale). Summary rows with `go` drill down.
        Column {
            id: rowsView
            visible: panel.page !== "moraleStruct" && panel.page !== "colonistStruct"
            anchors.top: panel.page === "morale" ? moraleHeader.bottom : parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 1
            Repeater {
                model: panel.rows
                delegate: Item {
                    width: rowsView.width
                    height: modelData.k === "gap" ? 5 : 13
                    // Operational status line (green Operational / red Not).
                    Text {
                        visible: modelData.k === "op"
                        anchors.left: parent.left
                        text: (modelData.l || "") + " " +
                              (modelData.on ? "Operational" : "Not Operational")
                        color: modelData.on ? Theme.panelGreen : Theme.warnRed
                        font.pixelSize: Theme.fontBody
                    }
                    // Label (main/sub/sub2).
                    Text {
                        visible: modelData.k === "main" || modelData.k === "sub" || modelData.k === "sub2"
                        anchors.left: parent.left
                        anchors.leftMargin: modelData.k === "main" ? 0 : 14
                        text: modelData.l || ""
                        color: modelData.k === "main" ? Theme.panelGreen : Theme.subGreen
                        font.pixelSize: Theme.fontBody
                    }
                    // Right value.
                    Text {
                        visible: modelData.k === "main" || modelData.k === "sub" || modelData.k === "sub2"
                        anchors.right: parent.right
                        text: modelData.k === "sub2" ? (modelData.r || "")
                                                     : (modelData.v !== undefined ? modelData.v : "")
                        color: modelData.k === "main" ? Theme.panelGreen : Theme.subGreen
                        font.pixelSize: Theme.fontBody
                    }
                    // Drill-down: a Summary main row with `go` opens that Status page.
                    MouseArea {
                        anchors.fill: parent
                        enabled: modelData.go !== undefined
                        cursorShape: Qt.PointingHandCursor
                        onClicked: panel.page = modelData.go
                    }
                }
            }
        }

        // Structures lists (the STRUCTURES drill-down) — buildings with their LIVE
        // operating status; double-click an entry to focus + select it. Shared by
        // the Colonists and Morale structure pages (model switches on the page).
        ListView {
            visible: panel.page === "moraleStruct" || panel.page === "colonistStruct"
            anchors.fill: parent
            clip: true
            spacing: 6
            model: panel.page === "colonistStruct" ? panel.view.colonistStructures
                                                   : panel.view.moraleStructures
            delegate: Item {
                width: ListView.view ? ListView.view.width : 0
                height: msCol.height + 2
                Rectangle { anchors.fill: parent; color: "#0c2a0c"
                            border.color: "#1f5f1f"; visible: msMouse.containsMouse }
                Column {
                    id: msCol
                    y: 1; width: parent.width; spacing: 1
                    Text { width: parent.width; text: modelData.title; color: Theme.panelGreen
                           font.pixelSize: Theme.fontBody; font.bold: true; elide: Text.ElideRight }
                    Text { text: "    " + modelData.status
                           color: modelData.warn ? Theme.warnRed : Theme.panelGreen
                           font.pixelSize: Theme.fontBody }
                }
                MouseArea {
                    id: msMouse; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onDoubleClicked: {
                        panel.view.focusUnit(modelData.idx, modelData.gen)
                        panel.closeRequested()
                    }
                }
            }
        }
    }

    // Focusing an object closes the report so the selection panel shows (Main handles it).
    signal closeRequested()

    // Bottom nav (OP2's red-bordered buttons), contextual to the current page:
    // a Status page offers STRUCTURES + SUMMARY; a Structures list offers DETAILS
    // (back to the status) + SUMMARY. The Summary itself has none — you drill in
    // by clicking its lines (Colonists / Morale Level).
    Row {
        visible: panel.page !== "summary"
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 2
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 6
        Repeater {
            model: panel.page === "morale"         ? [ { label: "STRUCTURES", go: "moraleStruct" },   { label: "SUMMARY", go: "summary" } ]
                 : panel.page === "moraleStruct"   ? [ { label: "DETAILS",    go: "morale" },         { label: "SUMMARY", go: "summary" } ]
                 : panel.page === "colonists"      ? [ { label: "STRUCTURES", go: "colonistStruct" }, { label: "SUMMARY", go: "summary" } ]
                 : panel.page === "colonistStruct" ? [ { label: "DETAILS",    go: "colonists" },      { label: "SUMMARY", go: "summary" } ]
                 : /* fallback */                    [ { label: "SUMMARY",    go: "summary" } ]
            delegate: RedNavButton {
                label: modelData.label
                onClicked: panel.page = modelData.go
            }
        }
    }
}
