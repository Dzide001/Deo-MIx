#pragma once

#include <Eigen/Dense>
#include <string>
#include <vector>

namespace stemsep {

struct StemTrackInfo {
    std::string name;      // e.g. "Vocals" -- matched case-insensitively by
                            // res/qml/Deo/StemPads.qml's findStemIndex()
    std::string colorHex;  // "#RRGGBB"
    Eigen::MatrixXf buffer; // 2 x N, N must equal the premix's column count
};

// Writes a complete, Mixxx-loadable .stem.mp4/.stem.m4a file: muxes `premix`
// (the original track audio, reused verbatim) plus exactly 4 `stems` into a
// 5-stream AAC MP4 (see muxstem.h), then patches in the moov/udta/stem JSON
// metadata atom mixxx::StemInfoImporter reads on load (see stematom.h).
// `stems` must be given in the exact order they should appear in the
// container and in the JSON `stems` array -- both are positional on the
// read side (see plan doc / soundsourcestem.cpp), there is no reordering
// here.
//
// Throws std::runtime_error on shape mismatch (stems.size() != 4, or any
// buffer not 2 rows / not matching premix's column count) or on any
// underlying mux/patch failure. Writes to `outPath + ".tmp"` first and
// renames over `outPath` only on full success, so a failure never leaves a
// truncated file at the caller's requested path.
void writeStemFile(
        const std::string& outPath,
        int sampleRate,
        const Eigen::MatrixXf& premix,
        const std::vector<StemTrackInfo>& stems,
        int aacBitRatePerStreamBps = 256000);

} // namespace stemsep
