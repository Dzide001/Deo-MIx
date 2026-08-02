#pragma once

#include <QString>
#include <vector>

namespace mixxx {

/// One transcribed word and its timing, in seconds.
struct WhisperWordTiming {
    double startSeconds;
    double endSeconds;
    QString text;
};

/// M16: converts whisper.cpp's raw word-level transcription output into
/// enhanced-LRC text (LrcParser, src/library/karaoke/lrcparser.h, already
/// parses this exact format -- one leading [mm:ss.xx] line timestamp plus
/// one inline <mm:ss.xx>word tag per word). Pure, side-effect-free
/// text-building logic, kept separate from WhisperTranscriptionJob's I/O/
/// threading/whisper-API-calling code so it's directly unit-testable
/// without needing a real model or audio file.
///
/// Raw ASR output has no concept of a "line" -- consecutive words are
/// grouped into a line until either a pause of at least
/// kLineBreakGapSeconds since the previous word, or kMaxWordsPerLine words
/// already collected, whichever comes first. A first-pass heuristic,
/// deliberately simple; revisited against real transcribed tracks once
/// this is wired into the real pipeline.
class WhisperLrcBuilder {
  public:
    static constexpr double kLineBreakGapSeconds = 0.7;
    static constexpr int kMaxWordsPerLine = 10;

    static QString buildEnhancedLrc(const std::vector<WhisperWordTiming>& words);
    static QString formatLrcTimestamp(double seconds);

    /// M16 Stage 4: splits one whisper.cpp segment's text into one
    /// WhisperWordTiming per whitespace-separated word, all sharing
    /// `startSeconds`/`endSeconds`. The caller's whisper_full_params
    /// (token_timestamps/max_len=1/split_on_word) make whisper.cpp split
    /// segments at ~1 word each, but that's not a hard guarantee -- BPE
    /// tokenization can still land more than one whitespace-separated
    /// token in a single segment. buildEnhancedLrc()/LrcParser (M14,
    /// src/library/karaoke/lrcparser.h) require exactly one array entry
    /// per space-separated word in the rendered line (LrcLine::
    /// wordTimestamps is defined to align 1:1 with text.split(' ')), so a
    /// multi-word segment must be split before reaching buildEnhancedLrc()
    /// -- otherwise it collapses into a single array entry there while
    /// still rendering as multiple space-separated words in the line
    /// text, silently desyncing the word-highlight index from that word
    /// onward for the rest of the line.
    static std::vector<WhisperWordTiming> splitSegmentIntoWords(
            double startSeconds, double endSeconds, const QString& segmentText);
};

} // namespace mixxx
