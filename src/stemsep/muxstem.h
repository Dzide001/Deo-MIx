#pragma once

#include <Eigen/Dense>
#include <array>
#include <string>

namespace stemsep {

// Muxes 5 stereo AAC-LC audio streams (index 0 = premix, 1-4 = stems, in
// caller-supplied order) into a single MP4 file at `sampleRate`, matching
// the container layout mixxx::SoundSourceSTEM expects on read
// (src/sources/soundsourcestem.cpp): exactly 5 audio streams, all stereo,
// streams 1-4 sharing codec/sample rate. All 5 streams are encoded with an
// identical AVCodecContext configuration so AAC's encoder-delay
// (initial_padding) is numerically equal across all of them -- readSampleFramesClamped
// reads the same frame-index range from every stream with no per-stream
// timing correction, so a mismatch here would show up as audible
// inter-stem phase drift. Does not write the moov/udta/stem metadata atom
// -- see stematom.h for that, applied as a post-processing step. Does not
// set movflags=faststart, so that patchStemAtom()'s "moov is the last
// top-level box" precondition holds.
//
// Throws std::runtime_error on any FFmpeg failure or shape mismatch
// (premix/stems not 2 rows, or stems' column counts not all equal to
// premix's).
void muxStemContainer(
        const std::string& outPath,
        int sampleRate,
        const Eigen::MatrixXf& premix,
        const std::array<Eigen::MatrixXf, 4>& stems,
        int bitRatePerStreamBps = 256000);

} // namespace stemsep
