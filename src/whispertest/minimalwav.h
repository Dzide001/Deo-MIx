#pragma once

#include <string>
#include <vector>

namespace whispertest {

// Minimal, self-contained RIFF/WAVE PCM reader -- mirrors
// src/stemsep/wavio.h's own "no libsndfile, just plain PCM WAV" rationale,
// but mono/16kHz (Whisper's required input shape) rather than stereo/44100
// (Demucs's). Supports 16-bit integer and 32-bit float PCM. Fails loudly
// (throws std::runtime_error) on anything else, including stereo input --
// no silent downmixing in this validation-only tool; real downmixing from
// Mixxx's own decoded audio is Stage 2's job, not this one's.
std::vector<float> loadWavMono16kFloat(const std::string& path);

} // namespace whispertest
