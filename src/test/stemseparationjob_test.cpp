#include "library/stemseparation/stemseparationjob.h"

#include <gtest/gtest.h>

#include <QFileInfo>
#include <QProcessEnvironment>
#include <QSignalSpy>

#include "test/librarytest.h"
#include "track/beats.h"
#include "track/cue.h"
#include "track/steminfoimporter.h"
#include "track/track.h"

namespace {

// This test drives the real HTDemucs ONNX pipeline end-to-end and needs a
// real model file plus a short real audio fixture -- neither is committed
// to the repo (the model is ~80MB+, licensing/size), so both are supplied
// via environment variables. The test is skipped, not failed, if unset --
// same posture as the manual validation this stage's plan document
// describes for the earlier standalone CLIs.
QString modelPathFromEnv() {
    return QProcessEnvironment::systemEnvironment().value(
            QStringLiteral("MIXXX_STEMSEP_TEST_MODEL_PATH"));
}

QString inputWavPathFromEnv() {
    return QProcessEnvironment::systemEnvironment().value(
            QStringLiteral("MIXXX_STEMSEP_TEST_INPUT_WAV"));
}

} // namespace

class StemSeparationJobTest : public LibraryTest {};

TEST_F(StemSeparationJobTest, SeparatesRegistersAndCopiesMetadata) {
    const QString modelPath = modelPathFromEnv();
    const QString inputWavPath = inputWavPathFromEnv();
    if (modelPath.isEmpty() || inputWavPath.isEmpty()) {
        GTEST_SKIP() << "Set MIXXX_STEMSEP_TEST_MODEL_PATH and "
                        "MIXXX_STEMSEP_TEST_INPUT_WAV to run this test.";
    }
    ASSERT_TRUE(QFileInfo::exists(modelPath));
    ASSERT_TRUE(QFileInfo::exists(inputWavPath));

    TrackPointer pSourceTrack = getOrAddTrackByLocation(inputWavPath);
    ASSERT_TRUE(pSourceTrack);
    ASSERT_TRUE(pSourceTrack->getId().isValid());

    const CuePointer pSourceCue = pSourceTrack->createAndAddCue(
            mixxx::CueType::HotCue,
            0,
            mixxx::audio::FramePos(1000),
            mixxx::audio::FramePos(),
            mixxx::RgbColor(0xFF0000));
    pSourceCue->setLabel(QStringLiteral("Test Cue"));

    const mixxx::BeatsPointer pSourceBeats = mixxx::Beats::fromConstTempo(
            mixxx::audio::SampleRate(44100),
            mixxx::audio::FramePos(0),
            mixxx::Bpm(120.0));
    ASSERT_TRUE(pSourceTrack->trySetBeats(pSourceBeats));

    mixxx::StemSeparationRequest request;
    request.pSourceTrack = pSourceTrack;
    request.modelPath = modelPath;
    request.outputPath = QDir::tempPath() + "/stemseparationjob_test_output.stem.mp4";
    QFile::remove(request.outputPath);

    mixxx::StemSeparationJob job(
            nullptr, trackCollectionManager(), nullptr, QString(), request);
    QSignalSpy finishedSpy(&job, &QThread::finished);
    QSignalSpy completedSpy(&job, &mixxx::StemSeparationJob::completed);
    QSignalSpy failedSpy(&job, &mixxx::StemSeparationJob::failed);

    job.start();
    // CPU HTDemucs inference on a full-length track is slow, especially in
    // a non-optimized debug build -- generous timeout for this opt-in,
    // env-var-gated integration test. If it fires, cancel and wait before
    // `job` goes out of scope: destroying a still-running QThread is fatal.
    if (!finishedSpy.wait(900000)) {
        job.slotCancel();
        job.wait();
        FAIL() << "Job did not finish within 900s";
    }

    if (!failedSpy.isEmpty()) {
        FAIL() << "Job failed: "
               << failedSpy.at(0).at(0).toString().toStdString();
    }
    ASSERT_EQ(completedSpy.size(), 1);

    const TrackPointer pNewTrack =
            completedSpy.at(0).at(0).value<TrackPointer>();
    ASSERT_TRUE(pNewTrack);
    ASSERT_TRUE(pNewTrack->getId().isValid());

    // The file exists and round-trips through the real, unmodified reader
    // (mixxx::StemInfoImporter, src/track/steminfoimporter.cpp) -- same
    // check Stage 2 validated standalone, now exercised via the full job.
    ASSERT_TRUE(QFileInfo::exists(request.outputPath));
    ASSERT_TRUE(mixxx::StemInfoImporter::hasStemAtom(request.outputPath));
    const QList<StemInfo> importedStems =
            mixxx::StemInfoImporter::importStemInfos(request.outputPath);
    ASSERT_EQ(importedStems.size(), 4);
    EXPECT_EQ(importedStems.at(0).getLabel(), QStringLiteral("Vocals"));
    EXPECT_EQ(importedStems.at(1).getLabel(), QStringLiteral("Drums"));
    EXPECT_EQ(importedStems.at(2).getLabel(), QStringLiteral("Bass"));
    EXPECT_EQ(importedStems.at(3).getLabel(), QStringLiteral("Other"));

    // Cue points and beatgrid were copied from the source track.
    const QList<CuePointer> newCues = pNewTrack->getCuePoints();
    ASSERT_EQ(newCues.size(), 1);
    EXPECT_EQ(newCues.at(0)->getPosition(), pSourceCue->getPosition());
    EXPECT_EQ(newCues.at(0)->getLabel(), pSourceCue->getLabel());
    EXPECT_EQ(newCues.at(0)->getColor(), pSourceCue->getColor());

    ASSERT_TRUE(pNewTrack->getBeats());
    EXPECT_EQ(pNewTrack->getBeats(), pSourceTrack->getBeats());

    QFile::remove(request.outputPath);
}
