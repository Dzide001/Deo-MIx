#include "library/videoengine/videoenginemanager.h"

#include <gtest/gtest.h>

#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QImage>
#include <QThread>

#include "control/controlobject.h"
#include "test/mixxxtest.h"

namespace {

// 16x16, 15-frame, solid-color .y4m fixtures (~15KB each, committed
// directly in src/test/ following the sine-30.wav/stems/*.wav precedent):
// uncompressed YUV4MPEG needs no decoder plugin beyond y4mdec itself, so
// this test exercises the real decodebin-based loadVideo() path without
// pulling in a real codec or a large binary asset. White (R=255) for deck
// A, blue (R=0) for deck B makes the composited red channel a simple,
// predictable measurement of which source is dominant.
QString deckAFixturePath() {
    return MixxxTest::getOrInitTestDir().filePath(QStringLiteral("deckA_white.y4m"));
}
QString deckBFixturePath() {
    return MixxxTest::getOrInitTestDir().filePath(QStringLiteral("deckB_blue.y4m"));
}

// Stage 4 (DeckPauseIsIndependentOfOtherDeck): the fixtures above are
// solid-color and non-animated, which can't distinguish "genuinely
// paused, frozen on one frame" from "genuinely playing" -- both look
// pixel-identical over time either way. These two are a short animated
// pattern (ffmpeg testsrc, 16x16, 30 frames @ 30fps) instead, generated
// the same uncompressed-YUV4MPEG way as the fixtures above.
QString deckAAnimatedFixturePath() {
    return MixxxTest::getOrInitTestDir().filePath(QStringLiteral("deckA_animated.y4m"));
}
QString deckBAnimatedFixturePath() {
    return MixxxTest::getOrInitTestDir().filePath(QStringLiteral("deckB_animated.y4m"));
}

// Stage 4: grabPreviewFrame() no longer needs a real-time wait after a
// crossfader change to reflect it (see CrossfadeBlendsRealPerDeckVideoFiles'
// own comment) but freshly (re)built pipelines can still take a little
// longer than gst_element_get_state()'s own bounded wait to deliver their
// first real frame into an appsink -- this bounds how long a test waits
// for grabPreviewFrame() to stop returning null, rather than a single
// fixed sleep guess.
QImage grabFirstNonNullFrame(mixxx::VideoEngineManager* pManager) {
    QImage frame;
    for (int attempt = 0; attempt < 10 && frame.isNull(); ++attempt) {
        frame = pManager->grabPreviewFrame();
    }
    return frame;
}

// VideoEngineManager's EOS-loop restart (and, in general, anything reached
// via a cross-thread Qt::QueuedConnection, e.g. a GStreamer pad-probe
// callback queuing a call back onto this thread) only actually runs once
// something pumps this thread's event queue -- QThread::msleep() blocks
// the calling thread WITHOUT doing that, so a bare msleep() here would
// wait out real time while such a queued call sits pending, undelivered,
// the whole time. Use this instead of msleep() wherever a test needs to
// let real time pass for a clip to play/loop.
void pumpEventsFor(int totalMs) {
    QElapsedTimer timer;
    timer.start();
    do {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    } while (timer.elapsed() < totalMs);
}

} // namespace

class VideoEngineManagerTest : public MixxxTest {
  protected:
    void SetUp() override {
        MixxxTest::SetUp();
        // VideoEngineManager expects "[Master]/crossfader" and each deck's
        // "play"/"playposition" COs to already exist (EngineMixer/
        // EngineBuffer normally create these before CoreServices
        // constructs VideoEngineManager) -- same
        // create-the-CO-directly-in-the-fixture approach LibraryTest uses
        // for m_keyNotationCO, rather than standing up a full EngineMixer.
        m_pCrossfaderCO = std::make_unique<ControlObject>(ConfigKey("[Master]", "crossfader"));
        m_pDeckAPlayCO = std::make_unique<ControlObject>(ConfigKey("[Channel1]", "play"));
        m_pDeckBPlayCO = std::make_unique<ControlObject>(ConfigKey("[Channel2]", "play"));
        m_pDeckAPositionCO = std::make_unique<ControlObject>(ConfigKey("[Channel1]", "playposition"));
        m_pDeckBPositionCO = std::make_unique<ControlObject>(ConfigKey("[Channel2]", "playposition"));
        m_pManager = std::make_unique<mixxx::VideoEngineManager>(nullptr);
    }

