#pragma once

#include <QQuickItem>
#include <memory>

#include "library/videoengine/videoenginemanager.h"

namespace mixxx {
namespace qml {

/// M12 Stage 7: the GPU-upload counterpart of QmlVideoPreviewItem -- see
/// VideoEngineManager's own class doc comment for the full design (GL
/// context sharing, the GstSample-lifetime ring, why this covers the CPU
/// item rather than QML choosing between them). In short:
/// updatePaintNode() lazily hands this window's native OpenGL context to
/// VideoEngineManager::setSharedGlContext() the first time it runs, then
/// on every call pulls VideoEngineManager::grabPreviewGlFrames() and
/// builds two QSGSimpleTextureNode children wrapping the decks' raw GL
/// texture ids directly -- no QImage, no CPU copy. Before GL sharing
/// succeeds (or on any platform/Qt-RHI-backend this hasn't been wired up
/// for), grabPreviewGlFrames() always returns default-constructed
/// (textureId == 0) frames, and this simply paints nothing, same as
/// QmlVideoPreviewItem does before any real frame has arrived.
///
/// Always compiled and registered as a QML type regardless of
/// -DVIDEO_ENGINE=ON (matches QmlVideoPreviewItem's own reasoning --
/// VideoEngineManager's header has no VIDEO_ENGINE-conditional content):
/// when s_pVideoEngineManager is never seeded, isGlPreviewAvailable() and
/// grabPreviewGlFrames() are simply never reachable in a meaningful way
/// (the pointer itself is null), so updatePaintNode() just returns early.
class QmlVideoPreviewGlItem : public QQuickItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(VideoPreviewGl)
  public:
    explicit QmlVideoPreviewGlItem(QQuickItem* pParent = nullptr);
    ~QmlVideoPreviewGlItem() override;

    static inline std::shared_ptr<mixxx::VideoEngineManager> s_pVideoEngineManager;

  protected:
    QSGNode* updatePaintNode(QSGNode* pOldNode, UpdatePaintNodeData*) override;

  private:
    class Impl;
    std::unique_ptr<Impl> m_pImpl;
};

} // namespace qml
} // namespace mixxx
