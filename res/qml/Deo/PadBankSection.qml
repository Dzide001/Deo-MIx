import QtQuick 2.12
import QtQuick.Controls 2.12
import "." as Deo
import ".." as Skin
import "../Theme"

// M4/pad-bank-switching: the shared shell both decks' pad areas now use,
// replacing the old fixed-per-deck StemPads.qml (Deck A)/
// CustomPadSection.qml (Deck B) split. Each instance independently switches
// between 4 banks -- Stems, Hotcues, Sampler, Custom -- via the dropdown
// below, which used to exist here as a permanent, non-functional
// single-entry placeholder (see StemPads.qml's git history) explicitly
// left in for this exact feature.
//
// DeckPanel.qml hosts one instance per deck at two DIFFERENT widths (Deck
// A's column is 253 wide, Deck B's is 140 -- an asymmetry baked into the
// deck's own original mockup design, not something this component should
// try to change). Rather than the old fixed-252-wide-canvas-plus-Scale-
// transform trick (which would non-uniformly squish an 8-pad grid designed
// for 252px down to 139px, distorting pad aspect ratio), this component
// binds directly to its own real assigned width/height and reflows its
// bank content's internal grid between 4 columns (wide side) and 2
// columns (narrow side) accordingly -- see `columns` below.
//
// Only the CURRENTLY SELECTED bank's content is ever actually instantiated
// (via the Loader below), not all 4 kept alive simultaneously behind
// visibility toggling. With both decks' two pad-bank instances each
// always instantiating all 4 banks' pads up front, this app was creating
// ~40 Skin.ControlButton-derived components (StemsBankContent's mute
// pads, HotcuesBankContent's hotcue pads, SamplerBankContent's sampler
// pads, CustomBankContent's assignable pads, x2 decks x2 slots) in one
// synchronous wave at startup -- Qt Quick's required-property
// initialization didn't reliably complete for all of them at that volume
// ("Required property group was not initialized", which cascaded into
// the entire main.qml failing to load). Switching banks now destroys/
// recreates content instead of just toggling visibility -- an accepted
// tradeoff (these are simple, cheap-to-rebuild control bindings, not
// expensive state) in exchange for the app actually starting reliably.
Item {
    id: root

    required property string group
    required property color accentColor
    // Tied to which physical slot this instance is hosted in (8 for the
    // wide slot next to FX, 4 for the narrow slot next to LOOP) -- fixed
    // per slot, independent of which bank is selected there. Every bank
    // respects it (Stems shows only its 4 real per-stem mute pads,
    // Hotcues/Sampler/Custom show fewer slots), so the narrow slot keeps
    // matching its pre-unification 4-pad scope no matter which bank is
    // showing there.
    required property int padCount
    // Default bank matches this slot's pre-unification identity -- the
    // wide slot used to always show Stems, the narrow one always showed
    // Custom pads (CustomPadSection.qml), so each keeps that as its
    // starting bank rather than both defaulting to Stems. Still a plain
    // mutable property (0=Stems 1=Hotcues 2=Sampler 3=Custom): the
    // dropdown's onActivated below assigns a concrete value that
    // overrides this initial binding as soon as the user picks a
    // different bank, same as any other QML property with a default
    // binding.
    property int currentBank: root.padCount >= 8 ? 0 : 3

    readonly property real labelWidth: 18
    readonly property real contentX: 32
    readonly property real contentY: 38

    // 4 across when there's genuinely room for it (Deck A's 253-wide
    // column), 2 across otherwise (Deck B's 140-wide column) -- computed
    // from this component's own real width rather than assuming one fixed
    // canvas size.
    readonly property int columns: (root.width - root.contentX) >= 220 ? 4 : 2

    ListModel {
        id: bankModel

        ListElement {
            display: "Stems"
        }
        ListElement {
            display: "Hotcues"
        }
        ListElement {
            display: "Sampler"
        }
        ListElement {
            display: "Custom"
        }
    }

    // Rotated "PADS" label on the left edge, matching the FX/LOOP vertical
    // labels elsewhere in the deck -- unchanged from the old StemPads.qml.
    Item {
        height: root.height
        width: root.labelWidth
        x: 8
        y: 0

        Label {
            anchors.centerIn: parent
            color: Theme.deckTextSecondary
            font.bold: true
            font.family: Theme.fontFamily
            font.pixelSize: 10
            rotation: -90
            text: "PADS"
        }
    }
    // The bank-switcher dropdown, now live -- same position/size/font/
    // border-compensation as the old decorative placeholder (see
    // StemPads.qml's own comment on the +8/-4 inset trick: Skin.ComboBox's
    // background Rectangle insets itself 4px from the control's declared
    // box, so this compensates so the VISIBLE pill lands on the original
    // mockup's coordinates).
    Skin.ComboBox {
        currentIndex: root.currentBank
        font.pixelSize: 9
        height: 17 + 8
        model: bankModel
        textRole: "display"
        width: 102 + 8
        x: root.contentX - 4
        y: 15 - 4

        onActivated: index => root.currentBank = index
    }

    Item {
        id: contentArea

        height: root.height - root.contentY
        width: root.width - root.contentX - 8
        x: root.contentX
        y: root.contentY

        Loader {
            anchors.fill: parent
            sourceComponent: {
                switch (root.currentBank) {
                case 0:
                    return stemsComponent;
                case 1:
                    return hotcuesComponent;
                case 2:
                    return samplerComponent;
                case 3:
                    return customComponent;
                default:
                    return null;
                }
            }
        }
    }
    Component {
        id: stemsComponent

        Deo.StemsBankContent {
            accentColor: root.accentColor
            columns: root.columns
            group: root.group
            padCount: root.padCount
        }
    }
    Component {
        id: hotcuesComponent

        Deo.HotcuesBankContent {
            columns: root.columns
            group: root.group
            padCount: root.padCount
        }
    }
    Component {
        id: samplerComponent

        Deo.SamplerBankContent {
            columns: root.columns
            padCount: root.padCount
        }
    }
    Component {
        id: customComponent

        Deo.CustomBankContent {
            accentColor: root.accentColor
            columns: root.columns
            group: root.group
            padCount: root.padCount
        }
    }
}
