#include "library/videoengine/videoenginemanager.h"

#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include <QMetaObject>
#include <QThread>
#include <QTimer>
#include <QUrl>

#include <cmath>
#include <functional>

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "control/controlpushbutton.h"
#include "moc_videoenginemanager.cpp"
#include "util/logger.h"

namespace {
const mixxx::Logger kLogger("VideoEngineManager");

constexpr int kPreviewWidth = 320;
constexpr int kPreviewHeight = 240;

// gst_parse_launch's mini-language needs file paths as quoted string
// literals; escape backslashes and double quotes so a path containing
// either doesn't break parsing or allow injection into the pipeline
// description.
QString escapeForPipelineString(const QString& path) {
    QString escaped = path;
    escaped.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    escaped.replace(QLatin1Char('"'), QLatin1String("\\\""));
    return escaped;
}

// Stage 4: one independent per-deck pipeline description, ending at that
// deck's own appsink instead of a shared compositor pad -- see the class
// doc comment's Stage 4 entry for why. decodeName is only meaningful for
// the real-file case (named so VideoEngineManager can retain a handle to
// seek it independently, Stage 3j); the placeholder/camera branches have
// no decodebin at all.
//
// The bounded queue right after decodebin (Stage 3k, widened Stage 3l,
// leaky mode fixed Stage 3n) still matters here for the same reason it
// always has: without it, decodebin can buffer well ahead of what's
// actually been displayed, so a seek's target frame has to fight through
// a backlog of stale, already-decoded buffers before it's actually
// visible.
//
// format=RGB in the caps right before each deck's own appsink (previously
// applied once, after the shared compositor's own videoconvert) now has
// to be enforced per deck, since each deck's own videoconvert is the last
// format-normalizing element before ITS OWN sink -- grabPreviewFrame()'s
// pullFrameFromAppSink() reads plane 0 assuming packed RGB888 layout.
QString deckPipelineDescFor(const QString& filePath, const QString& decodeName,
        const QString& sinkName, bool useCamera) {
    if (useCamera) {
        // Stage 3y: avfvideosrc (macOS camera capture). A live source,
        // like the videotestsrc placeholder below -- none of the
        // seek/position-sync machinery built for loaded clips applies to
        // it, and it's named neither "decodeA" nor "decodeB" so this
        // deck's pDecode correctly stays null while on camera, the same
        // "no real clip loaded" null-handling every seek/loop helper
        // already has to guard against covers this case for free.
        return QStringLiteral(
                "avfvideosrc device-index=0 "
                "! videoconvert ! videoscale "
                "! video/x-raw,width=%1,height=%2,format=RGB "
                "! appsink name=%3 emit-signals=false sync=true max-buffers=1 drop=true")
                .arg(kPreviewWidth)
                .arg(kPreviewHeight)
                .arg(sinkName);
    }
    if (filePath.isEmpty()) {
        // Stage 3z-3: no `is-live=true` -- videotestsrc defaults to
        // is-live=false, which is what lets this placeholder preroll/
        // pause/step normally like the file-based branches (a live
        // source's pad has nothing "buffered" to contribute while
        // PAUSED, by GStreamer design, which used to stall the old
        // shared compositor waiting on this pad even when the other
        // branch was completely ready).
        return QStringLiteral(
                "videotestsrc pattern=black "
                "! videoconvert ! videoscale "
                "! video/x-raw,width=%1,height=%2,format=RGB "
                "! appsink name=%3 emit-signals=false sync=true max-buffers=1 drop=true")
                .arg(kPreviewWidth)
                .arg(kPreviewHeight)
                .arg(sinkName);
    }
    return QStringLiteral(
            "filesrc location=\"%1\" ! decodebin name=%2 "
            "! queue max-size-buffers=0 max-size-bytes=0 max-size-time=500000000 "
            "! videoconvert ! videoscale "
            "! video/x-raw,width=%3,height=%4,format=RGB "
            "! appsink name=%5 emit-signals=false sync=true max-buffers=1 drop=true")
            .arg(escapeForPipelineString(filePath),
                    decodeName,
                    QString::number(kPreviewWidth),
                    QString::number(kPreviewHeight),
                    sinkName);
}

// Stage 4: pulls whatever sample is currently available from one deck's
// own appsink and decodes it into a QImage -- factored out of
// grabPreviewFrame() so it can be called once per deck. Pure function, no
// class state needed.
QImage pullFrameFromAppSink(GstElement* pAppSink, int timeoutMs) {
    if (!pAppSink) {
        return QImage();
    }
    GstSample* pSample = gst_app_sink_try_pull_sample(
            GST_APP_SINK(pAppSink), static_cast<GstClockTime>(timeoutMs) * GST_MSECOND);
    if (!pSample) {
        return QImage();
    }

    QImage result;
    GstCaps* pCaps = gst_sample_get_caps(pSample);
    GstVideoInfo videoInfo;
    GstBuffer* pBuffer = gst_sample_get_buffer(pSample);
    if (pCaps && pBuffer && gst_video_info_from_caps(&videoInfo, pCaps)) {
        GstVideoFrame frame;
        if (gst_video_frame_map(&frame, &videoInfo, pBuffer, GST_MAP_READ)) {
            int width = GST_VIDEO_FRAME_WIDTH(&frame);
            int height = GST_VIDEO_FRAME_HEIGHT(&frame);
            int stride = GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 0);
            guint8* pData = static_cast<guint8*>(GST_VIDEO_FRAME_PLANE_DATA(&frame, 0));
            QImage frameImage(pData, width, height, stride, QImage::Format_RGB888);
            result = frameImage.copy();
            gst_video_frame_unmap(&frame);
        }
    }
    gst_sample_unref(pSample);
    return result;
}

