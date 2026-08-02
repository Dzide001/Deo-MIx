#include "library/karaoke/cdgdecoder.h"

#include <gtest/gtest.h>

using namespace mixxx;

namespace {

// Builds one 24-byte CD+G pack: command byte (0x09, the only value that
// marks a pack as a real CD+G instruction), the given instruction byte,
// 2 unused parity-Q bytes, up to 16 data bytes (zero-padded), and 4
// unused parity-P bytes.
QByteArray makePack(quint8 instruction, const QVector<quint8>& data) {
    QByteArray pack(24, char(0));
    pack[0] = char(0x09);
    pack[1] = char(instruction);
    for (int i = 0; i < data.size() && i < 16; i++) {
        pack[4 + i] = char(data[i]);
    }
    return pack;
}

constexpr quint8 kMemoryPreset = 1;
constexpr quint8 kTileBlockNormal = 6;
constexpr quint8 kColorTableLow = 30;
constexpr quint8 kTileBlockXor = 38;

TEST(CdgDecoderTest, IgnoresNonCdgPacks) {
    QByteArray pack(24, char(0));
    pack[0] = char(0x00); // not the CD+G command marker

    const QList<CdgDecoder::Frame> frames = CdgDecoder::decode(pack);

    EXPECT_TRUE(frames.isEmpty());
}

TEST(CdgDecoderTest, EmptyFileProducesNoFrames) {
    EXPECT_TRUE(CdgDecoder::decode(QByteArray()).isEmpty());
}

TEST(CdgDecoderTest, MemoryPresetFillsWholeCanvas) {
    const QByteArray file = makePack(kMemoryPreset, {5});

    const QList<CdgDecoder::Frame> frames = CdgDecoder::decode(file);

    ASSERT_EQ(frames.size(), 1);
    const QImage& image = frames[0].image;
    ASSERT_EQ(image.width(), CdgDecoder::kWidth);
    ASSERT_EQ(image.height(), CdgDecoder::kHeight);
    EXPECT_EQ(image.pixelIndex(0, 0), 5);
    EXPECT_EQ(image.pixelIndex(CdgDecoder::kWidth - 1, CdgDecoder::kHeight - 1), 5);
}

TEST(CdgDecoderTest, ColorTableLowDecodesRgbCorrectly) {
    // color0 -> pure red (r=15,g=0,b=0), color1 -> pure green (r=0,g=15,b=0)
    // in the 4-bit-per-channel palette; colors 2-7 left at zero/black.
    const QByteArray file = makePack(
            kColorTableLow, {0x3C, 0x00, 0x03, 0x30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});

    const QList<CdgDecoder::Frame> frames = CdgDecoder::decode(file);

    ASSERT_EQ(frames.size(), 1);
    const QVector<QRgb> palette = frames[0].image.colorTable();
    EXPECT_EQ(palette[0], qRgb(255, 0, 0));
    EXPECT_EQ(palette[1], qRgb(0, 255, 0));
}

TEST(CdgDecoderTest, TileBlockNormalDrawsExpectedPixels) {
    // color0=0, color1=1, row=0, column=0, all 12 rows = 0x20 (only the
    // tile's leftmost pixel bit set).
    QVector<quint8> data = {0, 1, 0, 0};
    for (int i = 0; i < 12; i++) {
        data.append(0x20);
    }
    const QByteArray file = makePack(kTileBlockNormal, data);

    const QList<CdgDecoder::Frame> frames = CdgDecoder::decode(file);

    ASSERT_EQ(frames.size(), 1);
    const QImage& image = frames[0].image;
    for (int y = 0; y < 12; y++) {
        EXPECT_EQ(image.pixelIndex(0, y), 1) << "leftmost pixel, row " << y;
        EXPECT_EQ(image.pixelIndex(1, y), 0) << "second pixel, row " << y;
    }
}

TEST(CdgDecoderTest, TileBlockXorTogglesAgainstExistingPixels) {
    QVector<quint8> data = {0, 1, 0, 0};
    for (int i = 0; i < 12; i++) {
        data.append(0x20);
    }
    QByteArray file = makePack(kTileBlockNormal, data);
    // A second, identical draw but XOR mode -- XORing color1 (1) against
    // itself at the bit-set pixel should toggle it back to 0.
    file += makePack(kTileBlockXor, data);

    const QList<CdgDecoder::Frame> frames = CdgDecoder::decode(file);

    ASSERT_EQ(frames.size(), 2);
    const QImage& afterXor = frames[1].image;
    EXPECT_EQ(afterXor.pixelIndex(0, 0), 0);
    EXPECT_EQ(afterXor.pixelIndex(1, 0), 0);
}

TEST(CdgDecoderTest, FrameTimestampsMatchPackIndexOverPacksPerSecond) {
    QByteArray file = makePack(kMemoryPreset, {1});
    // Pad with 9 non-CD+G (skipped) packs so the next real instruction
    // lands at pack index 10.
    for (int i = 0; i < 9; i++) {
        QByteArray filler(24, char(0));
        file += filler;
    }
    file += makePack(kMemoryPreset, {2});

    const QList<CdgDecoder::Frame> frames = CdgDecoder::decode(file);

    ASSERT_EQ(frames.size(), 2);
    EXPECT_DOUBLE_EQ(frames[0].timestampSeconds, 0.0);
    EXPECT_DOUBLE_EQ(frames[1].timestampSeconds, 10.0 / 300.0);
}

TEST(CdgLyricsSourceTest, IsValidReflectsWhetherAnyFramesDecoded) {
    CdgLyricsSource emptySource{QList<CdgDecoder::Frame>()};
    EXPECT_FALSE(emptySource.isValid());

    CdgLyricsSource realSource(CdgDecoder::decode(makePack(kMemoryPreset, {1})));
    EXPECT_TRUE(realSource.isValid());
}

TEST(CdgLyricsSourceTest, CurrentLineIsAlwaysEmpty) {
    CdgLyricsSource source(CdgDecoder::decode(makePack(kMemoryPreset, {1})));
    EXPECT_EQ(source.currentLine(0.0), "");
    EXPECT_EQ(source.currentLine(999.0), "");
}

TEST(CdgLyricsSourceTest, CurrentFrameSeeksToTheRightKeyframe) {
    // Frame 0 at pack index 0 (t=0s, fill color 1), frame 1 at pack index
    // 300 (t=1.0s, fill color 2).
    QByteArray file = makePack(kMemoryPreset, {1});
    for (int i = 0; i < 299; i++) {
        file += QByteArray(24, char(0));
    }
    file += makePack(kMemoryPreset, {2});

    CdgLyricsSource source(CdgDecoder::decode(file));

    EXPECT_EQ(source.currentFrame(0.0).pixelIndex(0, 0), 1);
    EXPECT_EQ(source.currentFrame(0.5).pixelIndex(0, 0), 1);
    EXPECT_EQ(source.currentFrame(1.0).pixelIndex(0, 0), 2);
    EXPECT_EQ(source.currentFrame(999.0).pixelIndex(0, 0), 2);
}

TEST(CdgLyricsSourceTest, CurrentFrameIsNullWithNoData) {
    CdgLyricsSource source{QList<CdgDecoder::Frame>()};
    EXPECT_TRUE(source.currentFrame(0.0).isNull());
}

} // namespace
