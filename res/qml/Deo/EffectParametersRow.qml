import QtQuick 2.12
import Mixxx 1.0 as Mixxx
import ".." as Skin

// Shared expanded-parameters strip for the FX rack's gear column: shows
// individual parameterK controls for whichever slot's gear is toggled on.
// Adapted from stock EffectSlot.qml's parametersView, which normally
// lives inline per-slot; here it's one shared row below the 3-column
// block instead, since the columns no longer bundle each slot's controls
// together.
Item {
    id: root

    property Mixxx.EffectSlotProxy slot: null
    required property color accentColor

    implicitHeight: 50

    ListView {
        id: parametersView

        anchors.fill: parent
        clip: true
        model: root.slot ? root.slot.parametersModel : null
        orientation: ListView.Horizontal
        spacing: 5

        delegate: Item {
            id: parameter

            required property int index
            required property string shortName
            required property string name
            required property string controlKey
            required property int type
            property string label: shortName || name
            property bool isKnob: type == 0
            property bool isButton: type == 1

            height: 50
            width: 50

            Skin.EmbeddedText {
                anchors.fill: parent
                font.bold: false
                text: parameter.label
                verticalAlignment: Text.AlignBottom
            }
            Skin.ControlMiniKnob {
                anchors.centerIn: parent
                arcStart: 0
                color: root.accentColor
                group: root.slot ? root.slot.group : ""
                height: 30
                key: parameter.controlKey
                visible: parameter.isKnob
                width: 30
            }
            Skin.ControlButton {
                anchors.centerIn: parent
                activeColor: root.accentColor
                group: root.slot ? root.slot.group : ""
                height: 22
                key: parameter.controlKey
                text: "ON"
                toggleable: true
                visible: parameter.isButton
                width: parent.width
            }
        }
    }
}
