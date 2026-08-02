#include "library/karaoke/lrcparser.h"

#include <gtest/gtest.h>

using namespace mixxx;

namespace {

TEST(LrcParserTest, ParsesSimpleTimedLines) {
    const QString contents =
            "[00:12.00]First line\n"
            "[00:15.50]Second line\n"
            "[01:02.25]Third line\n";

    const QList<LrcLine> lines = LrcParser::parse(contents);

    ASSERT_EQ(lines.size(), 3);
    EXPECT_DOUBLE_EQ(lines[0].timestampSeconds, 12.0);
    EXPECT_EQ(lines[0].text, "First line");
    EXPECT_DOUBLE_EQ(lines[1].timestampSeconds, 15.5);
    EXPECT_EQ(lines[1].text, "Second line");
    EXPECT_DOUBLE_EQ(lines[2].timestampSeconds, 62.25);
    EXPECT_EQ(lines[2].text, "Third line");
}

TEST(LrcParserTest, SortsLinesByTimestampRegardlessOfFileOrder) {
    const QString contents =
            "[00:20.00]Later\n"
            "[00:05.00]Earlier\n";

    const QList<LrcLine> lines = LrcParser::parse(contents);

    ASSERT_EQ(lines.size(), 2);
    EXPECT_EQ(lines[0].text, "Earlier");
    EXPECT_EQ(lines[1].text, "Later");
}

TEST(LrcParserTest, DuplicatesALineWithMultipleTimestampTags) {
    const QString contents = "[00:10.00][00:40.00]Chorus repeats\n";

    const QList<LrcLine> lines = LrcParser::parse(contents);

    ASSERT_EQ(lines.size(), 2);
    EXPECT_DOUBLE_EQ(lines[0].timestampSeconds, 10.0);
    EXPECT_EQ(lines[0].text, "Chorus repeats");
    EXPECT_DOUBLE_EQ(lines[1].timestampSeconds, 40.0);
    EXPECT_EQ(lines[1].text, "Chorus repeats");
}

TEST(LrcParserTest, SkipsPlainMetadataTags) {
    const QString contents =
            "[ar:Some Artist]\n"
            "[ti:Some Title]\n"
            "[00:01.00]Only real line\n";

    const QList<LrcLine> lines = LrcParser::parse(contents);

    ASSERT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0].text, "Only real line");
}

TEST(LrcParserTest, SkipsMalformedLines) {
    const QString contents =
            "not a timestamp at all\n"
            "[00:01.00]Valid line\n";

    const QList<LrcLine> lines = LrcParser::parse(contents);

    ASSERT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0].text, "Valid line");
}

TEST(LrcParserTest, EmptyFileProducesEmptyList) {
    EXPECT_TRUE(LrcParser::parse(QString()).isEmpty());
}

TEST(LrcLyricsSourceTest, IsValidReflectsWhetherAnyLinesParsed) {
    LrcLyricsSource emptySource{QList<LrcLine>()};
    EXPECT_FALSE(emptySource.isValid());

    LrcLyricsSource realSource(LrcParser::parse("[00:01.00]Hello\n"));
    EXPECT_TRUE(realSource.isValid());
}

TEST(LrcLyricsSourceTest, CurrentLineTracksPlaybackPosition) {
    LrcLyricsSource source(LrcParser::parse(
            "[00:10.00]First\n"
            "[00:20.00]Second\n"));

    EXPECT_EQ(source.currentLine(0.0), "");
    EXPECT_EQ(source.currentLine(9.99), "");
    EXPECT_EQ(source.currentLine(10.0), "First");
    EXPECT_EQ(source.currentLine(15.0), "First");
    EXPECT_EQ(source.currentLine(20.0), "Second");
    EXPECT_EQ(source.currentLine(999.0), "Second");
}

TEST(LrcParserTest, ParsesEnhancedWordLevelTimestamps) {
    const QString contents = "[00:10.00]<00:10.00>One <00:10.50>two <00:11.00>three\n";

    const QList<LrcLine> lines = LrcParser::parse(contents);

    ASSERT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0].text, "One two three");
    ASSERT_EQ(lines[0].wordTimestamps.size(), 3);
    EXPECT_DOUBLE_EQ(lines[0].wordTimestamps[0], 10.0);
    EXPECT_DOUBLE_EQ(lines[0].wordTimestamps[1], 10.5);
    EXPECT_DOUBLE_EQ(lines[0].wordTimestamps[2], 11.0);
}

TEST(LrcParserTest, PlainLineHasNoWordTimestamps) {
    const QList<LrcLine> lines = LrcParser::parse("[00:10.00]Plain line, no word tags\n");

    ASSERT_EQ(lines.size(), 1);
    EXPECT_TRUE(lines[0].wordTimestamps.isEmpty());
}

TEST(LrcLyricsSourceTest, CurrentWordIndexTracksWordLevelTiming) {
    LrcLyricsSource source(LrcParser::parse(
            "[00:10.00]<00:10.00>One <00:10.50>two <00:11.00>three\n"));

    EXPECT_EQ(source.currentWordIndex(9.0), -1);
    EXPECT_EQ(source.currentWordIndex(10.0), 0);
    EXPECT_EQ(source.currentWordIndex(10.25), 0);
    EXPECT_EQ(source.currentWordIndex(10.5), 1);
    EXPECT_EQ(source.currentWordIndex(11.0), 2);
    EXPECT_EQ(source.currentWordIndex(999.0), 2);
}

TEST(LrcLyricsSourceTest, CurrentWordIndexIsMinusOneWithoutWordLevelData) {
    LrcLyricsSource source(LrcParser::parse("[00:10.00]Plain line\n"));

    EXPECT_EQ(source.currentWordIndex(10.0), -1);
}

} // namespace
