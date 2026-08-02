import QtQuick 2.12
import QtQuick.Layouts
import ".." as Skin

// M4/pad-bank-switching: the Hotcues bank's content -- one hotcue pad per
// physical slot (hotcues 1-N; Mixxx supports up to kMaxNumberOfHotcues=36,
// but N is tied to whichever slot this bank is shown in -- 8 for the wide
// slot, 4 for the narrow one, same as the stock skin's own
// Deck/HotcueAndStem.qml hotcue grid uses 8). Skin.HotcueButton is a fully
// self-contained, already-built primitive (color/rename/clear popup
// included) -- no new ControlProxy wiring needed here at all.
//
// 8 hand-written instances rather than `Repeater{model:8}` -- a
// property-bound-and-even-literal Repeater model both left this
// particular delegate's `index` context property undefined for reasons
// that didn't resolve after several different fixes aimed at the
// delegate itself (removing its Popup child, declaring `index` as a
// required property directly or via a wrapper Item). Hand-written literal
// pads is exactly the pattern StemsBankContent.qml already uses
// successfully for its own (non-uniform) 8 pads -- more verbose, but
// never depends on Repeater's `index` at all.
Item {
    id: root

    required property string group
    required property int columns
    required property int padCount

    GridLayout {
        anchors.fill: parent
        columns: root.columns
        columnSpacing: 6
        rowSpacing: 6

        Skin.HotcueButton {
            Layout.column: 0 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(0 / root.columns)
            group: root.group
            hotcueNumber: 1
            visible: 0 < root.padCount
        }
        Skin.HotcueButton {
            Layout.column: 1 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(1 / root.columns)
            group: root.group
            hotcueNumber: 2
            visible: 1 < root.padCount
        }
        Skin.HotcueButton {
            Layout.column: 2 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(2 / root.columns)
            group: root.group
            hotcueNumber: 3
            visible: 2 < root.padCount
        }
        Skin.HotcueButton {
            Layout.column: 3 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(3 / root.columns)
            group: root.group
            hotcueNumber: 4
            visible: 3 < root.padCount
        }
        Skin.HotcueButton {
            Layout.column: 4 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(4 / root.columns)
            group: root.group
            hotcueNumber: 5
            visible: 4 < root.padCount
        }
        Skin.HotcueButton {
            Layout.column: 5 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(5 / root.columns)
            group: root.group
            hotcueNumber: 6
            visible: 5 < root.padCount
        }
        Skin.HotcueButton {
            Layout.column: 6 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(6 / root.columns)
            group: root.group
            hotcueNumber: 7
            visible: 6 < root.padCount
        }
        Skin.HotcueButton {
            Layout.column: 7 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(7 / root.columns)
            group: root.group
            hotcueNumber: 8
            visible: 7 < root.padCount
        }
    }
}
