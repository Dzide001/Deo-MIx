#include "qml/qmlcdgpreviewitem.h"

#include <QImage>
#include <QPainter>
#include <QTimer>

#include "moc_qmlcdgpreviewitem.cpp"

namespace mixxx {
namespace qml {

class QmlCdgPreviewItem::Impl {
  public:
    explicit Impl(QmlCdgPreviewItem* pOwner) : m_pOwner(pOwner) {
        m_pTimer = new QTimer(pOwner);
        // Matches QmlVideoPreviewItem's own poll cadence -- smooth enough
        // for CDG's XOR-based blink/highlight animations without adding
        // meaningful cost (frame lookup is a binary search over already-
        // decoded keyframes, no re-decoding happens here).
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
        if (!QmlCdgPreviewItem::s_pKaraokeManager || m_pOwner->m_group.isEmpty()) {
            return;
        }
        QImage frame = QmlCdgPreviewItem::s_pKaraokeManager->currentCdgFrame(
                m_pOwner->m_group, m_pOwner->m_positionSeconds);
        if (frame.isNull()) {
            return;
        }
        m_latestFrame = frame;
        m_pOwner->update();
    }

    QmlCdgPreviewItem* m_pOwner;
    QTimer* m_pTimer;
    QImage m_latestFrame;
};

QmlCdgPreviewItem::QmlCdgPreviewItem(QQuickItem* pParent)
        : QQuickPaintedItem(pParent),
          m_pImpl(std::make_unique<Impl>(this)) {
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
}

QmlCdgPreviewItem::~QmlCdgPreviewItem() = default;

void QmlCdgPreviewItem::paint(QPainter* pPainter) {
    m_pImpl->paint(pPainter, boundingRect());
}

} // namespace qml
} // namespace mixxx
