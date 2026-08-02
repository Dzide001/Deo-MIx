import QtQuick 2.12
import QtQuick.Layouts
import "." as Deo

// M4/pad-bank-switching: the Sampler bank's content -- "basic playback"
// scope only (per explicit user decision). Bound to Mixxx's GLOBAL
// `[Sampler1]`.."[SamplerN]"` groups, NOT derived from `root.group` the
// way Stems/Hotcues are -- samplers are a shared resource, so this shows
// the exact same slots regardless of which deck's dropdown is currently
// displaying this bank (mirroring res/qml/SamplerRow.qml's own
// `"[Sampler" + (index + 1) + "]"` pattern). N is tied to whichever
// physical slot this bank is shown in (8 wide, 4 narrow), same as every
// other bank.
//
// Only kSamplerCount=4 real samplers exist by default
// (src/coreservices.cpp) -- in the 8-pad slot, pads 5-8 bind to groups
// with no backing ControlObjects yet, which is safe (QmlControlProxy's
// AllowMissingOrInvalid fallback) and simply renders those pads
// permanently disabled/empty rather than crashing, same as
// SamplerRow.qml already assumes. In the 4-pad slot, all 4 shown samplers
// are real by default.
//
// 8 hand-written instances rather than `Repeater{model:8}` -- see
// HotcuesBankContent.qml's own comment on why: a Repeater delegate's
// required properties intermittently failed to initialize ("Cannot
// create delegate") -- a real, if intermittent, race, not something
// specific to one file. Hand-written literal pads matches
// StemsBankContent.qml's own proven pattern and sidesteps it entirely.
Item {
    id: root

    required property int columns
    required property int padCount

    GridLayout {
        anchors.fill: parent
        columns: root.columns
        columnSpacing: 6
        rowSpacing: 6

        Deo.SamplerPad {
            Layout.column: 0 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(0 / root.columns)
            group: "[Sampler1]"
            samplerNumber: 1
            visible: 0 < root.padCount
        }
        Deo.SamplerPad {
            Layout.column: 1 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(1 / root.columns)
            group: "[Sampler2]"
            samplerNumber: 2
            visible: 1 < root.padCount
        }
        Deo.SamplerPad {
            Layout.column: 2 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(2 / root.columns)
            group: "[Sampler3]"
            samplerNumber: 3
            visible: 2 < root.padCount
        }
        Deo.SamplerPad {
            Layout.column: 3 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(3 / root.columns)
            group: "[Sampler4]"
            samplerNumber: 4
            visible: 3 < root.padCount
        }
        Deo.SamplerPad {
            Layout.column: 4 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(4 / root.columns)
            group: "[Sampler5]"
            samplerNumber: 5
            visible: 4 < root.padCount
        }
        Deo.SamplerPad {
            Layout.column: 5 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(5 / root.columns)
            group: "[Sampler6]"
            samplerNumber: 6
            visible: 5 < root.padCount
        }
        Deo.SamplerPad {
            Layout.column: 6 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(6 / root.columns)
            group: "[Sampler7]"
            samplerNumber: 7
            visible: 6 < root.padCount
        }
        Deo.SamplerPad {
            Layout.column: 7 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(7 / root.columns)
            group: "[Sampler8]"
            samplerNumber: 8
            visible: 7 < root.padCount
        }
    }
}
