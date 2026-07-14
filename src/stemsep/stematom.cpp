#include "stematom.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace stemsep {

namespace {

uint32_t readU32BE(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
            (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

void writeU32BE(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>((v >> 24) & 0xff);
    p[1] = static_cast<uint8_t>((v >> 16) & 0xff);
    p[2] = static_cast<uint8_t>((v >> 8) & 0xff);
    p[3] = static_cast<uint8_t>(v & 0xff);
}

struct TopLevelBox {
    std::string type;
    uint64_t offset;    // offset of the box's own [size][type] header from file start
    uint64_t totalSize; // total box size including header
};

// Walks top-level (depth-0) boxes of an MP4 file. Calls `visit` for each;
// stops early if `visit` returns false. Mirrors the box-walking style of
// MP4BoxHeader/seekTillAtom in src/track/steminfoimporter.cpp, adapted to
// operate on a plain std::ifstream instead of a QIODevice.
template <typename Visit>
void walkTopLevelBoxes(std::ifstream& f, uint64_t fileSize, Visit visit) {
    uint64_t offset = 0;
    while (offset + 8 <= fileSize) {
        f.seekg(static_cast<std::streamoff>(offset));
        uint8_t header[8];
        f.read(reinterpret_cast<char*>(header), 8);
        if (!f) {
            throw std::runtime_error("stemsep: stematom: short read on box header");
        }
        uint64_t boxSize = readU32BE(header);
        std::string type(reinterpret_cast<char*>(header + 4), 4);
        uint64_t headerSize = 8;
        if (boxSize == 1) {
            uint8_t largesize[8];
            f.read(reinterpret_cast<char*>(largesize), 8);
            if (!f) {
                throw std::runtime_error("stemsep: stematom: short read on extended box size");
            }
            boxSize = (static_cast<uint64_t>(readU32BE(largesize)) << 32) |
                    readU32BE(largesize + 4);
            headerSize = 16;
        } else if (boxSize == 0) {
            // Box extends to end of file (ISO-BMFF convention for the last box).
            boxSize = fileSize - offset;
        }
        if (boxSize < headerSize || offset + boxSize > fileSize) {
            throw std::runtime_error("stemsep: stematom: malformed box size for '" + type + "'");
        }
        if (!visit(TopLevelBox{type, offset, boxSize})) {
            return;
        }
        offset += boxSize;
    }
}

struct ChildBox {
    std::string type;
    uint64_t offset; // offset within the parent's in-memory buffer
    uint64_t totalSize;
};

// Walks depth-1 boxes inside an in-memory buffer that starts at a box's own
// header (buf[0..8) is that box's [size][type]); `payloadStart` is where the
// first child box begins (8 for a box with no version/flags fields, like moov).
std::vector<ChildBox> walkChildBoxes(const std::vector<uint8_t>& buf, uint64_t payloadStart) {
    std::vector<ChildBox> children;
    uint64_t offset = payloadStart;
    while (offset + 8 <= buf.size()) {
        uint64_t boxSize = readU32BE(&buf[offset]);
        std::string type(reinterpret_cast<const char*>(&buf[offset + 4]), 4);
        uint64_t headerSize = 8;
        if (boxSize == 1) {
            if (offset + 16 > buf.size()) {
                throw std::runtime_error("stemsep: stematom: truncated extended child box");
            }
            boxSize = (static_cast<uint64_t>(readU32BE(&buf[offset + 8])) << 32) |
                    readU32BE(&buf[offset + 12]);
            headerSize = 16;
        } else if (boxSize == 0) {
            boxSize = buf.size() - offset;
        }
        if (boxSize < headerSize || offset + boxSize > buf.size()) {
            throw std::runtime_error(
                    "stemsep: stematom: malformed child box size for '" + type + "'");
        }
        children.push_back(ChildBox{type, offset, boxSize});
        offset += boxSize;
    }
    return children;
}

std::vector<uint8_t> buildBox(const std::string& type, const std::vector<uint8_t>& payload) {
    if (type.size() != 4) {
        throw std::runtime_error("stemsep: stematom: box type must be 4 characters");
    }
    const uint64_t totalSize = 8 + payload.size();
    if (totalSize > 0xFFFFFFFFULL) {
        // Payloads here are always a small JSON manifest -- not worth
        // supporting the 64-bit extended-size box form.
        throw std::runtime_error(
                "stemsep: stematom: box payload too large for a 32-bit box size");
    }
    std::vector<uint8_t> box(totalSize);
    writeU32BE(box.data(), static_cast<uint32_t>(totalSize));
    std::memcpy(box.data() + 4, type.data(), 4);
    if (!payload.empty()) {
        std::memcpy(box.data() + 8, payload.data(), payload.size());
    }
    return box;
}

} // namespace

void patchStemAtom(const std::string& mp4Path, const std::string& manifestJson) {
    uint64_t fileSize;
    {
        std::ifstream sizeCheck(mp4Path, std::ios::binary | std::ios::ate);
        if (!sizeCheck) {
            throw std::runtime_error("stemsep: stematom: could not open " + mp4Path);
        }
        fileSize = static_cast<uint64_t>(sizeCheck.tellg());
    }

    std::ifstream in(mp4Path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("stemsep: stematom: could not open " + mp4Path);
    }

    TopLevelBox moovBox{};
    bool foundMoov = false;
    walkTopLevelBoxes(in, fileSize, [&](const TopLevelBox& box) {
        if (box.type == "moov") {
            moovBox = box;
            foundMoov = true;
            return false;
        }
        return true;
    });
    if (!foundMoov) {
        throw std::runtime_error("stemsep: stematom: no moov box found in " + mp4Path);
    }
    if (moovBox.offset + moovBox.totalSize != fileSize) {
        throw std::runtime_error(
                "stemsep: stematom: moov is not the last top-level box in " + mp4Path +
                " (writer must not use movflags=faststart)");
    }

    std::vector<uint8_t> moovBuf(moovBox.totalSize);
    in.seekg(static_cast<std::streamoff>(moovBox.offset));
    in.read(reinterpret_cast<char*>(moovBuf.data()), static_cast<std::streamsize>(moovBuf.size()));
    if (!in) {
        throw std::runtime_error("stemsep: stematom: short read on moov box");
    }
    in.close();

    const std::vector<ChildBox> children = walkChildBoxes(moovBuf, 8);
    const ChildBox* udta = nullptr;
    for (const auto& child : children) {
        if (child.type == "udta") {
            udta = &child;
            break;
        }
    }

    const std::vector<uint8_t> stemPayload(manifestJson.begin(), manifestJson.end());
    const std::vector<uint8_t> stemBox = buildBox("stem", stemPayload);

    if (udta != nullptr) {
        // Splice the new "stem" box in as the last child of the existing
        // "udta" box, then grow udta's and moov's own size fields.
        const uint64_t insertAt = udta->offset + udta->totalSize;
        const uint64_t udtaHeaderOffset = udta->offset;
        moovBuf.insert(moovBuf.begin() + static_cast<std::ptrdiff_t>(insertAt),
                stemBox.begin(),
                stemBox.end());
        const uint32_t newUdtaSize =
                readU32BE(&moovBuf[udtaHeaderOffset]) + static_cast<uint32_t>(stemBox.size());
        writeU32BE(&moovBuf[udtaHeaderOffset], newUdtaSize);
    } else {
        // No udta box exists yet -- wrap "stem" in a fresh "udta" and
        // append it as the last child of moov.
        const std::vector<uint8_t> udtaBox = buildBox("udta", stemBox);
        moovBuf.insert(moovBuf.end(), udtaBox.begin(), udtaBox.end());
    }

    const uint64_t newMoovSize = moovBuf.size();
    if (newMoovSize > 0xFFFFFFFFULL) {
        throw std::runtime_error("stemsep: stematom: patched moov box exceeds 32-bit size limit");
    }
    writeU32BE(moovBuf.data(), static_cast<uint32_t>(newMoovSize));

    // moov was the last top-level box (precondition, verified above), so
    // patching is a pure rewrite of the file tail from moov's start offset:
    // no bytes before moov are touched, and no stco/co64 chunk-offset table
    // anywhere in moov needs adjusting, since nothing in mdat moved. The
    // file only ever grows here (a stem box is always being added), so this
    // write extends the file in place with no separate truncate step.
    std::fstream out(mp4Path, std::ios::binary | std::ios::in | std::ios::out);
    if (!out) {
        throw std::runtime_error("stemsep: stematom: could not reopen " + mp4Path + " for writing");
    }
    out.seekp(static_cast<std::streamoff>(moovBox.offset));
    out.write(reinterpret_cast<const char*>(moovBuf.data()),
            static_cast<std::streamsize>(moovBuf.size()));
    if (!out) {
        throw std::runtime_error("stemsep: stematom: failed writing patched moov box");
    }
}

} // namespace stemsep
