import QtQuick 2.12
import ".." as Skin

// One channel's VU lane -- L+R bars side by side, matching the width/
// position of a single VU lane in the user's reference mockup
// (MixerDesignMockupA.qml: rectangle6/rectangle9, each a 27px-wide lane
// flanking the center divider). The mockup itself only drew one bar per
// lane, but per explicit user instruction ("keep all 4 real bars") both
// real per-channel vu_meter_left/right COs stay -- two bars centered
// within this same 27px lane instead of collapsing to one.
Item {
    id: root

    required property string group

    width: 27
    height: 345

    Skin.VuMeter {
        x: 6
        y: 8
        width: 5
        height: 293
        group: root.group
        key: "vu_meter_left"
    }
    Skin.VuMeter {
        x: 16
        y: 8
        width: 5
        height: 293
        group: root.group
        key: "vu_meter_right"
    }
}
