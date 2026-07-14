#include "library/stemseparation/trackmetadatacopy.h"

#include "track/cue.h"
#include "track/track.h"

namespace mixxx {
namespace stemseparation {

void copyTimelineMetadata(const Track& source, Track& dest) {
    // Cue points: build fresh Cue objects rather than sharing CuePointer
    // instances across two Track objects -- each Track's cue list should
    // own independent instances for dirty-tracking/persistence correctness.
    QList<CuePointer> copiedCues;
    const QList<CuePointer> sourceCues = source.getCuePoints();
    copiedCues.reserve(sourceCues.size());
    for (const CuePointer& pSourceCue : sourceCues) {
        CuePointer pCue(new Cue(
                pSourceCue->getType(),
                pSourceCue->getHotCue(),
                pSourceCue->getPosition(),
                pSourceCue->getEndPosition(),
                pSourceCue->getColor()));
        pCue->setLabel(pSourceCue->getLabel());
        copiedCues.append(pCue);
    }
    dest.setCuePoints(copiedCues);

    // Beatgrid: Beats is an immutable value object computed from sample
    // positions/sample rate, both identical between source and stem --
    // safe to share the same BeatsPointer.
    BeatsPointer pBeats = source.getBeats();
    if (pBeats) {
        dest.trySetBeats(pBeats);
    }

    dest.setKeys(source.getKeys());
}

} // namespace stemseparation
} // namespace mixxx
