#include "library/lyrictranscription/whisperlrcbuilder.h"

#include <gtest/gtest.h>

#include "library/karaoke/lrcparser.h"

using namespace mixxx;

namespace {

TEST(WhisperLrcBuilderTest, FormatsTimestampsAsMinutesSecondsHundredths) {
    EXPECT_EQ(WhisperLrcBuilder::formatLrcTimestamp(0.0), "00:00.00");
    EXPECT_EQ(WhisperLrcBuilder::formatLrcTimestamp(12.0), "00:12.00");
    EXPECT_EQ(WhisperLrcBuilder::formatLrcTimestamp(62.25), "01:02.25");
    EXPECT_EQ(WhisperLrcBuilder::formatLrcTimestamp(-5.0), "00:00.00");
}

TEST(WhisperLrcBuilderTest, SingleLineWhenWordsAreCloseTogether) {
    const std::vector<WhisperWordTiming> words{
            {10.0, 10.3, "One"},
            {10.5, 10.8, "two"},
            {11.0, 11.3, "three"},
    };

    const QString lrc = WhisperLrcBuilder::buildEnhancedLrc(words);
    const QList<LrcLine> lines = LrcParser::parse(lrc);

    ASSERT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0].text, "One two three");
    ASSERT_EQ(lines[0].wordTimestamps.size(), 3);
    EXPECT_DOUBLE_EQ(lines[0].wordTimestamps[0], 10.0);
    EXPECT_DOUBLE_EQ(lines[0].wordTimestamps[1], 10.5);
    EXPECT_DOUBLE_EQ(lines[0].wordTimestamps[2], 11.0);
}

TEST(WhisperLrcBuilderTest, BreaksLineOnLargeGapBetweenWords) {
    const std::vector<WhisperWordTiming> words{
            {10.0, 10.3, "First"},
            {10.4, 10.6, "line"},
            // Gap of 2s since the previous word's end (>= kLineBreakGapSeconds).
            {12.6, 12.9, "Second"},
            {13.0, 13.2, "line"},
    };

    const QString lrc = WhisperLrcBuilder::buildEnhancedLrc(words);
    const QList<LrcLine> lines = LrcParser::parse(lrc);

    ASSERT_EQ(lines.size(), 2);
    EXPECT_EQ(lines[0].text, "First line");
    EXPECT_EQ(lines[1].text, "Second line");
    EXPECT_DOUBLE_EQ(lines[1].timestampSeconds, 12.6);
}

TEST(WhisperLrcBuilderTest, BreaksLineAfterMaxWordsPerLine) {
    std::vector<WhisperWordTiming> words;
    for (int i = 0; i < WhisperLrcBuilder::kMaxWordsPerLine + 5; i++) {
        // Words packed tightly together, no gap-based break should trigger.
        const double start = i * 0.2;
        words.push_back(WhisperWordTiming{start, start + 0.1, QString("w%1").arg(i)});
    }

    const QString lrc = WhisperLrcBuilder::buildEnhancedLrc(words);
    const QList<LrcLine> lines = LrcParser::parse(lrc);

    ASSERT_EQ(lines.size(), 2);
    EXPECT_EQ(lines[0].wordTimestamps.size(), WhisperLrcBuilder::kMaxWordsPerLine);
    EXPECT_EQ(lines[1].wordTimestamps.size(), 5);
}

TEST(WhisperLrcBuilderTest, EmptyInputProducesEmptyOutput) {
    EXPECT_TRUE(WhisperLrcBuilder::buildEnhancedLrc({}).isEmpty());
}

TEST(WhisperLrcBuilderTest, SplitSegmentIntoWordsHandlesSingleWord) {
    const std::vector<WhisperWordTiming> result =
            WhisperLrcBuilder::splitSegmentIntoWords(10.0, 10.5, "Hello");

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].text, "Hello");
    EXPECT_DOUBLE_EQ(result[0].startSeconds, 10.0);
    EXPECT_DOUBLE_EQ(result[0].endSeconds, 10.5);
}

// M16 Stage 4: whisper.cpp's max_len=1/split_on_word settings make it
// split segments at ~1 word each, but this isn't a hard guarantee -- a
// multi-word segment must still come out as one WhisperWordTiming per
// word (sharing the segment's timing), or it would silently desync the
// word-highlight index from LrcParser's word count for the rest of the
// line (see splitSegmentIntoWords()'s own doc comment).
TEST(WhisperLrcBuilderTest, SplitSegmentIntoWordsHandlesMultipleWords) {
    const std::vector<WhisperWordTiming> result =
            WhisperLrcBuilder::splitSegmentIntoWords(10.0, 10.8, "New York City");

    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0].text, "New");
    EXPECT_EQ(result[1].text, "York");
    EXPECT_EQ(result[2].text, "City");
    for (const WhisperWordTiming& word : result) {
        EXPECT_DOUBLE_EQ(word.startSeconds, 10.0);
        EXPECT_DOUBLE_EQ(word.endSeconds, 10.8);
    }
}

TEST(WhisperLrcBuilderTest, SplitSegmentIntoWordsCollapsesExtraWhitespace) {
    const std::vector<WhisperWordTiming> result =
            WhisperLrcBuilder::splitSegmentIntoWords(0.0, 1.0, "  a   b  ");

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0].text, "a");
    EXPECT_EQ(result[1].text, "b");
}

TEST(WhisperLrcBuilderTest, SplitSegmentIntoWordsHandlesEmptyInput) {
    EXPECT_TRUE(WhisperLrcBuilder::splitSegmentIntoWords(0.0, 1.0, "").empty());
    EXPECT_TRUE(WhisperLrcBuilder::splitSegmentIntoWords(0.0, 1.0, "   ").empty());
}

// End-to-end regression for the desync scenario itself: feed a mix of
// single- and multi-word segments through the full pipeline (split ->
// buildEnhancedLrc -> LrcParser::parse) and confirm the line's word count
// always matches its wordTimestamps count, keeping the highlight index
// valid for every word in the line, not just the ones before the first
// multi-word segment.
TEST(WhisperLrcBuilderTest, MultiWordSegmentStaysAlignedThroughFullPipeline) {
    std::vector<WhisperWordTiming> words;
    auto append = [&words](double start, double end, const QString& segmentText) {
        const std::vector<WhisperWordTiming> split =
                WhisperLrcBuilder::splitSegmentIntoWords(start, end, segmentText);
        words.insert(words.end(), split.begin(), split.end());
    };
    append(10.0, 10.3, "Take");
    append(10.4, 11.0, "me to New York");
    append(11.1, 11.4, "tonight");

    const QString lrc = WhisperLrcBuilder::buildEnhancedLrc(words);
    const QList<LrcLine> lines = LrcParser::parse(lrc);

    ASSERT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0].text, "Take me to New York tonight");
    ASSERT_EQ(lines[0].wordTimestamps.size(), 6);
    EXPECT_EQ(lines[0].text.split(' ').size(), lines[0].wordTimestamps.size());
}

} // namespace
