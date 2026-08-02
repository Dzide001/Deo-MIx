#pragma once

#include <QString>

namespace mixxx {

/// M14: a per-track lyrics source, either parsed from a sidecar .lrc file
/// (LrcLyricsSource) or, later, decoded from a .cdg/MP3+G pair
/// (CdgLyricsSource). KaraokeManager and the QML-facing proxy only ever
/// talk to this interface, so neither cares which sidecar format a given
/// track actually shipped with.
class LyricsSource {
  public:
    virtual ~LyricsSource() = default;

    /// False if the sidecar file was found but contained no usable timed
    /// lines (empty, or every line failed to parse) -- KaraokeManager
    /// treats this the same as "no sidecar file at all" rather than
    /// showing a permanently blank lyric display.
    virtual bool isValid() const = 0;

    /// The line of text that should be showing at this playback position,
    /// or an empty string if no line is active yet (before the first
    /// timestamp).
    virtual QString currentLine(double positionSeconds) const = 0;
};

} // namespace mixxx