    void TearDown() override {
        m_pManager.reset();
        m_pCrossfaderCO.reset();
        m_pDeckAPlayCO.reset();
        m_pDeckBPlayCO.reset();
        m_pDeckAPositionCO.reset();
        m_pDeckBPositionCO.reset();
        MixxxTest::TearDown();
    }

    std::unique_ptr<ControlObject> m_pCrossfaderCO;
    std::unique_ptr<ControlObject> m_pDeckAPlayCO;
    std::unique_ptr<ControlObject> m_pDeckBPlayCO;
    std::unique_ptr<ControlObject> m_pDeckAPositionCO;
    std::unique_ptr<ControlObject> m_pDeckBPositionCO;
    std::unique_ptr<mixxx::VideoEngineManager> m_pManager;
};

TEST_F(VideoEngineManagerTest, RejectsUnknownDeckGroup) {
    if (!m_pManager->isAvailable()) {
        GTEST_SKIP() << "VideoEngineManager reports GStreamer unavailable";
    }
    EXPECT_FALSE(m_pManager->loadVideo(QStringLiteral("[Channel3]"), deckAFixturePath()));
}

TEST_F(VideoEngineManagerTest, CrossfadeBlendsRealPerDeckVideoFiles) {
    if (!m_pManager->isAvailable()) {
        GTEST_SKIP() << "VideoEngineManager reports GStreamer unavailable";
    }
    ASSERT_TRUE(QFileInfo::exists(deckAFixturePath()));
    ASSERT_TRUE(QFileInfo::exists(deckBFixturePath()));

    // Stage 3i: the pipeline now stays PAUSED unless at least one deck's
    // own "play" CO is active (so a freshly loaded clip doesn't auto-play
    // on a paused deck) -- this test is about the crossfade math, not
    // play/pause behavior, so mark deck A "playing" to keep the pipeline
    // actively rendering for the checks below.
    //
    // Stage 3v: VideoEngineManager's play CO subscription now explicitly
    // requests Qt::QueuedConnection (see the class doc comment), which
    // only ever gets delivered by an actual Qt event loop processing the
    // posted event -- this test's fixture never spins one on its own, so
    // pump it once directly to let the now-queued notification land
    // before proceeding, the same way the real app's own running event
    // loop would.
    m_pDeckAPlayCO->set(1.0);
    QCoreApplication::processEvents();

    ASSERT_TRUE(m_pManager->loadVideo(QStringLiteral("[Channel1]"), deckAFixturePath()));
    ASSERT_TRUE(m_pManager->loadVideo(QStringLiteral("[Channel2]"), deckBFixturePath()));

    // Same three representative crossfader positions Stage 3d-1's
    // standalone CLI already proved the alpha mapping against
    // (mixxx-videoengine-crossfade-test) -- this test proves the same
    // thing through the real ControlObject-driven path, with real decoded
    // files instead of synthetic videotestsrc patterns.
    //
    // Stage 4: no more real-time wait needed between crossfader changes.
    // Previously, the alpha value was written into a GStreamer compositor
    // pad's property and only took effect on the compositor's *next*
    // rendered frame -- pulling immediately after set() could race ahead
    // of that. Since blending moved into C++ (blendFrames(), called from
    // grabPreviewFrame()), the crossfader value is read fresh at grab
    // time and applied to whatever frame is already cached per deck, so
    // there's no "next frame" to wait for anymore -- each grabPreviewFrame()
    // call after a crossfader set() already reflects it immediately.
    QImage firstFrame = grabFirstNonNullFrame(m_pManager.get());
    ASSERT_FALSE(firstFrame.isNull()) << "both decks should have delivered a first real frame";

    m_pCrossfaderCO->set(-1.0);
    QImage frameA = m_pManager->grabPreviewFrame();
    ASSERT_FALSE(frameA.isNull());
    int redAtA = frameA.pixelColor(frameA.width() / 2, frameA.height() / 2).red();
    EXPECT_GT(redAtA, 200) << "deck A (white) should dominate at crossfader position -1";

    m_pCrossfaderCO->set(1.0);
    QImage frameB = m_pManager->grabPreviewFrame();
    ASSERT_FALSE(frameB.isNull());
    int redAtB = frameB.pixelColor(frameB.width() / 2, frameB.height() / 2).red();
    EXPECT_LT(redAtB, 55) << "deck B (blue) should dominate at crossfader position +1";

    m_pCrossfaderCO->set(0.0);
    QImage frameMid = m_pManager->grabPreviewFrame();
    ASSERT_FALSE(frameMid.isNull());
    int redAtMid = frameMid.pixelColor(frameMid.width() / 2, frameMid.height() / 2).red();
    EXPECT_LT(redAtMid, redAtA) << "even blend should be strictly between the two extremes";
    EXPECT_GT(redAtMid, redAtB) << "even blend should be strictly between the two extremes";
}