// Stage 4: replaces the old GStreamer `compositor` element entirely --
// alpha-blends two already-decoded, identically-sized RGB888 frames in
// C++ using the same linear mapping applyCrossfaderAlpha() used to write
// into the compositor's pad properties (proven correct by the standalone
// Stage 3d-1 CLI): alphaA = (1-position)/2, alphaB = (1+position)/2.
// Raw scanLine() pointer arithmetic, not QImage::pixelColor()/
// setPixelColor() (which do per-pixel format conversion and are far
// slower) -- this runs on every preview poll tick.
QImage blendFrames(const QImage& frameA, const QImage& frameB, double crossfaderValue) {
    if (frameA.isNull() && frameB.isNull()) {
        return QImage();
    }
    if (frameA.isNull()) {
        return frameB;
    }
    if (frameB.isNull()) {
        return frameA;
    }
    if (frameA.size() != frameB.size()) {
        // Shouldn't happen -- both decks' pipelines fix width/height
        // identically -- but if it ever does (e.g. mid-rebuild race),
        // degrade to deck A rather than crash on mismatched scanlines.
        kLogger.warning() << "blendFrames: size mismatch" << frameA.size() << frameB.size();
        return frameA;
    }
    double alphaA = (1.0 - crossfaderValue) / 2.0;
    double alphaB = (1.0 + crossfaderValue) / 2.0;
    QImage result(frameA.size(), QImage::Format_RGB888);
    const int rowBytes = frameA.width() * 3;
    for (int y = 0; y < frameA.height(); ++y) {
        const uchar* pA = frameA.constScanLine(y);
        const uchar* pB = frameB.constScanLine(y);
        uchar* pOut = result.scanLine(y);
        for (int x = 0; x < rowBytes; ++x) {
            int v = static_cast<int>(pA[x] * alphaA + pB[x] * alphaB + 0.5);
            pOut[x] = static_cast<uchar>(qBound(0, v, 255));
        }
    }
    return result;
}

// Stage 3x (clip looping), Stage 4 (relocated): attached to each deck's
// own appsink sink pad, not the decodebin's own src pad -- the appsink's
// sink pad is static (exists from pipeline construction), while
// decodebin's src pad only appears asynchronously once it autoplugs,
// which would need its own "pad-added" dance to attach to safely.
// Dropping the EOS event here (returning GST_PAD_PROBE_DROP) means the
// appsink never sees it and never marks itself finished; the actual
// re-seek is deliberately NOT done here (this callback runs on the
// GStreamer streaming thread -- see the class doc comment's Stage 3x
// entry for why a same-thread flushing seek from inside an event probe is
// a known deadlock risk) but deferred via a queued cross-thread call back
// into VideoEngineManager.
GstPadProbeReturn loopDeckAOnEos(GstPad* /*pPad*/, GstPadProbeInfo* pInfo, gpointer pUserData) {
    if ((GST_PAD_PROBE_INFO_TYPE(pInfo) & GST_PAD_PROBE_TYPE_EVENT_BOTH) &&
            GST_EVENT_TYPE(GST_PAD_PROBE_INFO_EVENT(pInfo)) == GST_EVENT_EOS) {
        auto* pManager = static_cast<QObject*>(pUserData);
        QMetaObject::invokeMethod(pManager, "slotDeckALooped", Qt::QueuedConnection);
        return GST_PAD_PROBE_DROP;
    }
    return GST_PAD_PROBE_OK;
}

GstPadProbeReturn loopDeckBOnEos(GstPad* /*pPad*/, GstPadProbeInfo* pInfo, gpointer pUserData) {
    if ((GST_PAD_PROBE_INFO_TYPE(pInfo) & GST_PAD_PROBE_TYPE_EVENT_BOTH) &&
            GST_EVENT_TYPE(GST_PAD_PROBE_INFO_EVENT(pInfo)) == GST_EVENT_EOS) {
        auto* pManager = static_cast<QObject*>(pUserData);
        QMetaObject::invokeMethod(pManager, "slotDeckBLooped", Qt::QueuedConnection);
        return GST_PAD_PROBE_DROP;
    }
    return GST_PAD_PROBE_OK;
}

} // namespace

extern "C" {
GST_PLUGIN_STATIC_DECLARE(coreelements);
GST_PLUGIN_STATIC_DECLARE(videotestsrc);
GST_PLUGIN_STATIC_DECLARE(videoconvertscale);
GST_PLUGIN_STATIC_DECLARE(app);
GST_PLUGIN_STATIC_DECLARE(compositor);
GST_PLUGIN_STATIC_DECLARE(playback);
GST_PLUGIN_STATIC_DECLARE(typefindfunctions);
GST_PLUGIN_STATIC_DECLARE(isomp4);
GST_PLUGIN_STATIC_DECLARE(applemedia);
GST_PLUGIN_STATIC_DECLARE(videoparsersbad);
GST_PLUGIN_STATIC_DECLARE(y4m);
}

