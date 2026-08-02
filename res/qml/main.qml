import "Deo" as Deo
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "Theme"

// M1: jog wheel (VINYL/SLIP) + CUE/Play/SYNC. M2 adds the pitch fader and
// loop section. M3 adds the per-deck FX rack. M5 adds the mixer (AUDIO +
// MASTER tabs; VIDEO disabled pending M12, SCRATCH out of scope). Pads
// and the library/browser are still out of scope until later milestones.
ApplicationWindow {
    id: root

    // minimumWidth/minimumHeight must be at least the sum of the deepest
    // Layout.minimumWidth/Height floors below (deck+mixer row: 400+400+400
    // + 2*24 spacing = 1248, plus 24 outer margins = 1272; three stacked
    // rows: 110+300+250 + 2*12 spacing + 24 margins = 708) -- Qt Quick
    // Layouts does not shrink children below their own minimums, it lets
    // them overflow past the allocated space (or, for a component whose
    // OWN declared minimum was set too low relative to its real internal
    // content, paint past its allocated slot into a neighbor) instead.
    // Also starts Maximized rather than a fixed 1600x1100 in plain
    // Windowed mode, so the UI always fills whatever screen space is
    // actually available instead of a size that can exceed it.
    color: Theme.backgroundColor
    height: 1100
    minimumHeight: 720
    minimumWidth: 1300
    visible: true
    visibility: Mixxx.Config.configStartInFullscreenKey ? Window.FullScreen : Window.Maximized
    width: 1600

    Mixxx.ControlProxy {
        group: "[App]"
        key: "num_decks"

        onInitializedChanged: {
            value = 2;
        }
    }

    // M12 Stage 3e/4b: a real, separate top-level Window (see
    // VideoPreviewPanel.qml), not an item embedded in this window's own
    // layout -- so it can never trigger a Layout re-flow here, and the
    // user can resize/fullscreen/move it to another monitor independently
    // of this window using the OS's own native window chrome.
    Deo.VideoPreviewPanel {
    }
    // M14 Stage 6: same real-top-level-Window pattern as
    // VideoPreviewPanel.qml above -- visible only while karaoke mode is
    // on (see the window's own `visible` binding).
    Deo.KaraokeDisplayWindow {
    }

    ColumnLayout {
        id: rootColumn

        // availableForPercentRows: root.height minus everything that
        // ISN'T part of the 14/47/36 percentage split below -- the 24px
        // outer margins (12 top + 12 bottom), the four 12px spacings
        // between these five rows (48px, since M14's LyricDisplay row was
        // inserted between TopBar and the waveform row), and TopBar's own
        // fixed height (26, not the original 32 -- trimmed leaner per
        // explicit user request to match a compact single-row reference
        // header). The percentage caps two blocks down previously summed
        // to just 97% of raw root.height with none of that fixed overhead
        // subtracted first, so the four rows structurally always demanded
        // more height than the window actually had -- squeeze pressure
        // that cascaded all the way down into DeckPanel's own body row,
        // which is what was cutting off DC/DD's transport row and pitch
        // fader. Dividing by 97 (not 100) turns 14/47/36 into weights of
        // THIS smaller, correct base rather than assuming they already
        // summed to 100. LyricDisplay's own height isn't part of this
        // overhead calculation -- it collapses to 0 when neither deck has
        // lyrics loaded (the common, non-karaoke case), so only its
        // spacing is accounted for here, not a fixed height for the row
        // itself.
        readonly property real availableForPercentRows: root.height - 24 - 48 - 26

        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // Window-level vertical stack, matching Deo Pro dj_layout_spec.json's
        // root children: top_bar (3%), waveform_overview_section (14%),
        // deck_section (32%), browser_section (51%).
        Deo.TopBar {
            Layout.fillWidth: true
        }
        Deo.LyricDisplay {
        }
        ColumnLayout {
            Layout.fillWidth: true
            // Layout.maximumHeight, not just preferredHeight -- a hard
            // cap, not a hint. root.height is the ApplicationWindow's own
            // height, externally driven by window resizing, so this
            // isn't circular. Without a cap, ColumnLayout/RowLayout
            // always auto-computes their own implicit height from
            // children by default, so this row could still be forced
            // taller than its 14% allocation if DeckWaveform ever needed
            // more room than that, same class of bug as deckMixerRow
            // below.
            Layout.maximumHeight: Math.max(110, rootColumn.availableForPercentRows * (14 / 97))
            Layout.preferredHeight: Math.max(110, rootColumn.availableForPercentRows * (14 / 97))
            Layout.minimumHeight: 110
            clip: true
            spacing: 2

            Deo.DeckWaveform {
                Layout.fillWidth: true
                Layout.fillHeight: true
                accentColor: Theme.deckAAccent
                group: "[Channel1]"
            }
            Deo.DeckWaveform {
                Layout.fillWidth: true
                Layout.fillHeight: true
                accentColor: Theme.deckBAccent
                group: "[Channel2]"
            }
        }
        RowLayout {
            id: deckMixerRow

            Layout.fillWidth: true
            // Layout.maximumHeight, not just preferredHeight -- a hard
            // cap against root.height (the window's own, externally
            // driven height -- not circular). preferredHeight alone is
            // only ever a hint: RowLayout always auto-computes its own
            // implicit height from its children (DeckPanel, MixerTabs),
            // and Qt Quick Layouts won't shrink a fillHeight child below
            // its own implicit size -- so even after capping DeckPanel's
            // OWN internal content (see DeckPanel.qml), this row itself
            // could still get pushed taller than its allocation if
            // DeckPanel's real internal minimum ever exceeded that, and
            // without a cap here that growth pushed past this row's own
            // box into the Library/browser section below instead of
            // being contained inside deckMixerRow.
            //
            // 38, not the original 32 -- per explicit user request,
            // deck_section's share of the window grew (taking the
            // difference from Library's 51, now 45 below) after the
            // deck's full lower body (FX/StemPads/CustomPad/Loop/jog
            // wheel/transport/pitch fader) was found to only have ~236px
            // to share at the old 32/97 weight, at this app's own default
            // ~1100px window height -- objectively tight for six sections
            // at a readable size, not a bug. Tried 47, then 40, dialed to
            // 38. 14+38+45 still sums to 97, so the /97 divisor elsewhere
            // doesn't need to change.
            Layout.maximumHeight: Math.max(300, rootColumn.availableForPercentRows * (38 / 97))
            Layout.preferredHeight: Math.max(300, rootColumn.availableForPercentRows * (38 / 97))
            Layout.minimumHeight: 300
            clip: true
            spacing: 24

            // deck_A / mixer_module / deck_B are EXACTLY 41% / 18% / 41%
            // of deck_section's width, always, regardless of window size
            // -- per explicit user request: a weight/floor system that
            // only roughly approximates the given percentages (and lets a
            // hardcoded minimumWidth silently override them, as
            // minimumWidth: 400 was doing to the mixer's 18%) is not
            // acceptable; these percentages must be computed and honored
            // exactly.
            //
            // availableForColumns: rootColumn.width (NOT
            // deckMixerRow.width) minus the 2 real gaps between the three
            // columns. rootColumn is the stable, externally-driven
            // reference here -- its width comes from root.width via
            // anchors.fill/anchors.margins, not from summing its own
            // children's implicit sizes, so reading it from deckMixerRow
            // (rootColumn's own child) is not circular. Reading
            // deckMixerRow's OWN width from within its own children would
            // be circular (the parent's width can't be resolved without
            // the children's hints, which need the parent's width) --
            // confirmed by Qt Quick Layouts itself detecting this
            // ("recursive rearrange, aborting after two iterations") and
            // falling back to an unreliable value instead of erroring.
            readonly property real availableForColumns: rootColumn.width - 2 * spacing
            // DeckPanel.qml itself now authors a fixed 656x333 canvas at
            // DeckDesignMockup.qml's own exact literal coordinates (same
            // technique as every sub-component inside it) -- the
            // unwired-mockup experiment swap that used to sit here is no
            // longer needed now that the real, wired component matches
            // the mockup's layout exactly.
            Deo.DeckPanel {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.maximumWidth: deckMixerRow.availableForColumns * 0.41
                Layout.minimumWidth: deckMixerRow.availableForColumns * 0.41
                Layout.preferredWidth: deckMixerRow.availableForColumns * 0.41
                accentColor: Theme.deckAAccent
                effectUnitNumber: 1
                group: "[Channel1]"
                label: "DECK A"
            }
            Deo.MixerTabs {
                id: mixerTabs

                Layout.fillHeight: true
                // Exactly 18% of availableForColumns, always -- no
                // minimumWidth floor. The previous 400px floor (there to
                // protect AudioMixerPanel's own internal minimum sum of
                // 90+180+90=360, and MasterPanel's 90+200+90=380) was
                // silently overriding the user's explicit 18% (400px
                // measured out to ~23.5% of a 1704px row, not 18%) --
                // removed per explicit instruction that the given
                // percentage must be honored exactly regardless of window
                // size, not quietly substituted with a bigger fixed floor.
                Layout.maximumWidth: deckMixerRow.availableForColumns * 0.18
                Layout.minimumWidth: deckMixerRow.availableForColumns * 0.18
                Layout.preferredWidth: deckMixerRow.availableForColumns * 0.18
                accentColorA: Theme.deckAAccent
                accentColorB: Theme.deckBAccent
            }
            Deo.DeckPanel {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.maximumWidth: deckMixerRow.availableForColumns * 0.41
                Layout.minimumWidth: deckMixerRow.availableForColumns * 0.41
                Layout.preferredWidth: deckMixerRow.availableForColumns * 0.41
                accentColor: Theme.deckBAccent
                effectUnitNumber: 2
                group: "[Channel2]"
                label: "DECK B"
                mirrored: true
            }
        }
        // M7: browser_section is a permanent, always-visible window-level
        // row in the spec (originally 51% of window height, sibling of
        // deck_section), not a toggleable overlay -- it was built as a
        // toggle in the first pass without checking this file, which was
        // wrong.
        //
        // 45, not the original 51 -- per explicit user request, gave 6
        // points to deck_section (now 38, see deckMixerRow above) after
        // the deck's full lower body was found to only have ~236px to
        // share at the old weight, at this app's own default ~1100px
        // window height. Tried 36 (deck at 47), then 43 (deck at 40),
        // dialed to 45 (deck at 38).
        RowLayout {
            Layout.fillWidth: true
            Layout.maximumHeight: Math.max(250, rootColumn.availableForPercentRows * (45 / 97))
            Layout.preferredHeight: Math.max(250, rootColumn.availableForPercentRows * (45 / 97))
            Layout.minimumHeight: 250
            clip: true
            spacing: 8

            Library {
                Layout.fillHeight: true
                Layout.fillWidth: true
            }
            // M14 Stage 5: singer queue, as a permanent sidebar next to
            // the library rather than a popup dialog -- per explicit user
            // request. Collapses to zero width (not just hidden) when
            // karaoke mode is off, matching LyricDisplay.qml's own
            // collapse-when-unneeded pattern, so it never affects the
            // library's own width in ordinary (non-karaoke) use.
            // karaokeModeEnabled is a real Q_PROPERTY with NOTIFY, so
            // binding these Layout sizes directly to it (rather than
            // needing a poll Timer, unlike KaraokeManager's plain
            // Q_INVOKABLE query methods used elsewhere in this feature)
            // stays live.
            Deo.SingerQueuePanel {
                Layout.fillHeight: true
                Layout.maximumWidth: Mixxx.KaraokeManager.karaokeModeEnabled ? 280 : 0
                Layout.minimumWidth: Mixxx.KaraokeManager.karaokeModeEnabled ? 280 : 0
                Layout.preferredWidth: Mixxx.KaraokeManager.karaokeModeEnabled ? 280 : 0
                clip: true
                visible: Mixxx.KaraokeManager.karaokeModeEnabled
            }
        }
    }
}
