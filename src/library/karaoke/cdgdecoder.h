#pragma once

#include <QByteArray>
#include <QImage>
#include <QList>
#include <QString>

#include "library/karaoke/lyricssource.h"

namespace mixxx {

/// M14 Stage 3: decodes the legacy CD+G ("CDG") subchannel graphics
/// format -- the traditional karaoke-disc-rip lyric format, usually
/// distributed as a .cdg file paired with an .mp3 of the same name
/// ("MP3+G"). The stream is 24-byte "packs", 300 per second of audio
/// (matching a CD's own 75 sectors/sec x 4 subchannel packets/sector),
/// each either a no-op or one of a handful of drawing instructions onto a
/// fixed 300x216-pixel, 16-color-indexed canvas.
///
/// Implements the commands that matter for real karaoke content -- Memory
/// Preset (full-screen fill), Color Table Low/High (palette definition),
/// and Tile Block Normal/XOR (the actual lyric-text drawing) -- and
/// no-ops the rest (Border Preset, Scroll Preset/Copy, Define Transparent
/// Color). Real karaoke CDG rips draw lyrics almost exclusively via tile
/// blocks; border/scroll/transparency are cosmetic extras a standalone
/// lyric window (not overlaid on video, unlike the format's original
/// on-TV use case) doesn't need.
class CdgDecoder {
  public:
    static constexpr int kWidth = 300;
    static constexpr int kHeight = 216;

    struct Frame {
        double timestampSeconds;
        QImage image;
    };

    /// Fully decodes the whole file up front into a list of keyframe
    /// canvas snapshots (one whenever a visually-consequential command
    /// changes the canvas), sorted by timestamp ascending -- CDG files
    /// are small (a whole song is typically a few hundred KB) and DJ
    /// seeking is a normal action here, so random access needs to be a
    /// binary search over keyframes rather than a replay from the start
    /// every time.
    static QList<Frame> decode(const QByteArray& fileContents);
};

/// Image-based LyricsSource backed by a decoded .cdg file. currentLine()
/// always returns an empty string (CDG has no concept of text) -- callers
/// that need the visual should use currentFrame() instead.
class CdgLyricsSource : public LyricsSource {
  public:
    explicit CdgLyricsSource(QList<CdgDecoder::Frame> frames);

    bool isValid() const override;
    QString currentLine(double positionSeconds) const override;

    /// The canvas as it should look at this playback position, or a null
    /// QImage if there's no frame data at all.
    QImage currentFrame(double positionSeconds) const;

  private:
    QList<CdgDecoder::Frame> m_frames;
};

} // namespace mixxx
