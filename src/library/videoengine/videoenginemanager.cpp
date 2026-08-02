#include "library/videoengine/videoenginemanager.h"

#include <gst/app/gstappsink.h>
#include <gst/gl/gl.h>
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
#ifdef __VIDEO_ENGINE_NDI_OUTPUT__
#include "library/videoengine/ndioutputsender.h"
#endif
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
// Stage 7: the tail every branch below feeds into -- kept as two entirely
// separate strings (not a shared helper with a conditional inside it) so
// the existing, proven CPU tail is never at risk of being subtly changed
// by a GL-path edit. The CPU tail is completely untouched from before
// Stage 7. The GL tail uploads to a GL texture (`glupload`) right after
// the exact same CPU-side format/size normalization the CPU tail already
// does -- deliberately NOT trying to move the resize itself onto the GPU
// too (GStreamer's GL plugin set has no direct equivalent used elsewhere
// in this codebase, and a CPU resize of an already-small preview frame is
// cheap relative to the per-pixel blend and re-upload work this whole
// stage exists to eliminate) -- ending in `memory:GLMemory` caps means the
// frame lands in a GPU texture with no further CPU touch from here on.
QString cpuTailFor(const QString& sinkName) {
    return QStringLiteral(
            "! videoconvert ! videoscale "
            "! video/x-raw,width=%1,height=%2,format=RGB "
            "! appsink name=%3 emit-signals=false sync=true max-buffers=1 drop=true")
            .arg(kPreviewWidth)
            .arg(kPreviewHeight)
            .arg(sinkName);
}

QString glTailFor(const QString& sinkName) {
    return QStringLiteral(
            "! videoconvert ! videoscale "
            "! video/x-raw,width=%1,height=%2,format=RGBA "
            "! glupload "
            "! video/x-raw(memory:GLMemory),format=RGBA "
            "! appsink name=%3 emit-signals=false sync=true max-buffers=1 drop=true")
            .arg(kPreviewWidth)
            .arg(kPreviewHeight)
            .arg(sinkName);
}

