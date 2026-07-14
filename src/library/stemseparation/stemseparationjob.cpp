#include "library/stemseparation/stemseparationjob.h"

#include <demucs.hpp>

#include <QFile>
#include <QFileInfo>
#include <QMetaObject>

#include <algorithm>
#include <array>
#include <thread>

#include "audio/types.h"
#include "library/stemseparation/trackmetadatacopy.h"
#include "library/trackcollectionmanager.h"
#include "mixer/basetrackplayer.h"
#include "mixer/playermanager.h"
#include "moc_stemseparationjob.cpp"
#include "sources/soundsourceproxy.h"
#include "stemwriter.h"
#include "track/track.h"
#include "track/trackref.h"
#include "util/indexrange.h"
#include "util/logger.h"
#include "util/thread_affinity.h"

namespace mixxx {

namespace {

const Logger kLogger("StemSeparationJob");

// demucsonnx (lib/demucsonnx/dsp.hpp) hardcodes its internal segment-length
// math to this rate and does not itself validate the audio it's given --
// feeding it audio at any other rate would silently misalign timing.
constexpr int kRequiredSampleRate = 44100;
constexpr SINT kReadChunkFrames = 65536;

// HTDemucs' fixed training/export order (see lib/demucsonnx, Stage 1:
// demucs.api.Separator(model='htdemucs').model.sources).
enum DemucsSourceIndex {
    kDemucsDrums = 0,
    kDemucsBass = 1,
    kDemucsOther = 2,
    kDemucsVocals = 3,
};

struct CanonicalStemSlot {
    int demucsIndex;
    const char* name;
    const char* colorHex;
};

// Canonical container/JSON order this pipeline commits to (Stage 2,
// stemsep::writeStemFile) -- colors reuse mixxx::StemInfoImporter's own
// default palette (kStemDefaultColor in steminfoimporter.cpp), index-for-index.
constexpr std::array<CanonicalStemSlot, 4> kCanonicalStemOrder = {{
        {kDemucsVocals, "Vocals", "#009E73"},
        {kDemucsDrums, "Drums", "#D55E00"},
        {kDemucsBass, "Bass", "#CC79A7"},
        {kDemucsOther, "Other", "#56B4E9"},
}};

std::vector<char> readFileBytes(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("could not open " + path.toStdString());
    }
    const QByteArray bytes = file.readAll();
    return std::vector<char>(bytes.constBegin(), bytes.constEnd());
}

} // namespace

StemSeparationJob::StemSeparationJob(
        QObject* parent,
        TrackCollectionManager* pTrackCollectionManager,
        PlayerManager* pPlayerManager,
        const QString& targetDeckGroup,
        StemSeparationRequest request)
        : QThread(parent),
          m_pTrackCollectionManager(pTrackCollectionManager),
          m_pPlayerManager(pPlayerManager),
          m_targetDeckGroup(targetDeckGroup),
          m_request(std::move(request)) {
    DEBUG_ASSERT(m_pTrackCollectionManager);
    // Must be collocated with the TrackCollectionManager -- registerAndReload()
    // is invoked on this object's thread via a blocking queued connection.
    if (parent != nullptr) {
        DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(m_pTrackCollectionManager);
    } else {
        moveToThread(m_pTrackCollectionManager->thread());
    }
}

StemSeparationJob::~StemSeparationJob() = default;

