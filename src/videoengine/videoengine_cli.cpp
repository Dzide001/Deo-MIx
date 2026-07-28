// M12 Stage 1/2 validation tool, mirroring stemsep/stemwriter_cli.cpp's
// role for the stem separator: a standalone program with no dependency on
// mixxx-lib, proving the video engine (GStreamer, vcpkg-built per
// milestone_12_video_spec_addendum.md) actually compiles/links/runs from
// real C++ before any Mixxx integration is attempted.
//
// Four independent checks, each proving one piece of the architecture
// diagram in the addendum doc:
//   1. compositor  -- CPU multi-source layer compositing (deck A/B + camera)
//   2. glvideomixer -- the GPU-accelerated equivalent
//   3. avfvideosrc  -- macOS camera capture (device enumeration only; real
//      frame capture needs a one-time OS permission grant this CLI can't
//      satisfy non-interactively, so this check stops short of PLAYING)
//   4. appsink      -- pulling real decoded frames out into application
//      code, the actual mechanism the future QML preview bridge needs
//      (qml6glsink is confirmed unavailable -- see the addendum doc)

#include <gst/app/gstappsink.h>
#include <gst/gst.h>

#include <cstdio>
#include <cstdlib>

// vcpkg's gstreamer port builds statically (no .dylib plugins to scan at
// runtime), and the gstreamer-full umbrella library it also advertises via
// pkgconfig turned out not to actually be built (gstreamer-full-1.0.pc
// exists, libgstreamer-full-1.0.a does not) -- so each plugin actually used
// here has to be declared/registered explicitly and linked directly,
// per GStreamer's own documented static-linking pattern. The plugin
// registration functions are plain C symbols (compiled as part of a C
// library); GST_PLUGIN_STATIC_DECLARE's `extern void ...(void)` expansion
// needs an explicit extern "C" block here to avoid C++ name mangling.
extern "C" {
GST_PLUGIN_STATIC_DECLARE(coreelements);
GST_PLUGIN_STATIC_DECLARE(compositor);
GST_PLUGIN_STATIC_DECLARE(videotestsrc);
GST_PLUGIN_STATIC_DECLARE(videoconvertscale);
GST_PLUGIN_STATIC_DECLARE(opengl);
GST_PLUGIN_STATIC_DECLARE(applemedia);
GST_PLUGIN_STATIC_DECLARE(osxvideo);
GST_PLUGIN_STATIC_DECLARE(app);
}

