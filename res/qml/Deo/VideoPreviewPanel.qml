import QtQuick 2.12
import QtQuick.Window
import Mixxx 1.0 as Mixxx
import "../Theme"

// M12 Stage 3e: replaces Stage 3c/3d's inline preview Rectangle in
// MixerTabs.qml's VIDEO tab, which caused a real layout-shift bug --
// showing/hiding it resized the whole center mixer column, which pushed
// DECK A/DECK B around. A floating panel with no participation in any
// Layout fixed that.
//
// Stage 4b: rebuilt as a real top-level Window instead of an in-scene
// Rectangle drawn over the main window's own surface -- the user
// explicitly asked to resize it, full-screen it, and move it to a
// different monitor, none of which an embedded Item can do (it was
// clipped to the main ApplicationWindow's own bounds and had no OS-level
// resize/fullscreen affordance at all). A genuine Window gets all three
// for free from the OS's own window manager: native title bar dragging
// (including across monitors), a native resize border/corner on every
// edge (any Window is resizable unless minimumSize == maximumSize, which
// this deliberately never sets), and -- on macOS -- the green traffic-
// light button's native fullscreen/zoom behavior. This also means the
// custom minimize toggle, drag-to-move MouseArea, and clampPosition()
// logic the old Rectangle needed (see git history) are gone entirely:
// the native title bar already does all of that, and clamping a real OS
// window to a single "parent" would actively fight the multi-monitor
// move this rewrite exists to allow.
//
// Declaring a Window as a plain child of another Window (main.qml's
// ApplicationWindow) is a normal QML pattern -- Qt Quick treats any
// Window-derived item anywhere in the tree as its own separate top-level
// OS window (using the nearest Window ancestor as its transient parent),
// not something embedded/clipped inside the parent's surface.
Window {
    id: root

    // Screen coordinates now (a real top-level window), not relative to
    // any parent item -- matches the previous default's intent (near the
    // screen's own top-left) without trying to clamp against a specific
    // monitor's bounds, since the user should be free to drag this to
    // any monitor and have that position remembered by the OS/session
    // the normal way any other window's position would be.
    x: 40
    y: 80
    width: 320
    height: 240
    minimumWidth: 160
    minimumHeight: 120
    title: qsTr("Video Preview")
    color: Theme.deckBackgroundColor
    visible: Mixxx.VideoEngine.enabled

    // Letterbox/pillarbox instead of stretching to fill this window's
    // current shape: neither preview item has any aspect-ratio awareness
    // of its own (QmlVideoPreviewItem::paint() draws via
    // QPainter::drawImage(bounds, image); QmlVideoPreviewGlItem's blend
    // node is sized to its own bounds the same way) -- so this container
    // is sized here in QML to "contain" (fit inside, preserve ratio,
    // center) rather than simply filling its parent, and whatever window
    // background shows through the gaps.
    Item {
        // VideoEngineManager's own pipelines always scale every source
        // (real clip, camera, black placeholder) to a fixed delivery
        // resolution before it ever reaches either preview item -- see
        // kPreviewWidth/kPreviewHeight in videoenginemanager.cpp. That's
        // the actual aspect ratio being displayed here regardless of a
        // source clip's own native ratio (a clip's real proportions are
        // already normalized away earlier in the pipeline, not something
        // this window can recover) -- 4:3 is that fixed 320x240 delivery
        // shape, not a hardcoded guess.
        readonly property real videoAspectRatio: 4 / 3

        anchors.centerIn: parent
        width: (parent.width / parent.height > videoAspectRatio)
                ? parent.height * videoAspectRatio
                : parent.width
        height: (parent.width / parent.height > videoAspectRatio)
                ? parent.height
                : parent.width / videoAspectRatio

        // Stage 7: the CPU path stays the always-live base layer -- it
        // needs no window/GL-context readiness of any kind, so it's
        // already showing a real composited frame from the very first
        // paint. Mixxx.VideoPreviewGl is stacked directly on top at the
        // same geometry and starts out drawing nothing at all (its
        // updatePaintNode() returns a null node until this window's RHI
        // OpenGL context has been shared with VideoEngineManager and the
        // GStreamer pipelines have been rebuilt to produce GL frames) --
        // so the CPU frame shows through underneath for however long that
        // takes. Once GL frames start arriving, the GL item's own opaque
        // composite simply covers the CPU one every tick, with no visible
        // seam (both draw the exact same crossfader-driven blend). This
        // avoids needing any reactive "which path is ready yet" plumbing
        // between C++ and QML -- deliberately simpler than, and safer
        // than, wiring up a property-driven Loader for a choice that can
        // only really be known after this window already exists.
        Mixxx.VideoPreview {
            anchors.fill: parent
        }
        Mixxx.VideoPreviewGl {
            anchors.fill: parent
        }
    }
}
