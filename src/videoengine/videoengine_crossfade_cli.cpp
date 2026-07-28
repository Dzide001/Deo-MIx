// M12 Stage 3d-1 validation tool: proves a GStreamer `compositor` pipeline
// genuinely crossfades between two fully-overlapping sources as pad alpha
// changes, before wiring this up to the real [Master]/crossfader
// ControlObject in VideoEngineManager (Stage 3d-2). Standalone, no
// dependency on mixxx-lib, same role as videoengine_cli.cpp for Stage 1/2.
//
// Uses two solid-color videotestsrc patterns (white for "deck A", blue for
// "deck B") so the blend result is numerically predictable: at the red
// channel, source A is 255, source B is 0. Three independent pipeline runs
// at the three representative crossfader positions (-1, 0, +1, matching
// [Master]/crossfader's real -1.0..1.0 range) each pull one composited
// frame via appsink and read its center pixel with gst_video_frame_map (not
// a raw byte-count check like Stage 1/2's appsink test -- this needs the
// real stride-aware pixel value). Asserts the -1/+1 extremes land near the
// two source colors and the 0 position lands strictly between them --
// proving alpha genuinely drives the blend, without hardcoding
// compositor's exact internal blend formula (which involves a
// two-stage "src-over" against a background layer, not a simple weighted
// average of the two source frames).

#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include <cstdio>
#include <cstdlib>

extern "C" {
GST_PLUGIN_STATIC_DECLARE(coreelements);
GST_PLUGIN_STATIC_DECLARE(compositor);
GST_PLUGIN_STATIC_DECLARE(videotestsrc);
GST_PLUGIN_STATIC_DECLARE(videoconvertscale);
GST_PLUGIN_STATIC_DECLARE(app);
}

namespace {

void registerStaticPlugins() {
    GST_PLUGIN_STATIC_REGISTER(coreelements);
    GST_PLUGIN_STATIC_REGISTER(compositor);
    GST_PLUGIN_STATIC_REGISTER(videotestsrc);
    GST_PLUGIN_STATIC_REGISTER(videoconvertscale);
    GST_PLUGIN_STATIC_REGISTER(app);
}

constexpr int kWidth = 64;
constexpr int kHeight = 64;

// Mirrors the mapping Stage 3d-2 will use against the real
// [Master]/crossfader CO (range -1.0..1.0): -1 = deck A fully audible/
// visible, +1 = deck B, 0 = even blend.
void alphasForPosition(double position, double* pAlphaA, double* pAlphaB) {
    *pAlphaA = (1.0 - position) / 2.0;
    *pAlphaB = (1.0 + position) / 2.0;
}

// Pulls one composited frame at the given crossfader position and returns
// the red channel of the center pixel (0-255), or -1 on failure.
int sampleRedChannelAtPosition(double position) {
    double alphaA = 0.0;
    double alphaB = 0.0;
    alphasForPosition(position, &alphaA, &alphaB);

    gchar* pPipelineDesc = g_strdup_printf(
            "compositor name=comp background=black "
            "sink_0::xpos=0 sink_0::ypos=0 sink_0::alpha=%f "
            "sink_1::xpos=0 sink_1::ypos=0 sink_1::alpha=%f "
            "! video/x-raw,format=RGB,width=%d,height=%d "
            "! appsink name=sink sync=false "
            "videotestsrc pattern=white num-buffers=5 "
            "! video/x-raw,width=%d,height=%d ! comp.sink_0 "
            "videotestsrc pattern=blue num-buffers=5 "
            "! video/x-raw,width=%d,height=%d ! comp.sink_1",
            alphaA,
            alphaB,
            kWidth,
            kHeight,
            kWidth,
            kHeight,
            kWidth,
            kHeight);

    GError* pError = nullptr;
    GstElement* pPipeline = gst_parse_launch(pPipelineDesc, &pError);
    g_free(pPipelineDesc);
    if (pError) {
        std::fprintf(stderr, "  failed to build pipeline: %s\n", pError->message);
        g_clear_error(&pError);
        return -1;
    }

    GstElement* pAppSink = gst_bin_get_by_name(GST_BIN(pPipeline), "sink");
    gst_element_set_state(pPipeline, GST_STATE_PLAYING);

    int redValue = -1;
    GstSample* pSample = gst_app_sink_pull_sample(GST_APP_SINK(pAppSink));
    if (pSample) {
        GstCaps* pCaps = gst_sample_get_caps(pSample);
        GstVideoInfo videoInfo;
        GstBuffer* pBuffer = gst_sample_get_buffer(pSample);
        if (pCaps && pBuffer && gst_video_info_from_caps(&videoInfo, pCaps)) {
            GstVideoFrame frame;
            if (gst_video_frame_map(&frame, &videoInfo, pBuffer, GST_MAP_READ)) {
                guint8* pData = static_cast<guint8*>(GST_VIDEO_FRAME_PLANE_DATA(&frame, 0));
                gint stride = GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 0);
                gint centerX = kWidth / 2;
                gint centerY = kHeight / 2;
                guint8* pPixel = pData + (centerY * stride) + (centerX * 3);
                redValue = pPixel[0];
                gst_video_frame_unmap(&frame);
            }
        }
        gst_sample_unref(pSample);
    } else {
        std::fprintf(stderr, "  gst_app_sink_pull_sample returned NULL\n");
    }

    gst_element_set_state(pPipeline, GST_STATE_NULL);
    gst_object_unref(pAppSink);
    gst_object_unref(pPipeline);
    return redValue;
}

bool checkCrossfadeCompositor() {
    std::printf("crossfade compositor: sampling red channel at 3 crossfader positions\n");

    int redAtA = sampleRedChannelAtPosition(-1.0);
    int redAtMid = sampleRedChannelAtPosition(0.0);
    int redAtB = sampleRedChannelAtPosition(1.0);

    std::printf("  position -1 (deck A / white, R=255 expected): red=%d\n", redAtA);
    std::printf("  position  0 (even blend): red=%d\n", redAtMid);
    std::printf("  position +1 (deck B / blue, R=0 expected):    red=%d\n", redAtB);

    if (redAtA < 0 || redAtMid < 0 || redAtB < 0) {
        std::printf("      FAIL (a sample failed to pull)\n");
        return false;
    }

    // Extremes should land close to their pure source color; the exact
    // tolerance just needs to rule out "alpha did nothing" (which would
    // make all three values identical) without hardcoding compositor's
    // exact two-stage over-blend arithmetic against the background layer.
    bool extremesOk = redAtA > 200 && redAtB < 55;
    bool midIsBetween = redAtMid < redAtA && redAtMid > redAtB;

    bool ok = extremesOk && midIsBetween;
    std::printf("      %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

} // namespace

int main(int argc, char** argv) {
    gst_init(&argc, &argv);
    registerStaticPlugins();

    std::printf("videoengine_crossfade_cli: GStreamer version: %s\n\n", gst_version_string());

    bool ok = checkCrossfadeCompositor();

    std::printf("\nvideoengine_crossfade_cli: %s\n", ok ? "ALL CHECKS PASSED" : "ONE OR MORE CHECKS FAILED");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
