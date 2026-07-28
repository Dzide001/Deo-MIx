// M12 Stage 3a validation tool: proves the one piece Stage 2 didn't cover --
// that real GStreamer appsink frames can actually be displayed inside a live
// Qt Quick scene, not just extracted as bytes in a CLI. Standalone, not
// linked into mixxx-lib, matching the Stage 1/2 validate-before-integrate
// pattern (see milestone_12_video_spec_addendum.md).
//
// GstVideoItem (a QQuickPaintedItem) owns a small GStreamer pipeline
// (videotestsrc ! appsink) and repaints itself with whatever frame appsink
// most recently delivered. main() builds a real QQuickWindow around one,
// lets a few frames arrive, then grabs the rendered window content to a PNG
// file -- the only way to automatably prove real pixels made it all the way
// through the pipeline into an actual rendered Qt Quick scene, since there's
// no way to visually inspect a live window directly from this tool.
//
// This uses QQuickPaintedItem (CPU-side QPainter drawing) rather than a
// QQuickFramebufferObject with GPU texture upload -- correct proof of
// concept for "can appsink frames reach the screen at all," not yet the
// performance-appropriate path for a real deck preview (a zero-copy GL
// texture upload, closer to how rendergraph_gl/rendergraph_sg already work
// in this codebase, is real Stage 3 integration work once this is proven).

#include <gst/app/gstappsink.h>
#include <gst/gst.h>

#include <QGuiApplication>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QQuickItem>
#include <QQuickPaintedItem>
#include <QQuickWindow>
#include <QTimer>

#include <cstdio>
#include <cstdlib>

extern "C" {
GST_PLUGIN_STATIC_DECLARE(coreelements);
GST_PLUGIN_STATIC_DECLARE(videotestsrc);
GST_PLUGIN_STATIC_DECLARE(videoconvertscale);
GST_PLUGIN_STATIC_DECLARE(app);
}

namespace {

void registerStaticPlugins() {
    GST_PLUGIN_STATIC_REGISTER(coreelements);
    GST_PLUGIN_STATIC_REGISTER(videotestsrc);
    GST_PLUGIN_STATIC_REGISTER(videoconvertscale);
    GST_PLUGIN_STATIC_REGISTER(app);
}

} // namespace

class GstVideoItem : public QQuickPaintedItem {
    Q_OBJECT
  public:
    explicit GstVideoItem(QQuickItem* pParent = nullptr)
            : QQuickPaintedItem(pParent) {
        setRenderTarget(QQuickPaintedItem::FramebufferObject);

        m_pPipeline = gst_parse_launch(
                "videotestsrc pattern=smpte is-live=true "
                "! video/x-raw,format=RGB,width=320,height=240,framerate=15/1 "
                "! appsink name=sink emit-signals=true max-buffers=1 drop=true",
                nullptr);
        GstElement* pAppSink = gst_bin_get_by_name(GST_BIN(m_pPipeline), "sink");
        g_signal_connect(pAppSink, "new-sample", G_CALLBACK(&GstVideoItem::onNewSampleStatic), this);
        gst_object_unref(pAppSink);
        gst_element_set_state(m_pPipeline, GST_STATE_PLAYING);
    }

    ~GstVideoItem() override {
        if (m_pPipeline) {
            gst_element_set_state(m_pPipeline, GST_STATE_NULL);
            gst_object_unref(m_pPipeline);
        }
    }

    void paint(QPainter* pPainter) override {
        QMutexLocker locker(&m_mutex);
        if (!m_latestFrame.isNull()) {
            pPainter->drawImage(boundingRect(), m_latestFrame);
        }
        m_framesPainted++;
    }

    int framesPainted() const {
        QMutexLocker locker(&m_mutex);
        return m_framesPainted;
    }

