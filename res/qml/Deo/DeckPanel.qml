import QtQuick 2.12
import QtQuick.Controls 2.12
import Mixxx 1.0 as Mixxx
import "." as Deo
import ".." as Skin
import "../Theme"

// Deck shell, rebuilt per the user's reference mockup (DeckDesignMockup.qml)
// as a plain Item with the mockup's exact literal coordinates on a fixed
// 656x333 authored canvas -- the same fixed-canvas + Scale technique
// already used for each of the sub-components below (FXRack, StemPads,
// CustomPadSection, LoopSection, TransportRow, PitchFader all already
// author their own fixed canvas matching THEIR OWN reference mockup
// slice exactly; this file previously arranged them with percentage/
// weight-based Layout splits instead of the whole-deck mockup's literal
// absolute positions, which is what this rewrite corrects). Two
// instances of this, mirrored left/right, make up the deck view.
Item {
    id: root

    required property string group
    required property string label
    required property color accentColor
    required property int effectUnitNumber
    property bool mirrored: false

    readonly property bool trackLoaded: trackLoadedControl.value > 0

    // No implicitWidth/implicitHeight mirroring `canvas`'s own size --
    // this panel is always instantiated inside a Layout context
    // (deckMixerRow in main.qml) with its own explicit
    // Layout.preferredWidth/minimumWidth/fillWidth/fillHeight, so its real
    // box should come from THAT external assignment alone.

    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -8
        color: Theme.deckPanelBackground
        z: -2
    }

    // Drag an audio file from Finder onto the panel to load it; the M1
    // spec explicitly leaves the library/browser out of scope, so this is
    // the load path for exercising the acceptance criteria.
    Mixxx.PlayerDropArea {
        anchors.fill: parent
        group: root.group
        z: -1
    }

    // Mirrors an absolute x/width pair across the canvas's own 656 width
    // for Deck B -- swapping DA/DB/DC/DD's left-to-right order without
    // geometrically flipping any child's own content (a Scale(-1)
    // transform on the whole canvas would also mirror every label's text
    // backwards, which LayoutMirroring never did and this must not
    // either).
    function mx(x, w) {
        return root.mirrored ? (canvas.width - x - w) : x;
    }

    // A single UNIFORM scale factor (not independent x/y like every
    // sub-component wrapper below), because this canvas contains a real
    // circle (the jog wheel) -- independent xScale/yScale stretches
    // rectangles harmlessly but visibly turns a circle into an oval the
    // instant the deck's real allocated aspect ratio drifts from the
    // mockup's 656:333 (confirmed: deck_section's width comes from a %
    // of the window's width, its height from an unrelated % of the
    // window's height, so an exact aspect-ratio match is not guaranteed).
    // min(), not a plain average or either axis alone, so the canvas
    // always fits fully inside root's box on both axes (never overflows
    // and gets clipped) -- any leftover space lands as a small letterbox
    // margin, invisible against the deckPanelBackground Rectangle already
    // painted behind everything.
    readonly property real fitScale: Math.min(root.width / 656, root.height / 333)

    Item {
        id: canvas

        // Fixed authored canvas, matching DeckDesignMockup.qml exactly --
        // scaled uniformly (see root.fitScale above) and centered to fit
        // whatever real box this panel is allocated, same technique as
        // every sub-component below except for the single shared scale
        // factor.
        width: 656
        height: 333
        clip: true

        anchors.centerIn: parent

        transform: Scale {
            origin.x: canvas.width / 2
            origin.y: canvas.height / 2
            xScale: root.fitScale
            yScale: root.fitScale
        }

        // Upper Deck: track-identity, BPM/Key, overview waveform, and
        // Elapsed/Remaining readout. Spans the full width at the top,
        // above the DA/DB/DC/DD row -- matches every sub-mockup below
        // starting at y=99 in DeckDesignMockup.qml.
        Deo.UpperDeck {
            x: 0
            y: 0
            width: canvas.width
            height: 99

            accentColor: root.accentColor
            group: root.group
            label: root.label
            mirrored: root.mirrored
        }

        // DA: Effects (top) / Pads (down), absolute (0,99,253,234) per
        // DeckDesignMockup.qml (effectsSectionMockup/padsDesignMockup, both
        // at local x=0). The 108/20/106 vertical split (FXRack's own
        // height / gap / StemPads' own height) is the mockup's own exact
        // numbers, not an even 50/50 guess.
        Item {
            id: fxRackWrapper

            x: root.mx(0, 253)
            y: 99
            width: 253
            height: 108
            clip: true

            Deo.FXRack {
                x: 0
                y: 0
                accentColor: root.accentColor
                group: root.group
                unitNumber: root.effectUnitNumber

                transform: Scale {
                    origin.x: 0
                    origin.y: 0
                    xScale: fxRackWrapper.width / 252
                    yScale: fxRackWrapper.height / 108
                }
            }
        }
        Rectangle {
            x: root.mx(0, 253)
            y: 99 + 108 + 9
            width: 253
            color: Theme.deckLineColor
            height: 1
        }
        Item {
            id: stemPadsWrapper

            x: root.mx(0, 253)
            y: 99 + 128
            width: 253
            height: 106
            clip: true

            Deo.StemPads {
                x: 0
                y: 0
                accentColor: root.accentColor
                group: root.group

                transform: Scale {
                    origin.x: 0
                    origin.y: 0
                    xScale: stemPadsWrapper.width / 252
                    yScale: stemPadsWrapper.height / 108
                }
            }
        }

        // DB: Custom pad section (top) / Loop section (down), absolute
        // (253,99,140,234) per DeckDesignMockup.qml
        // (customPadDesignMockup/customLoopSectionDesignMockup, both at
        // local x=253). Same 108/20/106 vertical split as DA.
        Item {
            id: customPadWrapper

            x: root.mx(253, 140)
            y: 99
            width: 140
            height: 108
            clip: true

            Deo.CustomPadSection {
                x: 0
                y: 0
                accentColor: root.accentColor
                group: root.group

                transform: Scale {
                    origin.x: 0
                    origin.y: 0
                    xScale: customPadWrapper.width / 139
                    yScale: customPadWrapper.height / 108
                }
            }
        }
        Rectangle {
            x: root.mx(253, 140)
            y: 99 + 108 + 9
            width: 140
            color: Theme.deckLineColor
            height: 1
        }
        Item {
            id: loopSectionWrapper

            x: root.mx(253, 140)
            y: 99 + 128
            width: 140
            height: 106
            clip: true

            Deo.LoopSection {
                x: 0
                y: 0
                accentColor: root.accentColor
                group: root.group

                transform: Scale {
                    origin.x: 0
                    origin.y: 0
                    xScale: loopSectionWrapper.width / 139
                    yScale: loopSectionWrapper.height / 108
                }
            }
        }

        // DC: jog wheel + transport row, absolute (393,99,219,234) per
        // DeckDesignMockup.qml's jogwheelDesignMockup (the fader section
        // nested inside it in that file is a Design-Studio reparenting
        // artifact -- its own local x=219 is exactly where DD starts
        // below, confirming DC's real width is 219, not its own declared
        // 221).
        Item {
            id: dcColumn

            // transportRowHeight: a flat 30, not the mockup's exact 25 --
            // already tried at 25 earlier this session and found too thin
            // to read comfortably at the deck's real allocated size (see
            // TransportRow's own history); this is a previously-settled
            // exception, not a fresh deviation. jogWheelHeight absorbs
            // whatever's left, same "one flexible element takes the
            // remainder" pattern as PitchFader's fader track.
            readonly property real transportRowHeight: 30
            readonly property real jogWheelHeight: dcColumn.height - dcColumn.transportRowHeight - 4

            x: root.mx(393, 219)
            y: 99
            width: 219
            height: 234
            clip: true

            Item {
                id: jogWheelArea

                x: 0
                y: 0
                width: dcColumn.width
                height: dcColumn.jogWheelHeight

                Deo.JogWheel {
                    anchors.fill: parent
                    anchors.bottomMargin: 8
                    accentColor: root.accentColor
                    group: root.group
                    vinylMode: vinylToggle.checked
                }
                // Eject + settings gear sit at the wheel's top-LEFT
                // normally, top-RIGHT when mirrored (Deck B) --
                // LayoutMirroring used to swap this automatically via
                // inherited anchors; now done explicitly since this Item
                // no longer sits inside a LayoutMirroring-enabled Layout.
                // The gear (per JogwheelDesignMockup.qml's own
                // gearSvgrepoCom, drawn directly under its eject icon)
                // opens a Jog Behavior menu -- CD Mode/Vinyl Mode is the
                // one real, already-wired option (the same state the V/S
                // toggle column already exposes); the other entries a
                // typical DJ app's jog settings menu has (auto-BPM/KEY
                // match, pitch range, etc.) have no equivalent real
                // Mixxx control here, so they're deliberately not
                // invented.
                Column {
                    anchors.top: parent.top
                    anchors.left: root.mirrored ? undefined : parent.left
                    anchors.right: root.mirrored ? parent.right : undefined
                    anchors.margins: 10
                    spacing: 3
                    z: 2

                    Skin.ControlButton {
                        activeColor: root.accentColor
                        group: root.group
                        height: 20
                        key: "eject"
                        text: "⏏"
                        width: 20
                    }
                    Skin.Button {
                        id: jogSettingsButton

                        height: 20
                        text: "⚙"
                        width: 20

                        onClicked: jogSettingsMenu.popup(jogSettingsButton)
                    }
                }
                Menu {
                    id: jogSettingsMenu

                    MenuItem {
                        checkable: true
                        checked: !vinylToggle.checked
                        text: "CD Mode (nudge)"

                        onTriggered: vinylToggle.checked = false
                    }
                    MenuItem {
                        checkable: true
                        checked: vinylToggle.checked
                        text: "Vinyl Mode (scratch)"

                        onTriggered: vinylToggle.checked = true
                    }
                }
                Column {
                    anchors.top: parent.top
                    anchors.right: root.mirrored ? undefined : parent.right
                    anchors.left: root.mirrored ? parent.left : undefined
                    anchors.margins: 10
                    spacing: 3
                    z: 2

                    Skin.Button {
                        id: vinylToggle

                        activeColor: root.accentColor
                        checkable: true
                        checked: true
                        implicitHeight: 16
                        implicitWidth: 22
                        text: "V"
                    }
                    Skin.ControlButton {
                        activeColor: root.accentColor
                        group: root.group
                        implicitHeight: 16
                        implicitWidth: 22
                        key: "slip_enabled"
                        text: "S"
                        toggleable: true
                    }
                }
            }
            Item {
                id: transportRowWrapper

                x: 0
                y: dcColumn.jogWheelHeight + 4
                width: dcColumn.width
                height: dcColumn.transportRowHeight
                clip: true

                Deo.TransportRow {
                    x: 0
                    y: 0
                    accentColor: root.accentColor
                    group: root.group

                    // /34, not /25 -- TransportRow's own authored canvas
                    // height was grown from 25 to 34 earlier this session
                    // for legibility (see TransportRow.qml's own header),
                    // but this divisor was never updated to match, so
                    // every button/text inside was being scaled by the
                    // wrong factor -- the actual cause of CUE/PLAY/SYNC's
                    // text sitting off-center in their boxes.
                    transform: Scale {
                        origin.x: 0
                        origin.y: 0
                        xScale: transportRowWrapper.width / 225
                        yScale: transportRowWrapper.height / 34
                    }
                }
            }
        }

        // DD: pitch bend slider section, absolute (612,99,44,234) per
        // DeckDesignMockup.qml's faderSectionDesignMockup (nested inside
        // jogwheelDesignMockup at local x=219 -- 393+219=612, and
        // 612+44=656 lands exactly on the canvas's own right edge).
        Item {
            id: pitchFaderWrapper

            x: root.mx(612, 44)
            y: 99
            width: 44
            height: 234
            clip: true

            Deo.PitchFader {
                x: 0
                y: 0
                accentColor: root.accentColor
                group: root.group

                transform: Scale {
                    origin.x: 0
                    origin.y: 0
                    xScale: pitchFaderWrapper.width / 44
                    yScale: pitchFaderWrapper.height / 234
                }
            }
        }
    }
}
