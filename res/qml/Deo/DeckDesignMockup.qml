import QtQuick 2.12

Item {
    width: 656
    height: 333

    JogwheelDesignMockup {
        id: jogwheelDesignMockup
        x: 393
        y: 99

        FaderSectionDesignMockup {
            id: faderSectionDesignMockup
            x: 219
            y: 0
        }
    }

    EffectsSectionMockup {
        id: effectsSectionMockup
        x: 0
        y: 99
    }

    PadsDesignMockup {
        id: padsDesignMockup
        x: 0
        y: 227
        height: 106
    }

    CustomPadDesignMockup {
        id: customPadDesignMockup
        x: 253
        y: 99
    }

    CustomLoopSectionDesignMockup {
        id: customLoopSectionDesignMockup
        x: 253
        y: 227
        height: 106
    }

}