TEST_F(VideoEngineManagerTest, DeckPauseIsIndependentOfOtherDeck) {
    if (!m_pManager->isAvailable()) {
        GTEST_SKIP() << "VideoEngineManager reports GStreamer unavailable";
    }
    ASSERT_TRUE(QFileInfo::exists(deckAAnimatedFixturePath()));
    ASSERT_TRUE(QFileInfo::exists(deckBAnimatedFixturePath()));

    // Stage 4: this is the bug the whole two-pipeline rewrite exists to
    // fix -- under the old shared-pipeline design, deck B's video kept
    // visibly advancing even while genuinely paused, as long as deck A
    // (sharing the same pipeline) was playing. Mark deck A playing, leave
    // deck B NOT playing, and confirm each deck's own pipeline now
    // behaves independently of the other's play state.
    m_pDeckAPlayCO->set(1.0);
    QCoreApplication::processEvents();

    ASSERT_TRUE(m_pManager->loadVideo(QStringLiteral("[Channel1]"), deckAAnimatedFixturePath()));
    ASSERT_TRUE(m_pManager->loadVideo(QStringLiteral("[Channel2]"), deckBAnimatedFixturePath()));

    QImage firstFrame = grabFirstNonNullFrame(m_pManager.get());
    ASSERT_FALSE(firstFrame.isNull()) << "both decks should have delivered a first real frame";

    // Isolate deck B (crossfader = +1.0, deck B fully visible) and confirm
    // its own pipeline is genuinely paused: two grabs ~200ms apart must be
    // pixel-identical.
    m_pCrossfaderCO->set(1.0);
    QImage deckBFrame1 = m_pManager->grabPreviewFrame();
    ASSERT_FALSE(deckBFrame1.isNull());
    pumpEventsFor(200);
    QImage deckBFrame2 = m_pManager->grabPreviewFrame();
    ASSERT_FALSE(deckBFrame2.isNull());
    EXPECT_EQ(deckBFrame1, deckBFrame2)
            << "deck B is not playing -- its own pipeline should be genuinely paused, "
               "not still advancing because deck A is playing";

    // Isolate deck A (crossfader = -1.0, deck A fully visible) and confirm
    // it's actually playing: two grabs ~200ms apart must differ.
    m_pCrossfaderCO->set(-1.0);
    QImage deckAFrame1 = m_pManager->grabPreviewFrame();
    ASSERT_FALSE(deckAFrame1.isNull());
    pumpEventsFor(200);
    QImage deckAFrame2 = m_pManager->grabPreviewFrame();
    ASSERT_FALSE(deckAFrame2.isNull());
    EXPECT_NE(deckAFrame1, deckAFrame2)
            << "deck A is playing -- its frames should keep advancing";
}
