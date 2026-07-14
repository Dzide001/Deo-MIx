#pragma once

class Track;

namespace mixxx {
namespace stemseparation {

/// Copies cue points, beatgrid, and musical key from `source` to `dest`.
///
/// Valid only when both tracks reference audio with an identical timeline
/// (true by construction for a freshly AI-separated stem file: Demucs
/// preserves exact sample positions from the source track).
void copyTimelineMetadata(const Track& source, Track& dest);

} // namespace stemseparation
} // namespace mixxx
