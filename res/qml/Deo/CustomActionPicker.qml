import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts
import ".." as Skin
import "../Theme"

// M4/pad-bank-switching: the Custom bank's assignment picker -- a plain
// pick-from-a-list popup (see CuratedActions.qml's own comment on why this
// isn't a live "click to learn" system). Opened by CustomPad.qml on
// long-press/right-click; emits actionPicked() with the chosen entry's
// full data so the caller can persist it via
// Mixxx.CustomPadSettings.setPadAssignment().
//
// Skin.ActionPopup's `default property alias children: content.children`
// (see ActionPopup.qml) puts everything declared here straight into its
// own internal ColumnLayout, so the ListView and Clear button below just
// need Layout.fillWidth -- no manual anchoring/stacking needed, the
// ColumnLayout stacks them top-to-bottom on its own.
Skin.ActionPopup {
    id: root

    // Supplied by the caller (CustomPad.qml, which already owns its own
    // CuratedActions instance for a separate lookup) rather than
    // instantiated here -- a plain ListModel isn't an Item, so it can't be
    // placed inside ActionPopup's own `children` (an Item-only list
    // property aliased to its internal ColumnLayout's children).
    required property var actionsModel

    signal actionPicked(string group, string key, string label, bool toggleable)
    signal clearRequested

    facing: Skin.ActionPopup.Facing.Top
    focus: true
    padding: 6
    width: 180

    ListView {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(contentHeight, 220)
        clip: true
        model: root.actionsModel

        delegate: ItemDelegate {
            required property string label
            required property string key
            required property bool toggleable
            required property string group

            highlighted: hovered
            text: label
            width: ListView.view.width

            onClicked: {
                root.actionPicked(group, key, label, toggleable);
                root.close();
            }
        }
    }
    Skin.Button {
        Layout.fillWidth: true
        text: "Clear"

        onClicked: {
            root.clearRequested();
            root.close();
        }
    }
}