  private:
    // Called on GStreamer's own streaming thread, never the Qt main
    // thread -- must not touch QQuickItem state directly. Copies the frame
    // into m_latestFrame under a mutex and marshals the actual repaint
    // request onto the main thread via a queued invocation.
    static GstFlowReturn onNewSampleStatic(GstElement* pSink, gpointer pUserData) {
        return static_cast<GstVideoItem*>(pUserData)->onNewSample(pSink);
    }

    GstFlowReturn onNewSample(GstElement* pSink) {
        GstSample* pSample = gst_app_sink_pull_sample(GST_APP_SINK(pSink));
        if (!pSample) {
            return GST_FLOW_ERROR;
        }
        GstBuffer* pBuffer = gst_sample_get_buffer(pSample);
        GstCaps* pCaps = gst_sample_get_caps(pSample);
        GstStructure* pStructure = gst_caps_get_structure(pCaps, 0);
        int width = 0;
        int height = 0;
        gst_structure_get_int(pStructure, "width", &width);
        gst_structure_get_int(pStructure, "height", &height);

        GstMapInfo mapInfo;
        if (gst_buffer_map(pBuffer, &mapInfo, GST_MAP_READ)) {
            QImage frame(mapInfo.data, width, height, width * 3, QImage::Format_RGB888);
            {
                QMutexLocker locker(&m_mutex);
                // .copy() detaches from the GStreamer-owned buffer before
                // it's unmapped below.
                m_latestFrame = frame.copy();
            }
            gst_buffer_unmap(pBuffer, &mapInfo);
            QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
        }
        gst_sample_unref(pSample);
        return GST_FLOW_OK;
    }

    GstElement* m_pPipeline = nullptr;
    mutable QMutex m_mutex;
    QImage m_latestFrame;
    int m_framesPainted = 0;
};

#include "videoengine_qml_bridge_cli.moc"

int main(int argc, char** argv) {
    gst_init(&argc, &argv);
    registerStaticPlugins();

    QGuiApplication app(argc, argv);

    // Plain C++ scene graph construction (no .qml file needed) -- proving
    // GstVideoItem renders inside a live QQuickWindow is the point here,
    // not exercising QML's own object-instantiation machinery, which is
    // already well-proven elsewhere in this codebase.
    QQuickWindow window;
    window.resize(320, 240);
    auto* pItem = new GstVideoItem(window.contentItem());
    pItem->setWidth(320);
    pItem->setHeight(240);
    window.show();

    int exitCode = EXIT_FAILURE;
    // Give the pipeline time to deliver a handful of real frames, then grab
    // the actual rendered window content and check it's not blank -- proof
    // the frames made it all the way from GStreamer through appsink,
    // through QQuickPaintedItem::paint(), into the real rendered scene.
    QTimer::singleShot(1500, &app, [&]() {
        QImage grabbed = window.grabWindow();
        bool hasRealContent = false;
        // A real SMPTE colorbar frame has multiple distinct colors; a blank/
        // black window (nothing ever painted) would not.
        QRgb firstPixel = grabbed.pixel(0, 0);
        for (int x = 0; x < grabbed.width() && !hasRealContent; x += 8) {
            if (grabbed.pixel(x, grabbed.height() / 2) != firstPixel) {
                hasRealContent = true;
            }
        }
        std::printf("videoengine_qml_bridge_cli: frames painted into the live window: %d\n",
                pItem->framesPainted());
        grabbed.save("/tmp/videoengine_qml_bridge_grab.png");
        std::printf("videoengine_qml_bridge_cli: grabbed window saved to /tmp/videoengine_qml_bridge_grab.png\n");
        if (pItem->framesPainted() > 0 && hasRealContent) {
            std::printf("videoengine_qml_bridge_cli: PASS -- real GStreamer frames rendered inside a live QQuickWindow\n");
            exitCode = EXIT_SUCCESS;
        } else {
            std::fprintf(stderr, "videoengine_qml_bridge_cli: FAIL -- window content looks blank/unpainted\n");
        }
        app.quit();
    });

    app.exec();
    return exitCode;
}
