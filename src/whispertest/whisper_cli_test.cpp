// M16 Stage 1: standalone validation CLI for the whisper.cpp
// FetchContent integration -- loads a ggml model and a 16kHz mono WAV
// file, runs transcription with word-level timestamps enabled, and
// prints the results. Confirms the "whisper" CMake target links and
// behaves correctly from within Mixxx's own build before any of this
// touches the real app (WhisperTranscriptionJob, Stage 2, reuses this
// same whisper_full() call shape against real track audio instead of a
// WAV file loaded by hand).
//
// Usage: mixxx-whisper-test <model.bin> <audio.wav>

#include <whisper.h>

#include <cstdio>
#include <exception>

#include "minimalwav.h"

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "Usage: %s <model.bin> <audio.wav>\n", argv[0]);
        return 1;
    }
    const char* modelPath = argv[1];
    const char* wavPath = argv[2];

    std::vector<float> samples;
    try {
        samples = whispertest::loadWavMono16kFloat(wavPath);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Failed to load WAV file: %s\n", e.what());
        return 1;
    }

    whisper_context_params cparams = whisper_context_default_params();
    whisper_context* ctx = whisper_init_from_file_with_params(modelPath, cparams);
    if (!ctx) {
        std::fprintf(stderr, "Failed to load model: %s\n", modelPath);
        return 1;
    }

    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    // Word-level timestamps: forcing a short max segment length (in
    // characters) splits output into roughly one word per segment --
    // exactly what a real .lrc's <mm:ss.xx>word tags need. split_on_word
    // ensures the forced split lands on word boundaries, not mid-word.
    wparams.token_timestamps = true;
    wparams.max_len = 1;
    wparams.split_on_word = true;
    wparams.print_progress = false;
    wparams.print_realtime = false;

    if (whisper_full(ctx, wparams, samples.data(), static_cast<int>(samples.size())) != 0) {
        std::fprintf(stderr, "whisper_full() failed\n");
        whisper_free(ctx);
        return 1;
    }

    const int numSegments = whisper_full_n_segments(ctx);
    std::printf("Transcribed %d word/segment(s):\n", numSegments);
    for (int i = 0; i < numSegments; i++) {
        // t0/t1 are in 10ms units.
        const double startSeconds = whisper_full_get_segment_t0(ctx, i) / 100.0;
        const double endSeconds = whisper_full_get_segment_t1(ctx, i) / 100.0;
        const char* text = whisper_full_get_segment_text(ctx, i);
        std::printf("[%7.2f -> %7.2f] %s\n", startSeconds, endSeconds, text);
    }

    whisper_free(ctx);
    return 0;
}
