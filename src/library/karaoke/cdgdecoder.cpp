#include "library/karaoke/cdgdecoder.h"

#include <algorithm>
#include <utility>

namespace mixxx {

namespace {

constexpr int kPackSize = 24;
constexpr int kPacksPerSecond = 300;
constexpr int kTileWidth = 6;
constexpr int kTileHeight = 12;
constexpr int kColumns = CdgDecoder::kWidth / kTileWidth; // 50
constexpr int kRows = CdgDecoder::kHeight / kTileHeight; // 18

// Only the low 6 bits of every CDG data byte are significant (a legacy of
// the underlying CD subchannel's own data format); the command byte's low
// 6 bits must equal this value for the pack to be a CD+G instruction at
// all (anything else in that stream is a no-op as far as CD+G playback
// is concerned).
constexpr quint8 kCdgDataMask = 0x3F;
constexpr quint8 kCdgCommand = 0x09;

enum CdgInstruction {
    kMemoryPreset = 1,
    kBorderPreset = 2,
    kTileBlockNormal = 6,
    kScrollPreset = 20,
    kScrollCopy = 24,
    kDefineTransparentColor = 28,
    kColorTableLow = 30,
    kColorTableHigh = 31,
    kTileBlockXor = 38,
};

// Two data bytes -> one 12-bit RGB color, per the CD+G color table
// command layout: byte0's low 6 bits are [red(4) | green-high(2)], byte1's
// low 6 bits are [green-low(2) | blue(4)]. Each 4-bit channel is scaled to
// 8-bit by *17 (15*17 == 255) for accurate reproduction.
QRgb decodeColor(quint8 byte0, quint8 byte1) {
    const int red = (byte0 >> 2) & 0x0F;
    const int green = ((byte0 & 0x03) << 2) | ((byte1 >> 4) & 0x03);
    const int blue = byte1 & 0x0F;
    return qRgb(red * 17, green * 17, blue * 17);
}

// The persistent, cumulative canvas state a real CD+G player maintains
// across the whole stream -- every instruction below mutates this in
// place, and a snapshot is taken (CdgDecoder::decode()) after any
// instruction that could have visibly changed it.
class Canvas {
  public:
    Canvas() : m_image(CdgDecoder::kWidth, CdgDecoder::kHeight, QImage::Format_Indexed8) {
        QVector<QRgb> palette(16, qRgb(0, 0, 0));
        m_image.setColorTable(palette);
        m_image.fill(0);
    }

    void setColor(int index, QRgb rgb) {
        QVector<QRgb> palette = m_image.colorTable();
        palette[index] = rgb;
        m_image.setColorTable(palette);
    }

    void fill(int colorIndex) {
        m_image.fill(colorIndex);
    }

    // rowBits: 12 bytes, one per pixel-row of the tile, low 6 bits each --
    // bit 5 (0x20) is the leftmost of the tile's 6 pixels, bit 0 (0x01)
    // the rightmost. xorMode implements Tile Block XOR (color38): the
    // existing pixel's palette index is XORed with color0/color1 rather
    // than overwritten, the mechanism real CD+G streams use for blinking/
    // highlight animation on top of already-drawn text.
    void drawTile(int row, int column, int color0, int color1,
            const quint8 rowBits[12], bool xorMode) {
        if (row < 0 || row >= kRows || column < 0 || column >= kColumns) {
            // Malformed/out-of-range coordinates -- ignore rather than
            // erroring, matching this whole module's tolerance for
            // loosely-formed real-world files.
            return;
        }
        const int baseX = column * kTileWidth;
        const int baseY = row * kTileHeight;
        for (int y = 0; y < kTileHeight; y++) {
            uchar* scanline = m_image.scanLine(baseY + y);
            const quint8 bits = rowBits[y] & kCdgDataMask;
            for (int x = 0; x < kTileWidth; x++) {
                const bool set = (bits & (0x20 >> x)) != 0;
                const int newValue = set ? color1 : color0;
                uchar& pixel = scanline[baseX + x];
                pixel = xorMode ? static_cast<uchar>(pixel ^ newValue)
                                 : static_cast<uchar>(newValue);
            }
        }
    }

    QImage snapshot() const {
        return m_image.copy();
    }

  private:
    QImage m_image;
};

} // namespace

QList<CdgDecoder::Frame> CdgDecoder::decode(const QByteArray& fileContents) {
    QList<Frame> frames;
    Canvas canvas;

    const auto* bytes = reinterpret_cast<const uchar*>(fileContents.constData());
    const int packCount = fileContents.size() / kPackSize;

    for (int packIndex = 0; packIndex < packCount; packIndex++) {
        const uchar* pack = bytes + packIndex * kPackSize;
        if ((pack[0] & kCdgDataMask) != kCdgCommand) {
            continue;
        }
        const quint8 instruction = pack[1] & kCdgDataMask;
        const uchar* data = pack + 4; // 16 data bytes

        bool changed = false;
        switch (instruction) {
        case kMemoryPreset: {
            canvas.fill(data[0] & 0x0F);
            changed = true;
            break;
        }
        case kTileBlockNormal:
        case kTileBlockXor: {
            const int color0 = data[0] & 0x0F;
            const int color1 = data[1] & 0x0F;
            const int row = data[2] & 0x1F;
            const int column = data[3] & 0x3F;
            quint8 rowBits[12];
            for (int i = 0; i < 12; i++) {
                rowBits[i] = data[4 + i];
            }
            canvas.drawTile(row, column, color0, color1, rowBits, instruction == kTileBlockXor);
            changed = true;
            break;
        }
        case kColorTableLow:
        case kColorTableHigh: {
            const int base = (instruction == kColorTableLow) ? 0 : 8;
            for (int i = 0; i < 8; i++) {
                canvas.setColor(base + i, decodeColor(data[i * 2], data[i * 2 + 1]));
            }
            changed = true;
            break;
        }
        case kBorderPreset:
        case kScrollPreset:
        case kScrollCopy:
        case kDefineTransparentColor:
        default:
            // Not implemented -- see this file's header comment for why.
            break;
        }

        if (changed) {
            frames.append(Frame{static_cast<double>(packIndex) / kPacksPerSecond,
                    canvas.snapshot()});
        }
    }

    return frames;
}

CdgLyricsSource::CdgLyricsSource(QList<CdgDecoder::Frame> frames) : m_frames(std::move(frames)) {
}

bool CdgLyricsSource::isValid() const {
    return !m_frames.isEmpty();
}

QString CdgLyricsSource::currentLine(double positionSeconds) const {
    Q_UNUSED(positionSeconds);
    return QString();
}

QImage CdgLyricsSource::currentFrame(double positionSeconds) const {
    if (m_frames.isEmpty()) {
        return QImage();
    }
    // m_frames is already sorted by timestamp (built in increasing pack
    // order) -- binary search for the last keyframe at or before this
    // position.
    auto it = std::upper_bound(m_frames.begin(),
            m_frames.end(),
            positionSeconds,
            [](double position, const CdgDecoder::Frame& frame) {
                return position < frame.timestampSeconds;
            });
    if (it == m_frames.begin()) {
        return QImage();
    }
    --it;
    return it->image;
}

} // namespace mixxx
