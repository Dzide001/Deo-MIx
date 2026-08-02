#include "library/lyrictranscription/whispertranscriptionjob.h"

#include <whisper.h>

#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "audio/types.h"
#include "library/lyrictranscription/whisperlrcbuilder.h"
#include "moc_whispertranscriptionjob.cpp"
#include "sources/soundsourceproxy.h"
#include "track/track.h"
#include "util/indexrange.h"
#include "util/logger.h"

namespace mixxx {

namespace {

const Logger kLogger("WhisperTranscriptionJob");

constexpr SINT kReadChunkFrames = 65536;
// whisper.cpp requires 16kHz mono input -- hardcoded in its own mel
// filterbank computation, not something this job can request otherwise.
constexpr int kWhisperSampleRate = 16000;

// Downmixes interleaved stereo to mono (plain average -- speech
// recognition doesn't need anything more careful than this) and resamples
// to 16kHz via linear interpolation. Not audiophile-grade (no
// anti-aliasing filter), but whisper.cpp's own accuracy tolerance for
// minor resampling artifacts is high -- this is the same class of
// tradeoff real-world lightweight ASR integrations commonly make, and a
// proper windowed-sinc resampler is a well-isolated future improvement to
// this one function alone if ever needed.
std::vector<float> downmixAndResampleToWhisperRate(
        const std::vector<float>& interleavedStereo, int nativeSampleRate) {
    const size_t frameCount = interleavedStereo.size() / 2;
    std::vector<float> mono(frameCount);
    for (size_t i = 0; i < frameCount; i++) {
        mono[i] = 0.5f * (interleavedStereo[2 * i] + interleavedStereo[2 * i + 1]);
    }

    if (nativeSampleRate == kWhisperSampleRate) {
        return mono;
    }

    const double ratio = static_cast<double>(nativeSampleRate) / kWhisperSampleRate;
    const size_t outFrameCount = static_cast<size_t>(frameCount / ratio);
    std::vector<float> resampled(outFrameCount);
    for (size_t i = 0; i < outFrameCount; i++) {
        const double sourcePos = i * ratio;
        const size_t index0 = static_cast<size_t>(sourcePos);
        const size_t index1 = std::min(index0 + 1, frameCount - 1);
        const double frac = sourcePos - index0;
        resampled[i] = static_cast<float>(
                mono[index0] * (1.0 - frac) + mono[index1] * frac);
    }
    return resampled;
}

// M16 Stage 4: picks whisper.cpp's model-specific DTW cross-attention
// alignment-head preset from the model file's name. Plain
// token_timestamps (the timestamp tokens the model itself predicts, used
// alone until this point) is known to drift within a long segment/
// sentence and only resync at the next natural pause -- exactly the
// "words go off the audio and later sync as it goes along" symptom found
// during Stage 4's real-world verification. whisper.cpp's DTW-based
// alignment (cross-attention weights + dynamic time warping, the same
// technique OpenAI's own reference implementation added specifically to
// fix this drift) gives meaningfully tighter per-token timing. It's
// documented as [EXPERIMENTAL] and needs a preset matched to the model
// architecture actually being used (whisper.cpp's own CLI takes this as
// an explicit --dtw flag, since it has no way to introspect a .bin file's
// architecture) -- here it's inferred from the filename convention
// ggml-<model>.bin/ggml-<model>.en.bin uses, longest/most specific match
// first so "large-v3" doesn't fall through to a bare "large" match.
// Falls back to WHISPER_AHEADS_N_TOP_MOST (model-agnostic, needs only
// dtw_n_top) for any filename that doesn't match a known model name.
void configureDtwAlignmentForModel(const QString& modelPath, whisper_context_params* pCparams) {
    const QString name = QFileInfo(modelPath).fileName().toLower();

    struct ModelPreset {
        const char* needle;
        whisper_alignment_heads_preset preset;
    };
    static const ModelPreset kPresets[] = {
            {"large-v3-turbo", WHISPER_AHEADS_LARGE_V3_TURBO},
            {"large-v3", WHISPER_AHEADS_LARGE_V3},
            {"large-v2", WHISPER_AHEADS_LARGE_V2},
            {"large-v1", WHISPER_AHEADS_LARGE_V1},
            {"large", WHISPER_AHEADS_LARGE_V1},
            {"medium.en", WHISPER_AHEADS_MEDIUM_EN},
            {"medium", WHISPER_AHEADS_MEDIUM},
            {"small.en", WHISPER_AHEADS_SMALL_EN},
            {"small", WHISPER_AHEADS_SMALL},
            {"base.en", WHISPER_AHEADS_BASE_EN},
            {"base", WHISPER_AHEADS_BASE},
            {"tiny.en", WHISPER_AHEADS_TINY_EN},
            {"tiny", WHISPER_AHEADS_TINY},
    };

    for (const ModelPreset& candidate : kPresets) {
        if (name.contains(QLatin1String(candidate.needle))) {
            pCparams->dtw_aheads_preset = candidate.preset;
            return;
        }
    }

    // Unrecognized filename -- generic fallback. 3 is safely within
    // [1, n_text_layer] for every shipped model size (even tiny's 4
    // text layers), per whisper.cpp's own validation of dtw_n_top.
    pCparams->dtw_aheads_preset = WHISPER_AHEADS_N_TOP_MOST;
    pCparams->dtw_n_top = 3;
}

// Segment-level t0/t1 (whisper_full_get_segment_t0/t1) come from the
// same drift-prone timestamp-token method configureDtwAlignmentForModel()
// works around -- this instead spans the DTW timestamps of the segment's
// own (non-special) tokens. Special tokens (language/timestamp/control
// tokens) always stringify with a "[_" prefix -- this is whisper.cpp's
// own internal convention (see src/whisper.cpp's vocab-to-string table),
// not a heuristic guess. Falls back to the segment-level t0/t1 if a
// segment has no non-special tokens (e.g. entirely special-token
// content), which shouldn't happen in practice but must not crash if it
// somehow does.
std::pair<double, double> dtwWordSpanSeconds(whisper_context* ctx, int iSegment) {
    double startCentiseconds = -1.0;
    double endCentiseconds = -1.0;
    const int numTokens = whisper_full_n_tokens(ctx, iSegment);
    for (int i = 0; i < numTokens; i++) {
        const QString tokenText = QString::fromUtf8(whisper_full_get_token_text(ctx, iSegment, i));
        if (tokenText.startsWith(QLatin1String("[_"))) {
            continue;
        }
        const whisper_token_data tokenData = whisper_full_get_token_data(ctx, iSegment, i);
        if (startCentiseconds < 0) {
            startCentiseconds = static_cast<double>(tokenData.t_dtw);
        }
        endCentiseconds = static_cast<double>(tokenData.t_dtw);
    }
    if (startCentiseconds < 0) {
        return {whisper_full_get_segment_t0(ctx, iSegment) / 100.0,
                whisper_full_get_segment_t1(ctx, iSegment) / 100.0};
    }
    return {startCentiseconds / 100.0, endCentiseconds / 100.0};
}

} // namespace

WhisperTranscriptionJob::WhisperTranscriptionJob(QObject* parent, WhisperTranscriptionRequest request)
        : QThread(parent), m_request(std::move(request)) {
}

WhisperTranscriptionJob::~WhisperTranscriptionJob() = default;

void WhisperTranscriptionJob::run() {
    if (!QFileInfo::exists(m_request.modelPath)) {
        emit failed(tr("Whisper model not found at %1").arg(m_request.modelPath));
        return;
    }

    mixxx::AudioSource::OpenParams openParams;
    openParams.setChannelCount(mixxx::audio::ChannelCount::stereo());
    mixxx::AudioSourcePointer pAudioSource =
            SoundSourceProxy(m_request.pSourceTrack).openAudioSource(openParams);
    if (!pAudioSource) {
        emit failed(tr("Could not open the track's audio for reading."));
        return;
    }

    const int nativeSampleRate = pAudioSource->getSignalInfo().getSampleRate().value();
    mixxx::IndexRange remainingFrameRange = pAudioSource->frameIndexRange();
    if (remainingFrameRange.empty()) {
        emit failed(tr("Track has no decodable audio."));
        return;
    }

    const SINT totalFrames = remainingFrameRange.length();
    std::vector<float> interleavedStereo(static_cast<size_t>(totalFrames) * 2);
    mixxx::SampleBuffer chunkBuffer(kReadChunkFrames * 2);
    SINT framesRead = 0;
    while (!remainingFrameRange.empty()) {
        if (m_cancellationRequested.loadAcquire() != 0) {
            return;
        }
        const auto chunkFrameRange = remainingFrameRange.splitAndShrinkFront(
                std::min<SINT>(kReadChunkFrames, remainingFrameRange.length()));
        const auto readableSampleFrames = pAudioSource->readSampleFrames(
                mixxx::WritableSampleFrames(
                        chunkFrameRange,
                        mixxx::SampleBuffer::WritableSlice(chunkBuffer)));
        const SINT chunkFrames = readableSampleFrames.frameLength();
        const CSAMPLE* pData = readableSampleFrames.readableData();
        for (SINT i = 0; i < chunkFrames * 2; i++) {
            interleavedStereo[static_cast<size_t>(framesRead) * 2 + i] = pData[i];
        }
        framesRead += chunkFrameRange.length();
        emit jobProgress(
                0.2f * framesRead / static_cast<float>(totalFrames), tr("Reading audio..."));
    }
    interleavedStereo.resize(static_cast<size_t>(framesRead) * 2);

    emit jobProgress(0.2f, tr("Preparing audio..."));
    const std::vector<float> whisperInput =
            downmixAndResampleToWhisperRate(interleavedStereo, nativeSampleRate);

    if (m_cancellationRequested.loadAcquire() != 0) {
        return;
    }

    emit jobProgress(0.25f, tr("Loading model..."));
    whisper_context_params cparams = whisper_context_default_params();
    cparams.dtw_token_timestamps = true;
    configureDtwAlignmentForModel(m_request.modelPath, &cparams);
    whisper_context* ctx =
            whisper_init_from_file_with_params(m_request.modelPath.toUtf8().constData(), cparams);
    if (!ctx) {
        emit failed(tr("Failed to load the whisper model."));
        return;
    }

    if (m_cancellationRequested.loadAcquire() != 0) {
        whisper_free(ctx);
        return;
    }

    emit jobProgress(0.3f, tr("Transcribing..."));
    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.token_timestamps = true;
    wparams.max_len = 1;
    wparams.split_on_word = true;
    wparams.print_progress = false;
    wparams.print_realtime = false;
    wparams.print_special = false;

    // whisper_full() itself isn't interruptible mid-call -- a
    // cancellation requested while it runs still stops us before writing
    // anything, same tradeoff StemSeparationJob accepts for its own
    // uninterruptible ONNX Runtime inference call.
    const int result = whisper_full(
            ctx, wparams, whisperInput.data(), static_cast<int>(whisperInput.size()));
    if (result != 0) {
        whisper_free(ctx);
        emit failed(tr("Transcription failed."));
        return;
    }

    if (m_cancellationRequested.loadAcquire() != 0) {
        whisper_free(ctx);
        return;
    }

    emit jobProgress(0.9f, tr("Writing lyrics file..."));
    std::vector<WhisperWordTiming> words;
    const int numSegments = whisper_full_n_segments(ctx);
    words.reserve(static_cast<size_t>(numSegments));
    for (int i = 0; i < numSegments; i++) {
        const QString segmentText =
                QString::fromUtf8(whisper_full_get_segment_text(ctx, i)).trimmed();
        if (segmentText.isEmpty()) {
            continue;
        }
        const auto [startSeconds, endSeconds] = dtwWordSpanSeconds(ctx, i);
        const std::vector<WhisperWordTiming> segmentWords =
                WhisperLrcBuilder::splitSegmentIntoWords(startSeconds, endSeconds, segmentText);
        words.insert(words.end(), segmentWords.begin(), segmentWords.end());
    }
    whisper_free(ctx);

    if (words.empty()) {
        emit failed(tr("No speech detected in this track."));
        return;
    }

    const QString lrcContents = WhisperLrcBuilder::buildEnhancedLrc(words);
    QFile lrcFile(m_request.outputLrcPath);
    if (!lrcFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit failed(tr("Failed to write %1").arg(m_request.outputLrcPath));
        return;
    }
    lrcFile.write(lrcContents.toUtf8());
    lrcFile.close();

    kLogger.debug() << "Wrote transcribed lyrics to" << m_request.outputLrcPath;
    emit jobProgress(1.0f, tr("Done"));
    emit completed(m_request.deckGroup, m_request.outputLrcPath);
}

void WhisperTranscriptionJob::slotCancel() {
    m_cancellationRequested = 1;
}

} // namespace mixxx
