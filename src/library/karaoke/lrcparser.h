#pragma once

#include <QList>
#include <QString>

#include "library/karaoke/lyricssource.h"

namespace mixxx {

/// One timed line from a .lrc file.
struct LrcLine {
    double timestampSeconds;
    QString text;
    /// M14 Stage 2 (enhanced/word-level LRC): one timestamp per word in
    /// `text.split(' ', Qt::SkipEmptyParts)`, aligned 1:1. Empty when this
    /// line had no inline `<mm:ss.xx>` word tags -- plain line-level-only
    /// files (Stage 1) never populate this, and callers treat an empty
    /// list as "no word-level highlight available for this line", not an
    /// error.
    QList<double> wordTimestamps;
};

/// M14 Stage 1/2: parses the widely-supported LRC lyric format
/// ([mm:ss.xx]Lyric text, one or more timestamp tags per line, plain
/// metadata tags like [ar:...]/[ti:...] ignored), including the enhanced/
/// word-level variant's inline `<mm:ss.xx>` tags before each word.
class LrcParser {
  public:
    /// Parses raw .lrc file content into a list of timed lines, sorted by
    /// timestamp ascending. Lines with no recognized timestamp tag (plain
    /// metadata, or a malformed timestamp) are skipped rather than
    /// erroring -- .lrc is a loosely-specified, hand-editable text format
    /// in the wild, and one bad line shouldn't take down the whole file.
    static QList<LrcLine> parse(const QString& fileContents);
};

/// Line-level LyricsSource backed by a parsed .lrc file.
class LrcLyricsSource : public LyricsSource {
  public:
    explicit LrcLyricsSource(QList<LrcLine> lines);

    bool isValid() const override;
    QString currentLine(double positionSeconds) const override;

    /// M14 Stage 2: the index (into currentLine()'s text, split on single
    /// spaces) of the word that should currently be highlighted, or -1 if
    /// the current line has no word-level timing data, or no word in it
    /// is active yet.
    int currentWordIndex(double positionSeconds) const;

  private:
    int findCurrentLineIndex(double positionSeconds) const;

    QList<LrcLine> m_lines;
};

} // namespace mixxx