void StemSeparationJob::run() {
    if (!QFileInfo::exists(m_request.modelPath)) {
        m_lastErrorMessage = tr("HTDemucs model not found at %1").arg(m_request.modelPath);
        emit failed(m_lastErrorMessage);
        return;
    }

    // Step 1: read the whole source track's audio, as plain stereo (same
    // request SoundSourceSTEM itself uses to get a "flat" mix out of an
    // already-multi-stem file -- see soundsourcestem.cpp).
    mixxx::AudioSource::OpenParams openParams;
    openParams.setChannelCount(mixxx::audio::ChannelCount::stereo());
    mixxx::AudioSourcePointer pAudioSource =
            SoundSourceProxy(m_request.pSourceTrack).openAudioSource(openParams);
    if (!pAudioSource) {
        m_lastErrorMessage = tr("Could not open the track's audio for reading.");
        emit failed(m_lastErrorMessage);
        return;
    }

    const int sampleRate = pAudioSource->getSignalInfo().getSampleRate().value();
    if (sampleRate != kRequiredSampleRate) {
        m_lastErrorMessage =
                tr("Track sample rate is %1 Hz; stem separation requires %2 Hz.")
                        .arg(sampleRate)
                        .arg(kRequiredSampleRate);
        emit failed(m_lastErrorMessage);
        return;
    }

    mixxx::IndexRange remainingFrameRange = pAudioSource->frameIndexRange();
    if (remainingFrameRange.empty()) {
        m_lastErrorMessage = tr("Track has no decodable audio.");
        emit failed(m_lastErrorMessage);
        return;
    }

    const SINT totalFrames = remainingFrameRange.length();
    Eigen::MatrixXf premix(2, totalFrames);
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
        for (SINT i = 0; i < chunkFrames; ++i) {
            premix(0, framesRead + i) = pData[2 * i];
            premix(1, framesRead + i) = pData[2 * i + 1];
        }
        // Zero-fill any short read (end-of-stream duration inaccuracy)
        // rather than leaving uninitialized samples.
        for (SINT i = chunkFrames; i < chunkFrameRange.length(); ++i) {
            premix(0, framesRead + i) = 0.0f;
            premix(1, framesRead + i) = 0.0f;
        }
        framesRead += chunkFrameRange.length();
        emit jobProgress(0.1f * framesRead / static_cast<float>(totalFrames),
                tr("Reading audio..."));
    }

    emit jobProgress(0.1f, tr("Loading model..."));

    demucsonnx::demucs_model model;
    try {
        Ort::SessionOptions sessionOptions;
        // Single-threaded was Stage 1's deliberate choice for deterministic
        // validation, never revisited since -- use all available cores for
        // real use, ONNX Runtime's math kernels parallelize well.
        sessionOptions.SetIntraOpNumThreads(
                std::max(1u, std::thread::hardware_concurrency()));
        if (!demucsonnx::load_model(readFileBytes(m_request.modelPath), model, sessionOptions)) {
            m_lastErrorMessage = tr("Failed to load the HTDemucs model.");
            emit failed(m_lastErrorMessage);
            return;
        }
    } catch (const std::exception& e) {
        m_lastErrorMessage = tr("Failed to load the HTDemucs model: %1").arg(e.what());
        emit failed(m_lastErrorMessage);
        return;
    }

    if (m_cancellationRequested.loadAcquire() != 0) {
        return;
    }

    Eigen::Tensor3dXf separated;
    try {
        separated = demucsonnx::demucs_inference(model,
                premix,
                [this](float fraction, const std::string& message) {
                    emit jobProgress(0.1f + 0.8f * fraction, QString::fromStdString(message));
                });
    } catch (const std::exception& e) {
        m_lastErrorMessage = tr("Stem separation failed: %1").arg(e.what());
        emit failed(m_lastErrorMessage);
        return;
    }

    // A running inference call is not safely interruptible mid-call (ONNX
    // Runtime sessions aren't designed for that) -- but a cancellation
    // requested while it ran still stops us before touching the library.
    if (m_cancellationRequested.loadAcquire() != 0) {
        return;
    }

    emit jobProgress(0.9f, tr("Writing stem file..."));

    std::vector<stemsep::StemTrackInfo> stemInfos;
    stemInfos.reserve(kCanonicalStemOrder.size());
    for (const auto& slot : kCanonicalStemOrder) {
        stemsep::StemTrackInfo info;
        info.name = slot.name;
        info.colorHex = slot.colorHex;
        info.buffer = Eigen::MatrixXf(2, totalFrames);
        for (SINT ch = 0; ch < 2; ++ch) {
            for (SINT i = 0; i < totalFrames; ++i) {
                info.buffer(ch, i) = separated(slot.demucsIndex, ch, i);
            }
        }
        stemInfos.push_back(std::move(info));
    }

    const QString tmpPath = m_request.outputPath + ".tmp";
    try {
        stemsep::writeStemFile(tmpPath.toStdString(), sampleRate, premix, stemInfos);
    } catch (const std::exception& e) {
        m_lastErrorMessage = tr("Failed to write the stem file: %1").arg(e.what());
        emit failed(m_lastErrorMessage);
        return;
    }
    QFile::remove(m_request.outputPath);
    if (!QFile::rename(tmpPath, m_request.outputPath)) {
        m_lastErrorMessage = tr("Failed to finalize the stem file.");
        emit failed(m_lastErrorMessage);
        return;
    }

    if (m_cancellationRequested.loadAcquire() != 0) {
        return;
    }

    m_writtenStemFilePath = m_request.outputPath;
    emit jobProgress(0.95f, tr("Registering track..."));

    // Note that this must happen on the same thread as the track
    // collection manager, which is not this method's worker thread.
    QMetaObject::invokeMethod(this, "registerAndReload", Qt::BlockingQueuedConnection);

    if (!m_pRegisteredTrack) {
        emit failed(m_lastErrorMessage.isEmpty()
                        ? tr("Failed to register the separated stem file.")
                        : m_lastErrorMessage);
        return;
    }

    emit jobProgress(1.0f, tr("Done"));
    emit completed(m_pRegisteredTrack);
}

void StemSeparationJob::registerAndReload() {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(m_pTrackCollectionManager);

    const auto trackRef = TrackRef::fromFilePath(m_writtenStemFilePath);
    TrackPointer pNewTrack = m_pTrackCollectionManager->getOrAddTrack(trackRef);
    if (!pNewTrack) {
        m_lastErrorMessage = tr("Failed to add the separated stem file to the library.");
        return;
    }

    mixxx::stemseparation::copyTimelineMetadata(*m_request.pSourceTrack, *pNewTrack);

    if (!m_targetDeckGroup.isEmpty() && m_pPlayerManager) {
        BaseTrackPlayer* pPlayer = m_pPlayerManager->getPlayer(m_targetDeckGroup);
        if (pPlayer && pPlayer->getLoadedTrack() == m_request.pSourceTrack) {
            // Only reload if the deck still holds the original source
            // track -- if the user moved on mid-job, still register the
            // stem track in the library, just don't yank what's now playing.
            m_pPlayerManager->slotLoadTrackToPlayer(
                    pNewTrack, m_targetDeckGroup, mixxx::StemChannelSelection(), false);
        }
    }

    m_pRegisteredTrack = pNewTrack;
}

void StemSeparationJob::slotCancel() {
    m_cancellationRequested = 1;
}

} // namespace mixxx
