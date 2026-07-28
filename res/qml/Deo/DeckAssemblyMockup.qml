import QtQuick 2.12
import "../Theme"

// DECK ASSEMBLY MOCKUP -- not used by the real app (nothing imports or
// instantiates this file). Combines the sections that can safely run
// standalone into one canvas, each pinned to its real computed pixel size
// (see space_allocations.md for the formulas, worked out at a
// representative ~1600x1100 window), so alignment/proportions between
// sections can be checked visually in one place instead of piece by piece.
//
// CustomPadSection.qml and LoopSectionDesignMockup.qml show the real
// component -- neither has a `required property string group`, so both
// instantiate fine outside the live app. FXRack.qml, StemPads.qml,
// TransportRow.qml, and JogWheel.qml all DO require a real `group` from
// Mixxx.ControlProxy and can't run standalone, so those four slots are
// plain labeled placeholder frames sized to match.
//
// To swap a placeholder for the real thing: build a
// "<Section>DesignMockup.qml" the same way LoopSectionDesignMockup.qml was
// built (strip required/ControlProxy properties, replace with local fake
// state), then replace that slot's Rectangle below with
// `<Section>DesignMockup { }`.
Item {
    id: root

    width: 656
    height: 333

    // ---- Upper Deck ----
    Rectangle {
        id: upperDeck

        border.color: Theme.deckLineColor
        border.width: 1
        color: "#222222"
        height: 91
        width: root.width
        x: 0
        y: 0

        Text {
            anchors.centerIn: parent
            color: Theme.deckTextSecondary
            text: "Upper Deck (placeholder)"
        }
    }

    // ---- Lower Deck body ----
    Item {
        id: body

        height: root.height - upperDeck.height - 8
        width: root.width
        x: 0
        y: upperDeck.height + 8

        // -- DA column (Effects / Stem Pads) --
        Item {
            id: daColumn

            height: body.height
            width: 252
            x: 0
            y: 0

            Rectangle {
                id: daUp

                border.color: Theme.deckLineColor
                border.width: 1
                color: "#222222"
                height: 108
                width: parent.width
                x: 0
                y: 0

                Text {
                    anchors.centerIn: parent
                    color: Theme.deckTextSecondary
                    text: "FXRack (placeholder)"
                }
            }
            Rectangle {
                color: Theme.deckLineColor
                height: 1
                width: parent.width
                y: daUp.height + 8
            }
            Rectangle {
                id: daDown

                border.color: Theme.deckLineColor
                border.width: 1
                color: "#222222"
                height: 108
                width: parent.width
                x: 0
                y: daUp.height + 17

                Text {
                    anchors.centerIn: parent
                    color: Theme.deckTextSecondary
                    text: "StemPads (placeholder)"
                }
            }
        }

        // -- DB column (Custom Pads / Loop) --
        Item {
            id: dbColumn

            height: body.height
            width: 139
            x: daColumn.width + 8
            y: 0

            CustomPadSection {
                id: dbUp

                accentColor: "#3C7993"
                height: 108
                width: parent.width
                x: 0
                y: 0
            }
            Rectangle {
                color: Theme.deckLineColor
                height: 1
                width: parent.width
                y: dbUp.height + 8
            }
            LoopSectionDesignMockup {
                id: dbDown

                x: 0
                y: dbUp.height + 17
            }
        }

        // -- DC column (Transport / Jog wheel) --
        Item {
            id: dcColumn

            height: body.height
            width: 221
            x: dbColumn.x + dbColumn.width + 8
            y: 0

            Rectangle {
                id: dcTransport

                border.color: Theme.deckLineColor
                border.width: 1
                color: "#222222"
                height: 44
                width: parent.width
                x: 0
                y: 0

                Text {
                    anchors.centerIn: parent
                    color: Theme.deckTextSecondary
                    text: "TransportRow (placeholder)"
                }
            }
            Rectangle {
                border.color: Theme.deckLineColor
                border.width: 1
                color: "#222222"
                height: parent.height - dcTransport.height - 4
                width: parent.width
                x: 0
                y: dcTransport.height + 4

                Text {
                    anchors.centerIn: parent
                    color: Theme.deckTextSecondary
                    text: "JogWheel (placeholder)"
                }
            }
        }

        // -- DD column (Pitch fader) --
        Rectangle {
            border.color: Theme.deckLineColor
            border.width: 1
            color: "#222222"
            height: body.height
            width: 44
            x: dcColumn.x + dcColumn.width + 8
            y: 0

            Text {
                anchors.centerIn: parent
                color: Theme.deckTextSecondary
                rotation: -90
                text: "PitchFader"
            }
        }
    }
}