QString deckPipelineDescFor(const QString& filePath, const QString& decodeName,
        const QString& sinkName, bool useCamera, bool useGl) {
    const QString tail = useGl ? glTailFor(sinkName) : cpuTailFor(sinkName);
    if (useCamera) {
        // Stage 3y: avfvideosrc (macOS camera capture). A live source,
        // like the videotestsrc placeholder below -- none of the
        // seek/position-sync machinery built for loaded clips applies to
        // it, and it's named neither "decodeA" nor "decodeB" so this
        // deck's pDecode correctly stays null while on camera, the same
        // "no real clip loaded" null-handling every seek/loop helper
        // already has to guard against covers this case for free.
        return QStringLiteral("avfvideosrc device-index=0 %1").arg(tail);
    }
    if (filePath.isEmpty()) {
        // Stage 3z-3: no `is-live=true` -- videotestsrc defaults to
        // is-live=false, which is what lets this placeholder preroll/
        // pause/step normally like the file-based branches (a live
        // source's pad has nothing "buffered" to contribute while
        // PAUSED, by GStreamer design, which used to stall the old
        // shared compositor waiting on this pad even when the other
        // branch was completely ready).
        return QStringLiteral("videotestsrc pattern=black %1").arg(tail);
    }
    return QStringLiteral(
            "filesrc location=\"%1\" ! decodebin name=%2 "
            "! queue max-size-buffers=0 max-size-bytes=0 max-size-time=500000000 "
            "%3")
            .arg(escapeForPipelineString(filePath), decodeName, tail);
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

// Stage 7: GL-memory counterpart of pullFrameFromAppSink() -- instead of
// mapping the buffer with GST_MAP_READ (which would silently download the
// GL texture to system memory, defeating the entire point of this stage),
// pulls the GstGLMemory straight off the buffer and hands back its raw
// texture id. Non-blocking (0 timeout): this is called once per Qt Quick
// render tick (potentially far more often than the CPU path's ~33ms poll),
// so it must never stall the render thread waiting on GStreamer -- the
// appsink's max-buffers=1 already guarantees a buffer is sitting there
// ready the instant one exists, same as the CPU path just without the
// wait. The returned GstSample (via GlFrame::pOwningSample) is not
// unreffed here -- it owns the texture's actual memory, so the caller
// keeps it alive until the GPU read is known to be done (see the class
// doc comment's Stage 7 entry on the 2-frame ring) and releases it via
// releaseGlFrameSample().
mixxx::VideoEngineManager::GlFrame pullGlFrameFromAppSink(GstElement* pAppSink) {
    mixxx::VideoEngineManager::GlFrame result;
    if (!pAppSink) {
        return result;
    }
    GstSample* pSample = gst_app_sink_try_pull_sample(GST_APP_SINK(pAppSink), 0);
    if (!pSample) {
        return result;
    }

    GstBuffer* pBuffer = gst_sample_get_buffer(pSample);
    GstCaps* pCaps = gst_sample_get_caps(pSample);
    GstVideoInfo videoInfo;
    GstMemory* pMemory = pBuffer ? gst_buffer_peek_memory(pBuffer, 0) : nullptr;
    if (!pBuffer || !pCaps || !gst_video_info_from_caps(&videoInfo, pCaps) || !pMemory ||
            !gst_is_gl_memory(pMemory)) {
        gst_sample_unref(pSample);
        return result;
    }

    GstGLMemory* pGlMemory = GST_GL_MEMORY_CAST(pMemory);
    result.textureId = pGlMemory->tex_id;
    result.width = GST_VIDEO_INFO_WIDTH(&videoInfo);
    result.height = GST_VIDEO_INFO_HEIGHT(&videoInfo);
    result.pOwningSample = pSample; // ownership transferred to the caller
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

// Stage 7: plain-data pair handed to glContextBusSyncHandler() as its
// gpointer user_data -- deliberately NOT VideoEngineManager* itself, so
// this free function never needs private access to the class; it only
// ever reads these two already-established pointers, never mutates
// VideoEngineManager's own state.
struct GlContextBusUserData {
    GstGLDisplay* pGlDisplay;
    GstGLContext* pGlContext;
};

// GStreamer's own documented app-integration pattern for a GL element
// (glupload here) to find an app-provided, already-shared GL context
// instead of creating its own unshared one: intercept
// GST_MESSAGE_NEED_CONTEXT on the bus and answer it with a GstContext
// wrapping the display/context this class already set up in
// setSharedGlContext(). Registered via gst_bus_set_sync_handler(), not the
// normal async bus-watch dispatch loop -- the requesting element blocks on
// this query as part of its own state change, so the answer has to be
// synchronous, not queued for later processing on the main loop.
GstBusSyncReply glContextBusSyncHandler(
        GstBus* /*pBus*/, GstMessage* pMessage, gpointer pUserData) {
    if (GST_MESSAGE_TYPE(pMessage) != GST_MESSAGE_NEED_CONTEXT) {
        return GST_BUS_PASS;
    }
    auto* pData = static_cast<GlContextBusUserData*>(pUserData);
    const gchar* pContextType = nullptr;
    gst_message_parse_context_type(pMessage, &pContextType);
    GstElement* pRequestingElement = GST_ELEMENT(GST_MESSAGE_SRC(pMessage));
    if (g_strcmp0(pContextType, GST_GL_DISPLAY_CONTEXT_TYPE) == 0) {
        GstContext* pDisplayContext = gst_context_new(GST_GL_DISPLAY_CONTEXT_TYPE, TRUE);
        gst_context_set_gl_display(pDisplayContext, pData->pGlDisplay);
        gst_element_set_context(pRequestingElement, pDisplayContext);
        gst_context_unref(pDisplayContext);
    } else if (g_strcmp0(pContextType, "gst.gl.app_context") == 0) {
        GstContext* pAppContext = gst_context_new("gst.gl.app_context", TRUE);
        GstStructure* pStructure = gst_context_writable_structure(pAppContext);
        gst_structure_set(pStructure, "context", GST_TYPE_GL_CONTEXT, pData->pGlContext, nullptr);
        gst_element_set_context(pRequestingElement, pAppContext);
        gst_context_unref(pAppContext);
    }
    return GST_BUS_PASS;
}

} // namespace

extern "C" {
GST_PLUGIN_STATIC_DECLARE(coreelements);
GST_PLUGIN_STATIC_DECLARE(videotestsrc);
GST_PLUGIN_STATIC_DECLARE(videoconvertscale);
GST_PLUGIN_STATIC_DECLARE(app);
// Stage 7: opengl replaces compositor here -- compositor was only ever
// needed by the pre-Stage-4 single-shared-pipeline design (one GStreamer
// `compositor` element blending both decks); Stage 4 moved that blend
// entirely into C++ (blendFrames()) and no pipeline description has
// referenced `compositor` since, so it was dead weight. opengl provides
// glupload/glcolorconvert, needed to get each deck's decoded frame onto a
// GPU texture with no CPU copy (see deckPipelineDescFor()).
GST_PLUGIN_STATIC_DECLARE(opengl);
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
        GST_PLUGIN_STATIC_REGISTER(opengl);
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

    // Stage 7: only ever non-null if setSharedGlContext() succeeded.
    if (m_pGlContext) {
        gst_object_unref(m_pGlContext);
        m_pGlContext = nullptr;
    }
    if (m_pGlWrappedQtContext) {
        // Only wraps Qt's context (Qt still owns the underlying CGL
        // context); unref just drops our GStreamer-side wrapper.
        gst_object_unref(m_pGlWrappedQtContext);
        m_pGlWrappedQtContext = nullptr;
    }
    if (m_pGlDisplay) {
        gst_object_unref(m_pGlDisplay);
        m_pGlDisplay = nullptr;
    }
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

    // Stage 5: same Qt::QueuedConnection reasoning as play/position above
    // -- the tempo/pitch slider is the same kind of same-thread QML write.
    deck.pRateControl = std::make_unique<ControlProxy>(ConfigKey(channelGroup, "rate_ratio"), this);
    deck.pRateControl->connectValueChanged(
            this,
            [this, &deck](double value) { handleRateChanged(deck, value); },
            Qt::QueuedConnection);
    deck.lastAppliedRate = deck.pRateControl->get();
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
        } else {
            // Stage 4: ordinary small-tick playback advance -- neither
            // threshold above fired, so this is the continuous drift
            // correction path, throttled so it checks (and, if it fires,
            // re-seeks) at most a couple of times a second rather than on
            // every single audio tick. Widened from an initial 300ms
            // (real testing showed an occasional visible glitch, most
            // likely this check firing on ordinary decode/audio
            // scheduling jitter rather than genuine drift -- see
            // maybeCorrectContinuousDrift()'s own tolerance, widened at
            // the same time).
            //
            // Stage 5: widened further (750ms -> 1500ms) now that
            // handleRateChanged() rate-tracks steady tempo/pitch/sync/
            // vinyl-pitch changes directly -- this check's job shifted from
            // "the primary way non-1x playback ever gets corrected" to a
            // coarse safety net for what rate-tracking can't see (vinyl's
            // throttled rate_ratio lag, the async delay between a rate
            // change and its queued rate-seek landing, transient scratch/
            // backspin excursions). It doesn't need to fire nearly as often
            // as before.
            constexpr qint64 kDriftCheckThrottleMs = 1500;
            if (!deck.driftCheckThrottle.isValid() ||
                    deck.driftCheckThrottle.elapsed() >= kDriftCheckThrottleMs) {
                deck.driftCheckThrottle.restart();
                maybeCorrectContinuousDrift(deck);
            }
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

void VideoEngineManager::handleRateChanged(DeckVideoContext& deck, double newRate) {
    double previousRate = deck.lastAppliedRate;
    deck.lastAppliedRate = newRate;
    if (!deck.pDecode) {
        return; // no real clip loaded on this deck -- nothing to rate-seek
    }
    bool sameSign = (newRate >= 0.0) == (previousRate >= 0.0);
    if (!sameSign) {
        // A direction change (e.g. through a backspin/reverse excursion) --
        // see this method's declaration comment for why rate-tracking
        // deliberately doesn't attempt to follow through one. The existing
        // position-based correction paths (handlePositionChanged()'s
        // backward-jump branch, maybeCorrectContinuousDrift()) pick up the
        // slack once the deck settles back onto a steady rate.
        return;
    }
    // 0.002 (~0.2%): finer than any real tempo/pitch UI step, coarse enough
    // that ordinary floating-point noise in rate_ratio's own value doesn't
    // trigger a reseek on every tick.
    constexpr double kRateEpsilon = 0.002;
    if (std::abs(newRate - previousRate) < kRateEpsilon) {
        return;
    }
    GstElement* pDecodeElement = deck.pDecode;
    gst_object_ref(pDecodeElement);
    QMetaObject::invokeMethod(
            deck.pSeekWorkerContext,
            [this, pDecodeElement, newRate]() {
                gint64 currentPositionNs = -1;
                if (gst_element_query_position(
                            pDecodeElement, GST_FORMAT_TIME, &currentPositionNs) &&
                        currentPositionNs >= 0) {
                    // Stage 5: a standalone spike (see
                    // src/videoengine/CMakeLists.txt's history comment)
                    // confirmed GST_SEEK_FLAG_INSTANT_RATE_CHANGE is
                    // outright rejected by this pipeline's decodebin
                    // chain -- gst_element_seek() returned FALSE every
                    // time, not merely "accepted but ignored". This is
                    // therefore an ordinary flushing, accurate seek that
                    // only changes the *rate*, keeping the current
                    // position as the new segment's start (the canonical
                    // GStreamer rate-change pattern: SET/currentPosition
                    // for a forward rate, SET/currentPosition as the
                    // *stop* bound for a reverse one).
                    bool seekOk = gst_element_seek(pDecodeElement,
                            newRate,
                            GST_FORMAT_TIME,
                            static_cast<GstSeekFlags>(
                                    GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
                            GST_SEEK_TYPE_SET,
                            newRate >= 0.0 ? currentPositionNs : 0,
                            GST_SEEK_TYPE_SET,
                            newRate >= 0.0 ? -1 : currentPositionNs);
                    kLogger.info()
                            << "[diag] handleRateChanged" << GST_OBJECT_NAME(pDecodeElement)
                            << "newRate=" << newRate << "currentPositionNs=" << currentPositionNs
                            << "seekOk=" << seekOk;
                }
                gst_object_unref(pDecodeElement);
            },
            Qt::QueuedConnection);
}

void VideoEngineManager::maybeCorrectContinuousDrift(DeckVideoContext& deck) {
    // Stage 5: this used to be the *only* mechanism catching non-1x
    // playback drift, firing (and re-seeking) roughly whenever a deck's
    // tempo/pitch differed from 1x at all, indefinitely, for as long as
    // that deck kept playing at that rate. handleRateChanged() now
    // rate-tracks steady tempo/pitch/sync/vinyl-pitch changes directly (the
    // video's own playback rate actually matches, not just gets
    // periodically re-seeked back into alignment), so this method's real
    // job shrank to a coarse safety net for what rate-tracking can't see:
    // vinyl's throttled/direction-stripped `rate_ratio` echo, the async gap
    // between a rate change and its queued rate-seek actually landing, and
    // transient scratch/backspin excursions (handleRateChanged() explicitly
    // skips those). Widened tolerance/throttle below reflect that reduced
    // role.
    if (!deck.pDecode) {
        return; // no real clip loaded -- nothing to drift-correct
    }
    gint64 actualVideoNs = -1;
    // Query the pipeline, not deck.pDecode (decodebin) directly. Real
    // testing found this method firing constantly, every throttle window,
    // reporting a huge (~1 second), slowly-growing "drift" even during
    // completely healthy playback -- decodebin's own reported position
    // reflects how far it has read/decoded *into* the stream, not what's
    // actually reached the appsink and been displayed yet; the queue
    // downstream of it (max-size-time=500ms) plus decodebin's own internal
    // buffering can legitimately sit up to ~1 second ahead of the real,
    // rendered frame. Comparing that read-ahead position against the
    // audio's actual position produced a large, mostly-fake "drift" that
    // this method then "corrected" with a real, disruptive flush-seek --
    // the actual cause of an occasional visible glitch during otherwise-
    // untouched playback. Querying the top-level pipeline instead resolves
    // through its sink (the appsink), reflecting what's actually been
    // consumed/rendered, which is what this comparison needs.
    if (!deck.pPipeline ||
            !gst_element_query_position(deck.pPipeline, GST_FORMAT_TIME, &actualVideoNs) ||
            actualVideoNs < 0) {
        return;
    }
    gint64 durationNs = 0;
    if (!gst_element_query_duration(deck.pDecode, GST_FORMAT_TIME, &durationNs) ||
            durationNs <= 0) {
        return;
    }
    double currentAudioPosition = deck.pPositionControl->get();
    // Delta since the anchor, mapped through this deck's own queried
    // duration only -- never an absolute audioPosition * duration
    // recomputation (Stage 3o's mistake, see the class doc comment's
    // Stage 3o and Stage 4 entries). A structural per-file duration
    // mismatch between the audio engine's and GStreamer's independently-
    // computed durations now only ever contaminates this one small,
    // recent delta, not an ever-growing absolute offset.
    double audioPositionDelta = currentAudioPosition - deck.anchorAudioPosition;
    qint64 expectedVideoNs = deck.anchorVideoPositionNs +
            static_cast<qint64>(audioPositionDelta * static_cast<double>(durationNs));
    qint64 driftNs = expectedVideoNs - actualVideoNs;
    // 500ms: widened from an initial 80ms (real testing showed an
    // occasional visible glitch during otherwise-healthy playback -- 80ms
    // was tight enough that ordinary decode/audio-engine scheduling jitter,
    // not genuine drift, could cross it) and again from 250ms (Stage 5:
    // handleRateChanged() now rate-tracks steady non-1x playback directly,
    // so this check no longer needs to catch that case itself -- it only
    // needs to catch what rate-tracking can't see, per this method's own
    // Stage 5 comment further down). Still a starting point, not a final
    // tuned value.
    constexpr qint64 kContinuousDriftToleranceNs = 500000000;
    if (qAbs(driftNs) <= kContinuousDriftToleranceNs) {
        // Within tolerance -- deliberately leave the anchor as-is rather
        // than refreshing it on a no-op check, so a genuinely slow,
        // gradual drift still accumulates toward eventually crossing
        // tolerance instead of being masked by constant micro-resets.
        return;
    }
    kLogger.info() << "[diag] maybeCorrectContinuousDrift" << GST_OBJECT_NAME(deck.pDecode)
                    << "actualVideoNs=" << actualVideoNs << "expectedVideoNs=" << expectedVideoNs
                    << "driftNs=" << driftNs;
    // Same short-wait, no-frame-step treatment as the beatloop-reset case
    // above -- this can repeat every throttle window (750ms) for as long
    // as a deck keeps drifting, so it needs to stay cheap, not the
    // rare-event 1s-wait path.
    constexpr qint64 kContinuousCorrectionWaitNs = 150000000; // 150ms
    seekElementAsync(deck,
            expectedVideoNs,
            kContinuousCorrectionWaitNs,
            /*stepFrameAfter=*/false,
            [this, &deck, expectedVideoNs, currentAudioPosition]() {
                deck.anchorVideoPositionNs = expectedVideoNs;
                deck.anchorAudioPosition = currentAudioPosition;
            });
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
    // A camera is a LIVE source: by GStreamer design a live source
    // delivers no buffers at all while PAUSED, so tying it to the audio
    // deck's play state would leave the preview permanently black
    // whenever that deck isn't playing a track -- which is most of the
    // time, and was exactly the observed symptom (camera device opens,
    // capture indicator lights up, preview stays black). None of the
    // play/pause machinery below is meaningful for it anyway: it has no
    // position to stay synced to, the same reason
    // seekDeckToCurrentPosition() already early-returns for this branch.
    // So while on camera this deck's pipeline just stays PLAYING for as
    // long as the camera is selected.
    if (deck.useCamera) {
        gst_element_set_state(deck.pPipeline, GST_STATE_PLAYING);
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
    // Stage 4: every caller of this method is a discrete, rare-event
    // re-sync (play-start, a big forward jump, a beatloop reset, or
    // initial clip load) -- exactly the kind of known-good sync point
    // maybeCorrectContinuousDrift()'s anchor-and-delta design needs to
    // reset against. Setting it here, once, covers all of those callers
    // automatically rather than needing each call site to remember to.
    seekElementAsync(deck,
            targetNs,
            waitNs,
            stepFrameAfter,
            [this, &deck, targetNs, position, onLanded = std::move(onLanded)]() {
                deck.anchorVideoPositionNs = targetNs;
                deck.anchorAudioPosition = position;
                deck.driftCheckThrottle.restart();
                if (onLanded) {
                    onLanded();
                }
            });
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

    const bool useGl = isGlPreviewAvailable();
    const QString pipelineDesc =
            deckPipelineDescFor(deck.videoPath, decodeName, sinkName, deck.useCamera, useGl);

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

    // Stage 7: glupload (present only when useGl asked for the GL tail)
    // needs to find the app-shared GstGLDisplay/GstGLContext this class set
    // up in setSharedGlContext() -- it asks for them via
    // GST_MESSAGE_NEED_CONTEXT on this pipeline's bus, which the sync
    // handler answers synchronously, before the state change below can
    // complete (the requesting element blocks on the answer as part of its
    // own state change). Must be installed before gst_element_set_state()
    // is called, and the handler's user-data is heap-allocated once per
    // pipeline rebuild since gst_bus_set_sync_handler() takes ownership via
    // its GDestroyNotify.
    if (useGl) {
        GstBus* pBus = gst_element_get_bus(pPipeline);
        gst_bus_set_sync_handler(pBus,
                glContextBusSyncHandler,
                new GlContextBusUserData{m_pGlDisplay, m_pGlContext},
                [](gpointer data) { delete static_cast<GlContextBusUserData*>(data); });
        gst_object_unref(pBus);
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
    // Stage 7 fix (defense in depth; QmlVideoPreviewItem::poll() already
    // skips calling this while GL is live): once the pipelines have been
    // rebuilt with the GL tail, their appsinks carry
    // video/x-raw(memory:GLMemory),format=RGBA. pullFrameFromAppSink()
    // below maps for CPU READ and reads plane 0 as packed RGB888, so
    // letting it run against GL buffers means both a forced GPU->CPU
    // readback and a misinterpreted format -- and, worse, it CONSUMES
    // the sample the GL path needs (these sinks are max-buffers=1
    // drop=true, so every sample taken here is one the GL item never
    // sees). Never touch the appsinks in GL mode.
    if (isGlPreviewAvailable()) {
        return QImage();
    }
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
    QImage composite = blendFrames(m_deckA.lastFrame, m_deckB.lastFrame, crossfaderValue);
#ifdef __VIDEO_ENGINE_NDI_OUTPUT__
    // Stage 6: piggyback on this same poll cadence (QmlVideoPreviewItem's
    // ~33ms timer, the only real caller of grabPreviewFrame()) rather than
    // running a second independent timer -- reuses the composite already
    // computed above, no duplicate blending work.
    if (m_pNdiSender && m_pNdiSender->isRunning()) {
        m_pNdiSender->pushFrame(composite);
    }
#endif
    return composite;
}

VideoEngineManager::GlPreviewFrames VideoEngineManager::grabPreviewGlFrames() {
    GlPreviewFrames result;
    if (!isGlPreviewAvailable()) {
        return result;
    }
    result.crossfaderValue = m_pCrossfader ? m_pCrossfader->get() : 0.0;
    result.deckA = pullGlFrameFromAppSink(m_deckA.pAppSink);
    result.deckB = pullGlFrameFromAppSink(m_deckB.pAppSink);
    return result;
}

void VideoEngineManager::releaseGlFrameSample(GstSample* pSample) {
    if (pSample) {
        gst_sample_unref(pSample);
    }
}

bool VideoEngineManager::setSharedGlContext(quintptr nativeCglContextHandle, GlApi api) {
    if (nativeCglContextHandle == 0) {
        return false;
    }
    if (m_pGlContext) {
        // Already set up (QmlVideoPreviewGlItem is expected to call this
        // once, but guard defensively rather than leaking/re-wrapping).
        return true;
    }

    GstGLDisplay* pDisplay = gst_gl_display_new();
    // Step 1: wrap Qt Quick's render-thread context. This is ONLY ever
    // the share-group source for step 2 -- it is deliberately never given
    // to a pipeline. Handing the wrapped Qt context straight to glupload
    // (what this code originally did) makes GStreamer's streaming thread
    // activate Qt's own CGL context concurrently with Qt's render thread,
    // which is undefined behaviour and faults inside the GPU driver --
    // the black-preview-then-hard-crash, with no macOS crash report
    // (it never becomes a Mach exception), seen on both earlier attempts.
    GstGLAPI gstApi = GST_GL_API_OPENGL;
    switch (api) {
    case GlApi::OpenGl3:
        gstApi = GST_GL_API_OPENGL3;
        break;
    case GlApi::Gles2:
        gstApi = GST_GL_API_GLES2;
        break;
    case GlApi::OpenGlLegacy:
        gstApi = GST_GL_API_OPENGL;
        break;
    }
    GstGLContext* pWrappedQtContext = gst_gl_context_new_wrapped(pDisplay,
            static_cast<guintptr>(nativeCglContextHandle),
            GST_GL_PLATFORM_CGL,
            gstApi);
    if (!pWrappedQtContext) {
        kLogger.warning() << "setSharedGlContext: gst_gl_context_new_wrapped failed";
        gst_object_unref(pDisplay);
        return false;
    }

    // gst_gl_context_create() below needs the share source's API/version
    // info filled in, which requires it to be active. Safe here precisely
    // because this runs on Qt's render thread from updatePaintNode(),
    // where Qt's context is already current -- this is the one place in
    // the process where touching it is legitimate.
    // NOTE: activate(TRUE) here only updates GStreamer's own
    // "which context is current on this thread" bookkeeping, which
    // fill_info() requires -- Qt has already made this very context
    // current on this (the render) thread, so nothing actually changes at
    // the CGL level.
    //
    // Crucially there is NO matching activate(FALSE): that lowers to
    // CGLSetCurrentContext(NULL), which would rip Qt's own context out
    // from under the render thread in the middle of updatePaintNode().
    // Qt Quick carries on issuing GL calls immediately after this
    // function returns and would do so with no current context at all --
    // a hard crash, and the cause of the third crash seen while getting
    // this path working. The context simply stays current, which is the
    // state Qt expects and the state it was already in on entry.
    GError* pError = nullptr;
    gst_gl_context_activate(pWrappedQtContext, TRUE);
    if (!gst_gl_context_fill_info(pWrappedQtContext, &pError)) {
        kLogger.warning() << "setSharedGlContext: gst_gl_context_fill_info failed:"
                          << (pError ? pError->message : "unknown");
        g_clear_error(&pError);
        gst_object_unref(pWrappedQtContext);
        gst_object_unref(pDisplay);
        return false;
    }

    // Step 2: GStreamer gets its OWN context in the same share group, so
    // textures it creates are visible to Qt's context (that's what keeps
    // the path zero-copy) while neither thread ever touches the other's
    // context.
    // Deliberately fprintf+fflush rather than kLogger: every crash while
    // getting this path working lost the tail of mixxx.log to buffering,
    // so the one place most likely to die needs output that is already on
    // disk before the next call runs.
    fprintf(stderr, "[gl-preview] wrapped Qt context OK, creating shared GStreamer context\n");
    fflush(stderr);

    GstGLContext* pGstContext = gst_gl_context_new(pDisplay);
    if (!pGstContext || !gst_gl_context_create(pGstContext, pWrappedQtContext, &pError)) {
        kLogger.warning() << "setSharedGlContext: could not create a GStreamer GL context "
                             "sharing with Qt's:"
                          << (pError ? pError->message : "unknown");
        g_clear_error(&pError);
        if (pGstContext) {
            gst_object_unref(pGstContext);
        }
        gst_object_unref(pWrappedQtContext);
        gst_object_unref(pDisplay);
        return false;
    }

    m_pGlDisplay = pDisplay;
    m_pGlWrappedQtContext = pWrappedQtContext;
    m_pGlContext = pGstContext;
    fprintf(stderr, "[gl-preview] shared GStreamer GL context created; rebuilding pipelines\n");
    fflush(stderr);
    kLogger.info() << "GL preview: GStreamer GL context created in Qt's share group -- "
                      "zero-copy GPU preview path ACTIVE (CPU preview path stands down)";

    // Neither deck's pipeline exists yet in GL form -- rebuild both now
    // that isGlPreviewAvailable() reports true, so deckPipelineDescFor()
    // routes them through the GL tail instead of the CPU one. Preserves
    // each deck's existing videoPath/useCamera state (rebuildDeckPipeline()
    // reads it off the deck, same as loadVideo()/setCameraSource() already
    // do for an in-place rebuild).
    rebuildDeckPipeline(m_deckA, QStringLiteral("decodeA"), QStringLiteral("sinkA"));
    rebuildDeckPipeline(m_deckB, QStringLiteral("decodeB"), QStringLiteral("sinkB"));
    return true;
}

#ifdef __VIDEO_ENGINE_NDI_OUTPUT__
bool VideoEngineManager::enableNdiOutput(bool enabled, const QString& sourceName) {
    if (!enabled) {
        if (m_pNdiSender) {
            m_pNdiSender->stop();
        }
        return true;
    }
    // Constructed lazily (not in the constructor) so a user who never
    // touches NDI output never pays NDIlib_initialize()'s cost at all --
    // same "don't do the expensive thing until asked" reasoning as camera
    // input never eagerly opening AVFoundation either.
    if (!m_pNdiSender) {
        m_pNdiSender = std::make_unique<NdiOutputSender>();
    }
    return m_pNdiSender->start(sourceName);
}
#endif

} // namespace mixxx