namespace mixxx {

VideoEngineManager::VideoEngineManager(QObject* parent)
        : QObject(parent),
          m_available(false),
          m_pEnabled(std::make_unique<ControlPushButton>(
                  ConfigKey("[VideoEngine]", "enabled"))) {
    // Stage 3z-10/Stage 4: one persistent worker thread per deck that all
    // of that deck's seekElementAsync()/stepOneFrameIfPaused() work is
    // queued onto (Qt::QueuedConnection targeting
    // DeckVideoContext::pSeekWorkerContext, whose thread affinity is
    // pSeekThread) -- see the header's DeckVideoContext member comment
    // for why per-deck, not shared. A plain QObject with no overridden
    // behavior is enough: QThread::run()'s default implementation already
    // runs a Qt event loop, which is all a Qt::QueuedConnection target
    // needs.
    initDeckSeekWorker(m_deckA, QStringLiteral("VideoEngineSeekThreadA"));
    initDeckSeekWorker(m_deckB, QStringLiteral("VideoEngineSeekThreadB"));

    m_pEnabled->setButtonMode(mixxx::control::ButtonMode::Toggle);
    m_pEnabled->set(0.0);
    connect(m_pEnabled.get(),
            &ControlObject::valueChanged,
            this,
            &VideoEngineManager::slotEnabledChanged);

    if (!gst_is_initialized()) {
        GError* pError = nullptr;
        if (!gst_init_check(nullptr, nullptr, &pError)) {
            kLogger.warning() << "gst_init_check failed:"
                               << (pError ? pError->message : "unknown error");
            if (pError) {
                g_error_free(pError);
            }
            return;
        }
    }
    // Plugin registration is process-global (GStreamer's registry, not
    // per-instance state), so it must only happen once even though the
    // real app only ever constructs one VideoEngineManager -- the gtest
    // suite constructs one per TEST_F in the same process, and a second
    // GST_PLUGIN_STATIC_REGISTER call for an already-registered plugin
    // hits "cannot register existing type" GLib/GStreamer criticals rather
    // than being a silent no-op.
    static bool s_pluginsRegistered = false;
    if (!s_pluginsRegistered) {
        GST_PLUGIN_STATIC_REGISTER(coreelements);
        GST_PLUGIN_STATIC_REGISTER(videotestsrc);
        GST_PLUGIN_STATIC_REGISTER(videoconvertscale);
        GST_PLUGIN_STATIC_REGISTER(app);
        GST_PLUGIN_STATIC_REGISTER(compositor);
        GST_PLUGIN_STATIC_REGISTER(playback);
        GST_PLUGIN_STATIC_REGISTER(typefindfunctions);
        GST_PLUGIN_STATIC_REGISTER(isomp4);
        GST_PLUGIN_STATIC_REGISTER(applemedia);
        GST_PLUGIN_STATIC_REGISTER(videoparsersbad);
        GST_PLUGIN_STATIC_REGISTER(y4m);
        s_pluginsRegistered = true;
    }
    m_available = true;
    kLogger.info() << "GStreamer" << gst_version_string() << "initialized";

    // [Master]/crossfader already exists by the time CoreServices
    // constructs VideoEngineManager (EngineMixer registers it first), so
    // ControlFlag::None (the default, requires the CO to already exist) is
    // correct here -- unlike m_pEnabled, which VideoEngineManager itself
    // owns and creates. Stage 4: read on demand in grabPreviewFrame(),
    // no subscription needed anymore (see the header's member comment).
    m_pCrossfader = std::make_unique<ControlProxy>(ConfigKey("[Master]", "crossfader"), this);

    initDeckControls(m_deckA, QStringLiteral("[Channel1]"));
    initDeckControls(m_deckB, QStringLiteral("[Channel2]"));

    // Stage 3g: build both pipelines now, not lazily on the first
    // loadVideo() call, so grabPreviewFrame() (and therefore the floating
    // preview panel) always has a live frame once the engine is
    // available -- a black frame from each deck's placeholder branch
    // before anything is loaded, rather than nothing at all.
    rebuildDeckPipeline(m_deckA, QStringLiteral("decodeA"), QStringLiteral("sinkA"));
    rebuildDeckPipeline(m_deckB, QStringLiteral("decodeB"), QStringLiteral("sinkB"));
}

VideoEngineManager::~VideoEngineManager() {
    teardownDeckPipeline(m_deckA);
    teardownDeckPipeline(m_deckB);
    // Stop accepting new queued work and let anything already queued run
    // to completion (deleteLater() is itself just another event posted to
    // this same queue, so it naturally runs after any seek/step tasks
    // ahead of it) before tearing down the thread itself.
    teardownDeckSeekWorker(m_deckA);
    teardownDeckSeekWorker(m_deckB);
}

void VideoEngineManager::initDeckControls(DeckVideoContext& deck, const QString& channelGroup) {
    // Stage 3v: explicitly request Qt::QueuedConnection rather than the
    // default Qt::AutoConnection. VideoEngineManager and the QML deck
    // Play button both live on the GUI thread, so a write originating
    // from a QML button click (unlike one from CueControl's own code,
    // which runs on the audio/engine thread and is therefore genuinely
    // cross-thread/queued) resolves AutoConnection to a *direct*,
    // same-thread connection between ControlDoublePrivate and
    // ControlProxy. A real, confirmed bug was observed where a QML
    // button's play-CO write was never seen by this class's subscription
    // at all -- forcing Qt::QueuedConnection removes same-thread direct
    // delivery as a variable entirely, converging on the one delivery
    // path already proven reliable. Same reasoning applies to the
    // position CO below (a waveform/position click is the same kind of
    // same-thread QML-originated write).
    deck.pPlayControl = std::make_unique<ControlProxy>(ConfigKey(channelGroup, "play"), this);
    deck.pPlayControl->connectValueChanged(
            this,
            [this, &deck](double value) { handlePlayChanged(deck, value); },
            Qt::QueuedConnection);
    deck.playing = deck.pPlayControl->toBool();

    // Stage 3j: "[Channel]/playposition" is normalized 0.0..1.0 within the
    // loaded track (EngineBuffer). Stage 3r/3t: also reacts while paused
    // (debounced scrub-seek) and, separately, to a large single-tick jump
    // while playing (a click/hotcue fired mid-playback) -- see
    // handlePositionChanged().
    deck.pPositionControl = std::make_unique<ControlProxy>(
            ConfigKey(channelGroup, "playposition"), this);
    deck.pPositionControl->connectValueChanged(
            this,
            [this, &deck](double value) { handlePositionChanged(deck, value); },
            Qt::QueuedConnection);
    deck.lastAudioPosition = deck.pPositionControl->get();

    deck.pScrubDebounceTimer = std::make_unique<QTimer>(this);
    deck.pScrubDebounceTimer->setSingleShot(true);
    connect(deck.pScrubDebounceTimer.get(), &QTimer::timeout, this, [this, &deck]() {
        commitScrubSeek(deck);
    });
    deck.scrubTicksSinceLastCommit = 0;
}

void VideoEngineManager::initDeckSeekWorker(DeckVideoContext& deck, const QString& threadName) {
    deck.pSeekThread = new QThread();
    deck.pSeekThread->setObjectName(threadName);
    deck.pSeekThread->start();
    deck.pSeekWorkerContext = new QObject();
    deck.pSeekWorkerContext->moveToThread(deck.pSeekThread);
}

void VideoEngineManager::teardownDeckSeekWorker(DeckVideoContext& deck) {
    deck.pSeekWorkerContext->deleteLater();
    deck.pSeekThread->quit();
    deck.pSeekThread->wait();
    delete deck.pSeekThread;
    deck.pSeekThread = nullptr;
    deck.pSeekWorkerContext = nullptr;
}

void VideoEngineManager::slotEnabledChanged(double value) {
    if (!m_available) {
        if (value > 0) {
            kLogger.warning() << "video engine enable requested but GStreamer is not available";
            m_pEnabled->set(0.0);
        }
        return;
    }
    kLogger.info() << "video engine" << (value > 0 ? "enabled" : "disabled");
}

void VideoEngineManager::slotDeckALooped() {
    handleDeckLooped(m_deckA);
}

void VideoEngineManager::slotDeckBLooped() {
    handleDeckLooped(m_deckB);
}

void VideoEngineManager::handleDeckLooped(DeckVideoContext& deck) {
    // Stage 3z-9: async, not blocking -- a short clip can reach EOS and
    // loop far more often than the "rare, discrete event" this path was
    // originally written for, so it gets the same non-blocking treatment
    // as every other seek in this class now.
    kLogger.info() << "[diag] deck video looped, seeking back to 0";
    seekElementAsync(deck, 0);
}

void VideoEngineManager::handlePlayChanged(DeckVideoContext& deck, double value) {
    bool wasPlaying = deck.playing;
    deck.playing = value > 0;
    kLogger.info() << "[diag] handlePlayChanged value=" << value << "wasPlaying=" << wasPlaying
                    << "playing=" << deck.playing;
    // Stage 3j: re-anchor this deck's video to the audio's current
    // position every time it (re)starts playing -- covers resume-after-
    // pause and resume-after-cue, the two cases the drift was actually
    // reported in. Stage 3z-9: applyPlayState() moves into the seek's
    // onLanded callback for this case -- otherwise, now that the seek is
    // async, PLAYING would be requested immediately, before the seek has
    // actually landed, reintroducing the stale-frame catch-up flash this
    // ordering was originally written to prevent (Stage 3s).
    if (deck.playing && !wasPlaying) {
        seekDeckToCurrentPosition(deck, [this, &deck]() { applyPlayState(deck); });
    } else {
        applyPlayState(deck);
    }
}

void VideoEngineManager::handlePositionChanged(DeckVideoContext& deck, double newPosition) {
    if (!deck.pDecode) {
        deck.lastAudioPosition = newPosition; // no real clip loaded on this deck
        return;
    }
    double signedDelta = newPosition - deck.lastAudioPosition;
    double delta = std::abs(signedDelta);
    kLogger.info() << "[diag] handlePositionChanged" << GST_OBJECT_NAME(deck.pDecode)
                    << "newPosition=" << newPosition
                    << "lastPosition=" << deck.lastAudioPosition << "delta=" << delta
                    << "isPlaying=" << deck.playing;
    deck.lastAudioPosition = newPosition;

    if (deck.playing) {
        // A genuine forward seek/hotcue/click fired while the deck is
        // already playing moves position by a large, unambiguous jump in
        // a single tick; ordinary per-tick playback advancement is a tiny
        // fraction of a percent of track length. Comparing only against
        // this deck's own previous tick (never against GStreamer's
        // queried position -- that comparison was Stage 3o's mistake)
        // means this can't misfire from an audio/video duration mismatch
        // the way Stage 3o's continuous correction did.
        constexpr double kBigJumpThreshold = 0.01; // ~1% of track length
        // Stage 3z: a beatloop's loop-out -> loop-in reset moves position
        // BACKWARD while playing -- often by well under 1% of the full
        // track, so ordinary loop sizes routinely fell under the
        // forward-jump threshold above and were silently treated as
        // normal playback, never re-seeking the video branch at all. A
        // separate, much smaller threshold for the backward direction
        // still matters here: scratching also moves position backward,
        // frequently, in small increments, and an accurate seek is
        // deliberately expensive (meant for rare, discrete events);
        // triggering it on every tiny scratch-jitter tick would stutter
        // the whole app during scratching.
        constexpr double kBackwardJumpThreshold = 0.001; // ~0.1% of track length
        bool isBackwardJump = signedDelta <= -kBackwardJumpThreshold;
        bool isBigForwardJump = signedDelta >= kBigJumpThreshold;
        if (isBackwardJump) {
            // Stage 3z-9: a beatloop reset can repeat every few seconds or
            // faster, so this gets a much shorter waitNs than the
            // rare-event path below (150ms vs 1s) and skips
            // stepOneFrameIfPaused() (stepFrameAfter=false): this deck is
            // playing throughout a loop reset, never touches
            // applyPlayState(), so there's nothing here that depends on
            // the seek having landed by any particular time.
            constexpr qint64 kLoopResetSeekWaitNs = 150000000; // 150ms
            seekDeckToCurrentPosition(
                    deck, nullptr, kLoopResetSeekWaitNs, /*stepFrameAfter=*/false);
        } else if (isBigForwardJump) {
            // Genuinely rare, discrete event (a forward click/hotcue) --
            // the accurate seek (with its default 1s wait) is appropriate
            // here, same reasoning as Stage 3p's restoration.
            seekDeckToCurrentPosition(deck);
        }
        return;
    }

    // Paused/scrubbing: debounce (trailing-edge). Restarting this
    // single-shot timer on every tick means a lone tick (a click, a cue
    // jump) still fires ~100ms later, while a rapid drag just keeps
    // restarting it and only the final quiet moment commits a seek.
    // Stage 3w: also count ticks since the last commit, so commitScrubSeek()
    // can tell a lone tick (click/cue jump) apart from an active drag.
    deck.scrubTicksSinceLastCommit++;
    deck.pScrubDebounceTimer->start(100);
}

void VideoEngineManager::commitScrubSeek(DeckVideoContext& deck) {
    int ticks = deck.scrubTicksSinceLastCommit;
    deck.scrubTicksSinceLastCommit = 0;
    if (!deck.pDecode || !deck.pPositionControl) {
        return;
    }
    gint64 durationNs = 0;
    if (!gst_element_query_duration(deck.pDecode, GST_FORMAT_TIME, &durationNs) ||
            durationNs <= 0) {
        return;
    }
    // Re-read the *current* position now, not whatever value was passed
    // to handlePositionChanged() when this debounce timer was (re)started
    // -- correct debounce behavior commits to wherever things ended up by
    // the time the quiet period elapsed, not a stale intermediate value.
    double position = qBound(0.0, deck.pPositionControl->get(), 1.0);
    gint64 targetNs = static_cast<gint64>(position * static_cast<double>(durationNs));
    kLogger.info() << "[diag] commitScrubSeek" << GST_OBJECT_NAME(deck.pDecode)
                    << "position=" << position << "targetNs=" << targetNs << "ticks=" << ticks;
    if (ticks <= 1) {
        // Stage 3w: exactly one tick since the last commit means this was
        // a lone, discrete jump -- a click or a cue press -- not an
        // active drag. seekElementAsync() (accurate, and never blocking
        // the calling thread either way) handles this uniformly with
        // play/pause/cue transitions.
        seekElementAsync(deck, targetNs);
        return;
    }
    // Fire-and-forget with KEY_UNIT, not seekElementAsync()'s ACCURATE
    // seek: approximate, keyframe-snapped preview while scrubbing is the
    // standard tradeoff most scrub-preview UIs make; once the deck
    // actually starts playing, seekDeckToCurrentPosition()'s accurate seek
    // takes over.
    gst_element_seek_simple(deck.pDecode,
            GST_FORMAT_TIME,
            static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
            targetNs);
    // Stage 3s: seeking the branch alone doesn't make it recomposite/
    // re-render while genuinely paused -- see stepOneFrameIfPaused().
    // Stage 3z-9/Stage 4: off the calling (GUI) thread and onto this
    // deck's own serialized worker queue -- a real drag can commit a tick
    // every ~100ms, and stepOneFrameIfPaused() alone can block for up to
    // ~250ms when the pipeline is genuinely paused, which is exactly the
    // drag case this branch handles. The pipeline is ref'd here, before
    // enqueueing, and unref'd once the queued task is done with it --
    // stepOneFrameIfPaused() itself assumes its caller guarantees the
    // pointer stays valid for the call's duration.
    GstElement* pPipeline = deck.pPipeline;
    if (pPipeline) {
        gst_object_ref(pPipeline);
        QMetaObject::invokeMethod(
                deck.pSeekWorkerContext,
                [this, pPipeline]() {
                    stepOneFrameIfPaused(pPipeline);
                    gst_object_unref(pPipeline);
                },
                Qt::QueuedConnection);
    }
}

void VideoEngineManager::stepOneFrameIfPaused(GstElement* pPipeline) {
    if (!pPipeline) {
        return;
    }
    // Stage 3z-1: GST_STATE() reads the element's current_state field
    // directly, with no regard for a still-in-flight async transition --
    // applyPlayState() requests PAUSED via a non-blocking
    // gst_element_set_state() call (deliberately, since it's invoked from
    // signal handlers that must not stall) and never waits for that
    // transition to actually land. If this method's caller (a seek right
    // after a cue/pause) runs before that PLAYING->PAUSED transition has
    // fully settled, GST_STATE() can still read the stale prior state,
    // this check silently (and wrongly) bails out, and nothing gets
    // nudged to render a fresh frame. A short bounded wait resolves any
    // pending transition first, so the check below reflects this
    // pipeline's true, settled state rather than a possibly-stale
    // snapshot. GST_SECOND/10 (100ms), not the 1-second bound used
    // elsewhere for a fresh PLAYING->PAUSED transition -- that's normally
    // near-instant (no decode/network wait involved), and this runs on a
    // background worker thread, but keeping the bound tight still limits
    // how long a genuinely stuck pipeline could occupy that worker.
    gst_element_get_state(pPipeline, nullptr, nullptr, GST_SECOND / 10);
    // Stage 4: this deck's own pipeline's state depends purely on this
    // deck's own `playing` flag now (no more shared-pipeline compromise
    // where the *other* deck being active could keep this one PLAYING) --
    // so the only reason this pipeline would be anything other than
    // PAUSED here is a transition genuinely still in flight, already
    // accounted for by the wait above.
    if (GST_STATE(pPipeline) != GST_STATE_PAUSED) {
        return;
    }
    // Stage 3z-5: a brief PLAYING pulse, not GStreamer's documented
    // gst_event_new_step() technique -- STEP was confirmed, via repeated
    // direct testing, to never produce a new frame in this pipeline
    // shape. Real PLAYING mode is independently proven reliable (it's how
    // the whole crossfade feature was validated originally), so this
    // sidesteps STEP entirely: nudge PLAYING just long enough for at
    // least one real frame to flow through and land in the appsink's
    // single-buffer queue, then return to PAUSED. The frame is left there
    // for the real preview's own independent poll
    // (QmlVideoPreviewItem::poll(), grabPreviewFrame(0)) to pick up
    // naturally on its next tick -- this method deliberately does NOT
    // pull the frame itself, which would just steal it from that poller.
    gst_element_set_state(pPipeline, GST_STATE_PLAYING);
    gst_element_get_state(pPipeline, nullptr, nullptr, GST_SECOND / 10);
    // 50ms: comfortably longer than one frame period at any realistic
    // frame rate (even 24fps is ~42ms/frame), short enough that a brief
    // pulse right after a cue/pause press isn't itself perceptible as
    // unwanted playback.
    QThread::msleep(50);
    gst_element_set_state(pPipeline, GST_STATE_PAUSED);
    gst_element_get_state(pPipeline, nullptr, nullptr, GST_SECOND / 10);
}

void VideoEngineManager::applyPlayState(DeckVideoContext& deck) {
    if (!deck.pPipeline) {
        return;
    }
    // Stage 4: this deck's own pipeline's PLAYING/PAUSED state now
    // follows purely this deck's own `playing` flag -- no more Stage 3i
    // shared-pipeline compromise ("PLAYING while either deck wants to
    // play"). This is what makes independent per-deck pause actually
    // independent: pausing this deck can no longer be overridden by the
    // other deck's own state.
    GstState targetState = deck.playing ? GST_STATE_PLAYING : GST_STATE_PAUSED;
    kLogger.info() << "[diag] applyPlayState targetState="
                    << gst_element_state_get_name(targetState) << "playing=" << deck.playing;
    gst_element_set_state(deck.pPipeline, targetState);
}

void VideoEngineManager::seekDeckToCurrentPosition(DeckVideoContext& deck,
        std::function<void()> onLanded,
        qint64 waitNs,
        bool stepFrameAfter) {
    if (!deck.pDecode || !deck.pPositionControl) {
        kLogger.info() << "[diag] seekDeckToCurrentPosition: null element or control, skipping";
        return; // no real clip loaded on this deck (placeholder/camera branch)
    }
    gint64 durationNs = 0;
    if (!gst_element_query_duration(deck.pDecode, GST_FORMAT_TIME, &durationNs) ||
            durationNs <= 0) {
        kLogger.info() << "[diag] seekDeckToCurrentPosition" << GST_OBJECT_NAME(deck.pDecode)
                        << "duration query failed, skipping";
        return; // duration not known yet -- leave position as-is
    }
    double position = qBound(0.0, deck.pPositionControl->get(), 1.0);
    gint64 targetNs = static_cast<gint64>(position * static_cast<double>(durationNs));
    kLogger.info() << "[diag] seekDeckToCurrentPosition" << GST_OBJECT_NAME(deck.pDecode)
                    << "position=" << position << "durationNs=" << durationNs
                    << "targetNs=" << targetNs;
    seekElementAsync(deck, targetNs, waitNs, stepFrameAfter, std::move(onLanded));
}

void VideoEngineManager::seekElementAsync(DeckVideoContext& deck,
        qint64 targetNs,
        qint64 waitNs,
        bool stepFrameAfter,
        std::function<void()> onLanded) {
    // History: this used to be seekElementBlocking(), doing the seek +
    // wait synchronously, right here, on whatever thread called it --
    // every caller in this class runs on Qt's main/GUI thread. Stages 3k
    // through 3z-8 tuned *how long* that block lasted and *which seek
    // flag* was used, chasing a "fast-forward catch-up" artifact each
    // time.
    //
    // Stage 3z-9: real testing showed tuning the wait was never going to
    // be enough, because blocking the GUI thread AT ALL -- even briefly --
    // has a much bigger blast radius than "this one seek looks slow": it
    // also blocks the QML preview's own poll timer and delays the *next*
    // audio-engine position-change delivery, since both arrive on the GUI
    // thread too. The fix is architectural: never block the calling
    // thread here at all. gst_element_seek_simple(), gst_element_get_state(),
    // and stepOneFrameIfPaused() are all pure GstElement/GLib calls --
    // none of them touch a Qt object, so none of them care which thread
    // they run on.
    //
    // Stage 3z-10/Stage 4: queue the work onto this deck's own
    // pSeekWorkerContext (Qt::QueuedConnection) rather than spawning an
    // independent detached thread per call -- a fresh thread per call
    // fixed the GUI-thread block but let multiple such threads race
    // against each other with no ordering, which could leave a deck's
    // pipeline in the wrong state. Each deck's own worker's event loop is
    // a strict FIFO queue, so this call and any other still pending
    // against the same deck's worker run one at a time, in the order
    // they were issued. Only hop back to this object's own (main) thread,
    // once landed, to invoke onLanded, since that callback (e.g.
    // applyPlayState(), which reads and writes this class's own member
    // state) does need to run there.
    //
    // deck.pDecode/deck.pPipeline are raw pointers into pipelines this
    // class owns; rebuildDeckPipeline() can tear one down and replace it
    // with a fresh element at any time, including while this queued task
    // is still pending or running. gst_object_ref() keeps both alive for
    // as long as the task needs them, independent of what deck.pDecode/
    // deck.pPipeline point to by the time it runs.
    GstElement* pDecodeElement = deck.pDecode;
    if (!pDecodeElement) {
        return;
    }
    GstElement* pPipeline = deck.pPipeline;
    gst_object_ref(pDecodeElement);
    if (pPipeline) {
        gst_object_ref(pPipeline);
    }
    QMetaObject::invokeMethod(
            deck.pSeekWorkerContext,
            [this, pDecodeElement, pPipeline, targetNs, waitNs, stepFrameAfter, onLanded]() {
                kLogger.info() << "[diag] seekElementAsync" << GST_OBJECT_NAME(pDecodeElement)
                                << "targetNs=" << targetNs << "waitNs=" << waitNs;
                bool seekOk = gst_element_seek_simple(pDecodeElement,
                        GST_FORMAT_TIME,
                        static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
                        targetNs);
                if (!seekOk) {
                    kLogger.warning() << "seek failed for" << GST_OBJECT_NAME(pDecodeElement);
                } else {
                    GstStateChangeReturn changeResult =
                            gst_element_get_state(pDecodeElement, nullptr, nullptr, waitNs);
                    gint64 landedPositionNs = -1;
                    gst_element_query_position(pDecodeElement, GST_FORMAT_TIME, &landedPositionNs);
                    kLogger.info() << "[diag] seekElementAsync" << GST_OBJECT_NAME(pDecodeElement)
                                    << "changeResult=" << static_cast<int>(changeResult)
                                    << "landedPositionNs=" << landedPositionNs;
                    // Stage 3s: force a fresh render at this new position
                    // before onLanded (e.g. applyPlayState()) flips the
                    // pipeline to PLAYING -- otherwise a stale
                    // last-rendered frame is what real playback starts
                    // from, visible as a catch-up burst.
                    if (stepFrameAfter) {
                        stepOneFrameIfPaused(pPipeline);
                    }
                }
                gst_object_unref(pDecodeElement);
                if (pPipeline) {
                    gst_object_unref(pPipeline);
                }
                QMetaObject::invokeMethod(
                        this,
                        [onLanded]() {
                            if (onLanded) {
                                onLanded();
                            }
                        },
                        Qt::QueuedConnection);
            },
            Qt::QueuedConnection);
}

bool VideoEngineManager::loadVideo(const QString& deckGroup, const QString& filePath) {
    if (!m_available) {
        kLogger.warning() << "loadVideo requested but GStreamer is not available";
        return false;
    }
    // The deck drop target (PlayerDropArea.qml) hands over drag-and-drop
    // URLs as-is (matching how it already calls
    // player.loadTrackFromLocationUrl(url) for ordinary audio drops) --
    // accept both a file:// URL and a plain filesystem path here rather
    // than pushing that distinction onto every caller.
    const QUrl url(filePath);
    const QString localPath = url.isLocalFile() ? url.toLocalFile() : filePath;
    DeckVideoContext* pDeck = nullptr;
    QString decodeName;
    QString sinkName;
    if (deckGroup == QLatin1String("[Channel1]")) {
        pDeck = &m_deckA;
        decodeName = QStringLiteral("decodeA");
        sinkName = QStringLiteral("sinkA");
    } else if (deckGroup == QLatin1String("[Channel2]")) {
        pDeck = &m_deckB;
        decodeName = QStringLiteral("decodeB");
        sinkName = QStringLiteral("sinkB");
    } else {
        kLogger.warning() << "loadVideo: unrecognized deck group" << deckGroup;
        return false;
    }
    pDeck->videoPath = localPath;
    // Stage 4: only this deck's own pipeline is rebuilt -- previously,
    // loading a clip on one deck also tore down and rebuilt the other
    // deck's video (both shared one pipeline), causing a visible blip on
    // the untouched deck.
    rebuildDeckPipeline(*pDeck, decodeName, sinkName);
    return pDeck->pPipeline != nullptr;
}

bool VideoEngineManager::setCameraSource(const QString& deckGroup, bool enabled) {
    if (!m_available) {
        kLogger.warning() << "setCameraSource requested but GStreamer is not available";
        return false;
    }
    DeckVideoContext* pDeck = nullptr;
    QString decodeName;
    QString sinkName;
    if (deckGroup == QLatin1String("[Channel1]")) {
        pDeck = &m_deckA;
        decodeName = QStringLiteral("decodeA");
        sinkName = QStringLiteral("sinkA");
    } else if (deckGroup == QLatin1String("[Channel2]")) {
        pDeck = &m_deckB;
        decodeName = QStringLiteral("decodeB");
        sinkName = QStringLiteral("sinkB");
    } else {
        kLogger.warning() << "setCameraSource: unrecognized deck group" << deckGroup;
        return false;
    }
    pDeck->useCamera = enabled;
    rebuildDeckPipeline(*pDeck, decodeName, sinkName);
    return pDeck->pPipeline != nullptr;
}

void VideoEngineManager::teardownDeckPipeline(DeckVideoContext& deck) {
    if (deck.pPipeline) {
        gst_element_set_state(deck.pPipeline, GST_STATE_NULL);
        gst_object_unref(deck.pPipeline);
        deck.pPipeline = nullptr;
    }
    if (deck.pAppSinkPad) {
        gst_object_unref(deck.pAppSinkPad);
        deck.pAppSinkPad = nullptr;
    }
    deck.pAppSink = nullptr; // owned by deck.pPipeline, already released above
    if (deck.pDecode) {
        gst_object_unref(deck.pDecode);
        deck.pDecode = nullptr;
    }
}

void VideoEngineManager::rebuildDeckPipeline(
        DeckVideoContext& deck, const QString& decodeName, const QString& sinkName) {
    teardownDeckPipeline(deck);

    const QString pipelineDesc =
            deckPipelineDescFor(deck.videoPath, decodeName, sinkName, deck.useCamera);

    GError* pError = nullptr;
    GstElement* pPipeline = gst_parse_launch(pipelineDesc.toUtf8().constData(), &pError);
    if (pError) {
        kLogger.warning() << "failed to build" << sinkName << "pipeline:" << pError->message;
        g_clear_error(&pError);
        return;
    }

    GstElement* pAppSink =
            gst_bin_get_by_name(GST_BIN(pPipeline), sinkName.toUtf8().constData());
    if (!pAppSink) {
        kLogger.warning() << sinkName << "pipeline missing expected appsink element";
        gst_object_unref(pPipeline);
        return;
    }

    // gst_element_set_state's return value alone isn't enough here: a
    // pipeline with a decodebin branch typically returns ASYNC (still
    // negotiating caps as decodebin autoplugs pads on a background
    // thread), not SUCCESS, at this call site -- only
    // gst_element_get_state's blocking wait resolves ASYNC to its real
    // final outcome. Without this, rebuildDeckPipeline() could report
    // success and hand back a pipeline that a subsequent
    // grabPreviewFrame() call finds not actually ready yet.
    GstStateChangeReturn changeResult = gst_element_set_state(pPipeline, GST_STATE_PLAYING);
    if (changeResult != GST_STATE_CHANGE_FAILURE) {
        changeResult = gst_element_get_state(pPipeline, nullptr, nullptr, 5 * GST_SECOND);
    }
    if (changeResult == GST_STATE_CHANGE_FAILURE || changeResult == GST_STATE_CHANGE_ASYNC) {
        kLogger.warning() << sinkName << "pipeline failed to reach PLAYING (result"
                           << static_cast<int>(changeResult) << ")";
        gst_object_unref(pAppSink);
        gst_element_set_state(pPipeline, GST_STATE_NULL);
        gst_object_unref(pPipeline);
        return;
    }

    deck.pPipeline = pPipeline;
    deck.pAppSink = pAppSink; // reference kept via deck.pPipeline's ownership
    gst_object_unref(pAppSink);
    deck.pAppSinkPad = gst_element_get_static_pad(pAppSink, "sink");
    // Null when this deck has no real file loaded (get_by_name correctly
    // finds nothing -- the placeholder/camera branches have no decodebin
    // at all).
    deck.pDecode = gst_bin_get_by_name(GST_BIN(pPipeline), decodeName.toUtf8().constData());

    // Stage 3x/Stage 4: clip looping -- only attach the EOS-catching
    // probe for a deck that actually has a real clip loaded (deck.pDecode
    // non-null); the placeholder/camera branches never reach EOS on their
    // own, so there's nothing to loop for a deck without a clip. Probes
    // are pad-scoped, so a freshly rebuilt pipeline always gets a fresh
    // pad with no stale/duplicate probes to worry about.
    if (deck.pDecode) {
        gst_pad_add_probe(deck.pAppSinkPad,
                GST_PAD_PROBE_TYPE_EVENT_BOTH,
                &deck == &m_deckA ? loopDeckAOnEos : loopDeckBOnEos,
                this,
                nullptr);
    }

    // Stage 3j: a clip loaded mid-track should start synced to wherever
    // the audio deck already is, not from position 0.
    seekDeckToCurrentPosition(deck);

    // Stage 3i (Stage 4: now scoped to just this deck): rebuildDeckPipeline()
    // above just set the fresh pipeline to PLAYING unconditionally --
    // correct it down to PAUSED immediately if this deck doesn't actually
    // want to play, so a clip loaded onto a paused deck doesn't auto-play
    // just because it was loaded.
    applyPlayState(deck);
}

QImage VideoEngineManager::grabPreviewFrame(int timeoutMs) {
    // Stage 4: pull from each deck's own appsink independently and blend
    // in C++ (blendFrames()) instead of pulling one already-composited
    // frame from a shared GStreamer compositor's appsink. Budget split so
    // total call latency stays bounded to timeoutMs regardless of how
    // many decks are currently stalled.
    int perDeckTimeoutMs = timeoutMs > 0 ? qMax(1, timeoutMs / 2) : 0;
    if (m_deckA.pAppSink) {
        QImage fresh = pullFrameFromAppSink(m_deckA.pAppSink, perDeckTimeoutMs);
        if (!fresh.isNull()) {
            m_deckA.lastFrame = std::move(fresh);
        }
    }
    if (m_deckB.pAppSink) {
        QImage fresh = pullFrameFromAppSink(m_deckB.pAppSink, perDeckTimeoutMs);
        if (!fresh.isNull()) {
            m_deckB.lastFrame = std::move(fresh);
        }
    }
    double crossfaderValue = m_pCrossfader ? m_pCrossfader->get() : 0.0;
    return blendFrames(m_deckA.lastFrame, m_deckB.lastFrame, crossfaderValue);
}

} // namespace mixxx