namespace {

void registerStaticPlugins() {
    GST_PLUGIN_STATIC_REGISTER(coreelements);
    GST_PLUGIN_STATIC_REGISTER(compositor);
    GST_PLUGIN_STATIC_REGISTER(videotestsrc);
    GST_PLUGIN_STATIC_REGISTER(videoconvertscale);
    GST_PLUGIN_STATIC_REGISTER(opengl);
    GST_PLUGIN_STATIC_REGISTER(applemedia);
    GST_PLUGIN_STATIC_REGISTER(osxvideo);
    GST_PLUGIN_STATIC_REGISTER(app);
}

struct BusWatchContext {
    GMainLoop* pLoop;
    bool sawEos;
};

gboolean onBusMessage(GstBus*, GstMessage* pMessage, gpointer pUserData) {
    BusWatchContext* pContext = static_cast<BusWatchContext*>(pUserData);
    switch (GST_MESSAGE_TYPE(pMessage)) {
    case GST_MESSAGE_EOS:
        pContext->sawEos = true;
        g_main_loop_quit(pContext->pLoop);
        break;
    case GST_MESSAGE_ERROR: {
        GError* pError = nullptr;
        gchar* pDebugInfo = nullptr;
        gst_message_parse_error(pMessage, &pError, &pDebugInfo);
        std::fprintf(stderr,
                "  ERROR from element %s: %s\n",
                GST_OBJECT_NAME(pMessage->src),
                pError->message);
        if (pDebugInfo) {
            std::fprintf(stderr, "  debug info: %s\n", pDebugInfo);
        }
        g_clear_error(&pError);
        g_free(pDebugInfo);
        g_main_loop_quit(pContext->pLoop);
        break;
    }
    default:
        break;
    }
    return TRUE;
}

// Runs pPipelineDesc to EOS (or error). Returns true only on clean EOS.
bool runPipelineToCompletion(const gchar* pPipelineDesc) {
    GError* pError = nullptr;
    GstElement* pPipeline = gst_parse_launch(pPipelineDesc, &pError);
    if (pError) {
        std::fprintf(stderr, "  failed to build pipeline: %s\n", pError->message);
        g_clear_error(&pError);
        return false;
    }

    BusWatchContext context{g_main_loop_new(nullptr, FALSE), false};
    GstBus* pBus = gst_element_get_bus(pPipeline);
    guint watchId = gst_bus_add_watch(pBus, onBusMessage, &context);
    gst_object_unref(pBus);

    GstStateChangeReturn changeResult = gst_element_set_state(pPipeline, GST_STATE_PLAYING);
    bool stateOk = changeResult != GST_STATE_CHANGE_FAILURE;
    if (stateOk) {
        g_main_loop_run(context.pLoop);
    } else {
        std::fprintf(stderr, "  failed to set pipeline to PLAYING\n");
    }

    gst_element_set_state(pPipeline, GST_STATE_NULL);
    g_source_remove(watchId);
    gst_object_unref(pPipeline);
    g_main_loop_unref(context.pLoop);
    return stateOk && context.sawEos;
}

bool checkCompositor() {
    std::printf("[1/4] compositor (CPU multi-layer compositing)\n");
    bool ok = runPipelineToCompletion(
            "compositor name=comp "
            "sink_0::xpos=0 sink_0::ypos=0 "
            "sink_1::xpos=320 sink_1::ypos=0 "
            "! video/x-raw,width=640,height=240 "
            "! videoconvert ! fakesink "
            "videotestsrc pattern=smpte num-buffers=30 "
            "! video/x-raw,width=320,height=240 ! comp.sink_0 "
            "videotestsrc pattern=ball num-buffers=30 "
            "! video/x-raw,width=320,height=240 ! comp.sink_1");
    std::printf("      %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

bool checkGlVideoMixer() {
    std::printf("[2/4] glvideomixer (GPU-accelerated compositing)\n");
    // gldownload converts GL memory back to system memory before fakesink,
    // so this exercises the real GPU compositing path without requiring an
    // on-screen window (a real deck preview would instead end in a GL-aware
    // sink -- glimagesink for a native window, or the future appsink/QML
    // bridge for in-app preview).
    bool ok = runPipelineToCompletion(
            "glvideomixer name=mix "
            "sink_0::xpos=0 sink_0::ypos=0 "
            "sink_1::xpos=320 sink_1::ypos=0 "
            "! gldownload ! videoconvert ! fakesink "
            "videotestsrc pattern=smpte num-buffers=30 "
            "! video/x-raw,width=320,height=240 ! glupload ! mix.sink_0 "
            "videotestsrc pattern=ball num-buffers=30 "
            "! video/x-raw,width=320,height=240 ! glupload ! mix.sink_1");
    std::printf("      %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

bool checkCameraEnumeration() {
    std::printf("[3/4] avfvideosrc (macOS camera capture -- element check only)\n");
    // Full PLAYING (real frame capture) needs a one-time OS camera
    // permission grant this non-interactive CLI can't satisfy. Element
    // construction (and NULL->READY, which only touches device
    // enumeration, not capture) is what's actually being proven here: that
    // the plugin linked correctly and the class exists, not live capture.
    GstElement* pSrc = gst_element_factory_make("avfvideosrc", "camtest");
    if (!pSrc) {
        std::fprintf(stderr, "  gst_element_factory_make(\"avfvideosrc\") returned NULL\n");
        std::printf("      FAIL\n");
        return false;
    }
    GstStateChangeReturn changeResult = gst_element_set_state(pSrc, GST_STATE_READY);
    bool ok = changeResult != GST_STATE_CHANGE_FAILURE;
    gst_element_set_state(pSrc, GST_STATE_NULL);
    gst_object_unref(pSrc);
    std::printf("      %s (element linked and reached READY; real capture needs camera permission, not checked here)\n",
            ok ? "PASS" : "FAIL");
    return ok;
}

// The actual mechanism a future QML preview bridge needs: proving decoded
// frame *data* (not just GstSample metadata) is reachable from application
// code via appsink, since qml6glsink is confirmed unavailable (see the
// addendum doc). Uses pull-sample (synchronous) rather than the
// callback-based API for simplicity in a short-lived CLI check.
bool checkAppSink() {
    std::printf("[4/4] appsink (frame extraction for the future QML bridge)\n");
    GError* pError = nullptr;
    GstElement* pPipeline = gst_parse_launch(
            "videotestsrc pattern=smpte num-buffers=5 "
            "! video/x-raw,format=RGBA,width=64,height=64 "
            "! appsink name=sink sync=false",
            &pError);
    if (pError) {
        std::fprintf(stderr, "  failed to build pipeline: %s\n", pError->message);
        g_clear_error(&pError);
        std::printf("      FAIL\n");
        return false;
    }

    GstElement* pAppSink = gst_bin_get_by_name(GST_BIN(pPipeline), "sink");
    gst_element_set_state(pPipeline, GST_STATE_PLAYING);

    bool ok = false;
    GstSample* pSample = gst_app_sink_pull_sample(GST_APP_SINK(pAppSink));
    if (pSample) {
        GstBuffer* pBuffer = gst_sample_get_buffer(pSample);
        GstMapInfo mapInfo;
        if (pBuffer && gst_buffer_map(pBuffer, &mapInfo, GST_MAP_READ)) {
            // 64x64 RGBA = 16384 bytes/frame; just prove real pixel bytes
            // came through, not that any specific pattern was drawn.
            ok = mapInfo.size == 64 * 64 * 4;
            std::printf("      pulled a real frame: %zu bytes (expected %d)\n",
                    mapInfo.size,
                    64 * 64 * 4);
            gst_buffer_unmap(pBuffer, &mapInfo);
        }
        gst_sample_unref(pSample);
    } else {
        std::fprintf(stderr, "  gst_app_sink_pull_sample returned NULL\n");
    }

    gst_element_set_state(pPipeline, GST_STATE_NULL);
    gst_object_unref(pAppSink);
    gst_object_unref(pPipeline);
    std::printf("      %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

} // namespace

int main(int argc, char** argv) {
    gst_init(&argc, &argv);
    registerStaticPlugins();

    std::printf("videoengine_cli: GStreamer version: %s\n\n", gst_version_string());

    bool compositorOk = checkCompositor();
    bool glVideoMixerOk = checkGlVideoMixer();
    bool cameraOk = checkCameraEnumeration();
    bool appSinkOk = checkAppSink();

    std::printf("\nvideoengine_cli: %s\n",
            (compositorOk && glVideoMixerOk && cameraOk && appSinkOk)
                    ? "ALL CHECKS PASSED"
                    : "ONE OR MORE CHECKS FAILED");
    return (compositorOk && glVideoMixerOk && cameraOk && appSinkOk) ? EXIT_SUCCESS : EXIT_FAILURE;
}
