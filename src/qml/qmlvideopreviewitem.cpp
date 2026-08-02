#include "qml/qmlvideopreviewitem.h"

#include <QImage>
#include <QPainter>
#include <QTimer>

#include "moc_qmlvideopreviewitem.cpp"

namespace mixxx {
namespace qml {

class QmlVideoPreviewItem::Impl {
  public:
    explicit Impl(QmlVideoPreviewItem* pOwner)
            : m_pOwner(pOwner) {
        m_pTimer = new QTimer(pOwner);
        // ~30fps, matching the compositor's own output framerate.
        m_pTimer->setInterval(33);
        QObject::connect(m_pTimer, &QTimer::timeout, pOwner, [this]() {
            poll();
        });
        m_pTimer->start();
    }

    void paint(QPainter* pPainter, const QRectF& bounds) {
        if (!m_latestFrame.isNull()) {
            pPainter->drawImage(bounds, m_latestFrame);
        }
    }

  private:
    void poll() {
        if (!QmlVideoPreviewItem::s_pVideoEngineManager) {
            return;
        }
        // Stage 7 fix: this CPU item and QmlVideoPreviewGlItem both pull
        // from the SAME per-deck appsinks, and an appsink pull CONSUMES
        // the sample. Those sinks are `max-buffers=1 drop=true`, so with
        // both items live the two paths raced for a one-deep queue and
        // each only ever won a fraction of the frames -- the GL item was
        // being starved by this timer. On top of that, once the GL tail
        // is in use the buffers are GLMemory/RGBA, which
        // pullFrameFromAppSink() would map for CPU READ and then
        // reinterpret as RGB888. So the two paths are strictly mutually
        // exclusive, not a stacked base layer plus overlay as originally
        // designed: once GL is live this item stops pulling entirely and
        // keeps painting its last CPU frame (immediately covered by the
        // GL item's own output, so it never actually shows).
        if (QmlVideoPreviewItem::s_pVideoEngineManager->isGlPreviewAvailable()) {
            return;
        }
        // timeoutMs=0: never block the UI thread -- returns immediately
        // (null QImage) if the compositor hasn't produced a new frame
        // since the last poll, rather than waiting for one.
        QImage frame = QmlVideoPreviewItem::s_pVideoEngineManager->grabPreviewFrame(0);
        if (frame.isNull()) {
            return;
        }
        m_latestFrame = frame;
        m_pOwner->update();
    }

    QmlVideoPreviewItem* m_pOwner;
    QTimer* m_pTimer;
    QImage m_latestFrame;
};

QmlVideoPreviewItem::QmlVideoPreviewItem(QQuickItem* pParent)
        : QQuickPaintedItem(pParent),
          m_pImpl(std::make_unique<Impl>(this)) {
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
}

QmlVideoPreviewItem::~QmlVideoPreviewItem() = default;

void QmlVideoPreviewItem::paint(QPainter* pPainter) {
    m_pImpl->paint(pPainter, boundingRect());
}

} // namespace qml
} // namespace mixxx
