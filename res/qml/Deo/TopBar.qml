import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import ".." as Skin
import "../Theme"

// Top Bar, sitting above the scrolling waveform row in main.qml, per the
// user's VirtualDJ reference screenshot. Built from what this app
// actually has real data for -- app branding, the master output level
// meter (Skin.VuMeter, group "[Master]", the same real COs already used
// in MasterPanel.qml), a live clock, battery status (Mixxx.Battery, the
// same singleton BatteryIcon.qml already uses), and a settings gear.
//
// M13: the gear now opens the skin's own QML Settings.qml popup
// (SoundHardware/Library/Controller/Interface/MixerEffect/AutoDJ/
// Broadcast/Recording/Analyzer/StatsPerformance pages, all pre-built)
// instead of Mixxx.PreferencesDialog.show()'s legacy Qt Widgets dialog --
// Settings.qml existed fully built but was never instantiated anywhere in
// this skin (confirmed via an exhaustive grep during M8). The one
// legacy-only feature confirmed to have no QML equivalent is the
// controller MIDI/HID Learning Wizard -- kept reachable via a link on
// Settings/Controller.qml's own page rather than duplicating the legacy
// dialog's full surface here.
//
// Deliberately NOT attempting a few things the reference shows: a CPU
// meter (no CPU/performance-monitoring API exists anywhere in this
// codebase -- would need new C++ work, not something to fake with a
// placeholder number), a user email/account display and a "LAYOUT"
// preset dropdown (VirtualDJ's cloud-account and named-layout-preset
// concepts -- Mixxx has no equivalent to either), and the traffic-light
// window controls (native macOS window chrome, not a QML element).
Rectangle {
    id: root

    // 26, not 32 -- per explicit user request, leaner to match the
    // reference's compact single-row header more closely and reclaim a
    // little vertical space for the rest of the app.
    implicitHeight: 26
    color: Theme.deckPanelBackground

    Timer {
        interval: 1000
        repeat: true
        running: true
        triggeredOnStart: true

        onTriggered: clockLabel.text = Qt.formatDateTime(new Date(), "h:mm AP")
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 12

        Label {
            color: Theme.deckTextBright
            font.bold: true
            font.family: Theme.fontFamily
            font.pixelSize: 13
            text: "DEO PRO DJ"
        }
        Item {
            Layout.fillWidth: true
        }
        Label {
            color: Theme.deckTextSecondary
            font.family: Theme.fontFamily
            font.pixelSize: 10
            text: "MASTER"
        }
        RowLayout {
            Layout.preferredHeight: 18
            spacing: 2

            Skin.VuMeter {
                Layout.fillHeight: true
                group: "[Master]"
                key: "vu_meter_left"
                width: 4
            }
            Skin.VuMeter {
                Layout.fillHeight: true
                group: "[Master]"
                key: "vu_meter_right"
                width: 4
            }
        }
        // Battery -- only shown when Mixxx.Battery actually reports one
        // available (desktop machines with no battery report none).
        Label {
            color: Theme.deckTextSecondary
            font.family: Theme.fontFamily
            font.pixelSize: 10
            text: (Mixxx.Battery.isCharging ? "⚡" : "🔋") + Math.round(Mixxx.Battery.percentage) + "%"
            visible: Mixxx.Battery.isBatteryAvailable
        }
        Label {
            id: clockLabel

            color: Theme.deckTextSecondary
            font.family: Theme.fontFamily
            font.pixelSize: 10
        }
        // M14 Stage 4: karaoke mode toggle -- same Item+MouseArea shape as
        // the settings gear right after it (see that item's own comment
        // for why a plain MouseArea is used instead of relying on
        // Skin.Button/AbstractButton's own click handling).
        // karaokeModeEnabled is a real Q_PROPERTY with NOTIFY (unlike
        // KaraokeManager's plain Q_INVOKABLE query methods elsewhere in
        // this feature), so binding `highlight` directly to it is safe
        // and stays live.
        Item {
            Layout.preferredHeight: 24
            Layout.preferredWidth: 32

            Skin.Button {
                anchors.centerIn: parent
                activeColor: Theme.deckActiveColor
                highlight: Mixxx.KaraokeManager.karaokeModeEnabled
                implicitHeight: 20
                implicitWidth: 20
                text: "🎤"
            }
            MouseArea {
                anchors.fill: parent

                onClicked: Mixxx.KaraokeManager.karaokeModeEnabled = !Mixxx.KaraokeManager.karaokeModeEnabled
            }
        }
        // M14 Stage 6: picks which physical display
        // KaraokeDisplayWindow.qml (the singer-facing second screen)
        // shows on -- only useful (and only shown) once karaoke mode is
        // on and there's actually more than one screen to choose between.
        //
        // M16 fix: this was the plain QtQuick.Controls ComboBox, whose
        // default OS-style padding/implicitHeight (~40px) massively
        // overflowed the 26px-tall bar -- Skin.ComboBox (res/qml/
        // ComboBox.qml) is this skin's own compact-styled equivalent,
        // already used throughout Settings/*.qml, sized here to match the
        // bar's other controls.
        Skin.ComboBox {
            Layout.preferredHeight: 20
            Layout.preferredWidth: 110
            clip: true
            model: Qt.application.screens.map((screen, i) => (i + 1) + ": " + screen.name)
            visible: Mixxx.KaraokeManager.karaokeModeEnabled && Qt.application.screens.length > 1
            currentIndex: Mixxx.KaraokeManager.karaokeDisplayScreenIndex

            onActivated: index => {
                Mixxx.KaraokeManager.karaokeDisplayScreenIndex = index;
            }
        }
        // Item wrapper + explicit MouseArea, not just Skin.Button's own
        // AbstractButton click handling -- diagnostic testing found the
        // bare 20x20 Skin.Button's own click delivery unreliable here
        // (needed several attempts to register), while a plain MouseArea
        // covering the same corner fired every time. Widening the real
        // hit target to 32x24 (visual icon stays 20x20, centered) on top
        // of switching to the proven-reliable MouseArea addresses both
        // possible causes at once -- a too-small/imprecise target and
        // AbstractButton's own click handling.
        Item {
            Layout.preferredHeight: 24
            Layout.preferredWidth: 32

            Skin.Button {
                anchors.centerIn: parent
                implicitHeight: 20
                implicitWidth: 20
                text: "⚙"
            }
            MouseArea {
                anchors.fill: parent

                onClicked: settingsPopup.open()
            }
        }
    }
    // Centered against Overlay.overlay's own width/height, not
    // Window.width/height -- confirmed via diagnostic logging that the
    // Window attached property resolves to 0 on this Item (opened=true,
    // visible=true, but x=-450/y=-300 for a 900x600 popup -- entirely
    // off-screen, which is why it appeared to "pop up and vanish": it
    // WAS opening, just invisible off in negative space, then got
    // auto-closed by the next click landing "outside" its real,
    // off-screen bounds). Overlay.overlay is the actual Item Popup
    // renders into and always reports the real window content size.
    Skin.Settings {
        id: settingsPopup

        height: 600
        width: 900
        x: (Overlay.overlay.width - width) / 2
        y: (Overlay.overlay.height - height) / 2
    }
}
