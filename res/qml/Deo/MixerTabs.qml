import QtQuick 2.12
import QtQuick.Layouts
import "." as Deo
import ".." as Skin

// M5: AUDIO/VIDEO/SCRATCH/MASTER tab shell. Only AUDIO and MASTER have
// real content; VIDEO is disabled (blocked on M12 — no video engine
// exists) and SCRATCH is out of scope entirely per discussion. StackLayout
// keeps every tab's contents alive rather than recreating them via a
// Loader, so switching away and back doesn't lose control state
// (acceptance criterion #6).
ColumnLayout {
    id: root

    required property color accentColorA
    required property color accentColorB

    spacing: 6

    RowLayout {
        Layout.fillWidth: true
        spacing: 0

        Skin.Button {
            Layout.fillWidth: true
            checkable: true
            checked: tabStack.currentIndex === 0
            text: "AUDIO"

            onClicked: tabStack.currentIndex = 0
        }
        Skin.Button {
            Layout.fillWidth: true
            checkable: true
            checked: tabStack.currentIndex === 1
            enabled: false
            opacity: 0.4
            text: "VIDEO"
        }
        Skin.Button {
            Layout.fillWidth: true
            checkable: true
            checked: tabStack.currentIndex === 2
            enabled: false
            opacity: 0.4
            text: "SCRATCH"
        }
        Skin.Button {
            Layout.fillWidth: true
            checkable: true
            checked: tabStack.currentIndex === 3
            text: "MASTER"

            onClicked: tabStack.currentIndex = 3
        }
    }
    StackLayout {
        id: tabStack

        Layout.fillWidth: true
        Layout.fillHeight: true
        currentIndex: 0

        Deo.AudioMixerPanel {
            accentColorA: root.accentColorA
            accentColorB: root.accentColorB
        }
        Item {
            // VIDEO: blocked on M12, no video engine exists.
        }
        Item {
            // SCRATCH: out of scope, not a defined feature.
        }
        Deo.MasterPanel {
        }
    }
}
