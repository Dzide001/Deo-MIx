#pragma once

#include <QQuickPaintedItem>
#include <memory>

#include "library/videoengine/videoenginemanager.h"

namespace mixxx {
namespace qml {

/// M12 Stage 3g: pulls live frames from the real per-deck crossfade
/// compositor pipeline owned by VideoEngineManager (loadVideo(), the real
/// [Master]/crossfader-driven blend) via its synchronous
/// grabPreviewFrame(), polled on a QTimer -- not a private pipeline of its
/// own. Stage 3c's original standalone videotestsrc pipeline here only
/// ever existed to prove the appsink->QQuickPaintedItem bridge worked in
/// isolation before VideoEngineManager existed; it was never rewired to
/// the real manager after Stage 3d landed, which left the floating
/// preview permanently showing static SMPTE bars regardless of what was
/// loaded or the crossfader position.
///
/// Always compiled and registered as a QML type regardless of whether the
/// app was built with -DVIDEO_ENGINE=ON (VideoEngineManager's own header
/// has no VIDEO_ENGINE-conditional content, so this compiles fine either
/// way): when s_pVideoEngineManager is never seeded (non-VIDEO_ENGINE
/// build) or the manager reports unavailable, grabPreviewFrame() just
/// keeps returning a null QImage and paint() draws nothing -- no
/// #ifdef branching needed in this class at all.
class QmlVideoPreviewItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(VideoPreview)
  public:
    explicit QmlVideoPreviewItem(QQuickItem* pParent = nullptr);
    ~QmlVideoPreviewItem() override;

    void paint(QPainter* pPainter) override;

    static inline std::shared_ptr<mixxx::VideoEngineManager> s_pVideoEngineManager;

  private:
    class Impl;
    std::unique_ptr<Impl> m_pImpl;
};

} // namespace qml
} // namespace mixxx
