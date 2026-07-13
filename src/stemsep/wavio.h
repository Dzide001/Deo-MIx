#pragma once

#include <Eigen/Dense>
#include <string>

namespace stemsep {

// Minimal, self-contained RIFF/WAVE PCM reader/writer -- stage 1 doesn't
// need libsndfile's full codec support (FLAC/Ogg/MP3/etc, which pulls in a
// pile of transitive static-link dependencies mixxx-lib gets "for free"
// elsewhere but this standalone target does not), only plain PCM WAV.
// Supports 16-bit integer and 32-bit float PCM on read; always writes
// 32-bit float.

// Loads a WAV file as a 2-channel (stereo) float buffer, sample rate
// stemsep expects (44100 Hz, matching demucsonnx). Fails loudly (throws
// std::runtime_error) on anything else -- no silent resampling in this
// stage, and mono input is not upmixed.
Eigen::MatrixXf loadWavStereoFloat(const std::string& path, int expectedSampleRate);

// Writes a 2-channel float buffer (rows = channels, cols = samples) as a
// 32-bit float WAV file at the given sample rate.
void writeWavStereoFloat(
        const std::string& path, const Eigen::MatrixXf& buffer, int sampleRate);

} // namespace stemsep
