#include "qml/qmlvideopreviewglitem.h"

#include <QQuickWindow>
#include <QSGNode>
#include <QSGOpacityNode>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <QTimer>
#include <QVector>
#include <QOpenGLContext>
#include <QSurfaceFormat>

#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

#if defined(Q_OS_MAC)
#include "qml/qmlvideopreviewglitem_mac.h"
#endif

#include "moc_qmlvideopreviewglitem.cpp"

namespace mixxx {
namespace qml {

namespace {
// A texture/QSGTexture pair created for one deck's frame, plus the
// GstSample that owns the underlying GL memory -- see the header's class
// doc comment and VideoEngineManager's own Stage 7 entry for why these
// can't be deleted/released the instant a newer frame replaces them
// (Qt Quick's actual GPU read of the texture the scene graph was just
// handed happens asynchronously, on the render thread's own schedule, not
// synchronously within this call). Kept in a small per-deck queue and
// only actually torn down once at least one full updatePaintNode() call
// has passed since it was current -- by then the render thread is done
// with it.
struct RetiredGlFrame {
    QRhiTexture* pTex = nullptr;
    QSGTexture* pSgTex = nullptr;
    GstSample* pSample = nullptr;
};

void destroyRetiredFrame(RetiredGlFrame& frame, VideoEngineManager* pManager) {
    delete frame.pSgTex;
    delete frame.pTex;
    if (pManager) {
        pManager->releaseGlFrameSample(frame.pSample);
    }
    frame = RetiredGlFrame();
}
} // namespace

class QmlVideoPreviewGlItem::Impl {
  public:
    explicit Impl(QmlVideoPreviewGlItem* pOwner)
            : m_pOwner(pOwner) {
    }

    ~Impl() {
        VideoEngineManager* pManager = QmlVideoPreviewGlItem::s_pVideoEngineManager.get();
        for (auto& retired : m_deckA.retiredFrames) {
            destroyRetiredFrame(retired, pManager);
        }
        for (auto& retired : m_deckB.retiredFrames) {
            destroyRetiredFrame(retired, pManager);
        }
    }

    QSGNode* updatePaintNode(QSGNode* pOldNode) {
        VideoEngineManager* pManager = QmlVideoPreviewGlItem::s_pVideoEngineManager.get();
        QQuickWindow* pWindow = m_pOwner->window();
        if (!pManager || !pWindow) {
            delete pOldNode;
            return nullptr;
        }

        // Stage 7 re-enabled (2026-08-01) after root-causing the original
        // black-preview/crash: the CPU-path item and this one were both
        // pulling from the SAME per-deck appsinks, and an appsink pull
        // consumes the sample. With `max-buffers=1 drop=true` sinks the
        // two paths raced over a one-deep queue -- this item was starved
        // of most frames -- while the CPU path additionally mapped
        // GLMemory/RGBA buffers for CPU READ and reinterpreted them as
        // RGB888. Both paths are now strictly mutually exclusive: the
        // CPU item stops polling (and grabPreviewFrame() hard-returns
        // empty) as soon as isGlPreviewAvailable() is true, which is
        // exactly what this call makes true.
        // DISABLED AGAIN (2026-08-01, second attempt). Three real,
        // separate bugs were found and fixed along the way (see
        // master_decision_list.md's M12 entry for the full account) --
        // the CPU/GL appsink sample-stealing race, handing GStreamer the
        // wrapped Qt context instead of a share-group sibling, and
        // declaring a legacy 2.1 context as OPENGL3 -- but a fourth
        // blocker remains: with the API finally declared correctly,
        // gst_gl_context_fill_info() gets far enough to issue real GL
        // queries against Qt's context on the render thread and crashes
        // inside that call (confirmed by unbuffered stderr markers
        // placed immediately after it never printing). Re-enable only
        // with a plan for doing the context handshake OFF Qt's render
        // thread; leaving it on costs a hard crash the moment the video
        // engine is switched on, and the CPU path is fully functional.
        // ensureGlContextShared(pManager, pWindow);
        if (!pManager->isGlPreviewAvailable()) {
            // Nothing to draw yet -- the CPU-path Mixxx.VideoPreview item
            // stacked underneath this one (VideoPreviewPanel.qml) is
            // still showing its own frame; returning null here just means
            // this item itself contributes nothing on top of it.
            delete pOldNode;
            return nullptr;
        }

        QRhi* pRhi = pWindow->rhi();
        if (!pRhi || pRhi->backend() != QRhi::OpenGLES2) {
            // Only ever true if something upstream changed the app's
            // forced graphics API (coreservices.cpp forces OpenGL) --
            // nothing safe to do with a foreign GL texture id under any
            // other backend.
            delete pOldNode;
            return nullptr;
        }

        // Deck B needs an opacity that varies per frame with the
        // crossfader; QSGSimpleTextureNode (a plain QSGGeometryNode) has
        // no opacity of its own, so it's wrapped in a QSGOpacityNode --
        // deck A, drawn first/underneath, needs no such wrapper since it's
        // always fully opaque.
        auto* pRootNode = pOldNode;
        QSGSimpleTextureNode* pNodeA = nullptr;
        QSGOpacityNode* pOpacityNodeB = nullptr;
        QSGSimpleTextureNode* pNodeB = nullptr;
        if (!pRootNode) {
            pRootNode = new QSGNode();
            pNodeA = new QSGSimpleTextureNode();
            pOpacityNodeB = new QSGOpacityNode();
            pNodeB = new QSGSimpleTextureNode();
            pOpacityNodeB->appendChildNode(pNodeB);
            pRootNode->appendChildNode(pNodeA);
            pRootNode->appendChildNode(pOpacityNodeB);
        } else {
            pNodeA = static_cast<QSGSimpleTextureNode*>(pRootNode->firstChild());
            pOpacityNodeB = static_cast<QSGOpacityNode*>(pRootNode->lastChild());
            pNodeB = static_cast<QSGSimpleTextureNode*>(pOpacityNodeB->firstChild());
        }

        const QRectF bounds = m_pOwner->boundingRect();
        pNodeA->setRect(bounds);
        pNodeB->setRect(bounds);

        VideoEngineManager::GlPreviewFrames frames = pManager->grabPreviewGlFrames();

        // Deck A is always drawn fully opaque; deck B is drawn on top at
        // opacity alphaB -- since alphaA + alphaB always sum to 1 (the
        // same linear crossfader mapping blendFrames() uses on the CPU
        // path), "opaque A, then B at opacity alphaB" is mathematically
        // identical to the CPU path's per-pixel A*alphaA + B*alphaB mix.
        const double alphaB = (1.0 + frames.crossfaderValue) / 2.0;
        pOpacityNodeB->setOpacity(alphaB);

        updateDeckNode(pRhi, pWindow, pNodeA, m_deckA, frames.deckA, pManager);
        updateDeckNode(pRhi, pWindow, pNodeB, m_deckB, frames.deckB, pManager);

        return pRootNode;
    }

  private:
    struct DeckGlState {
        // Small bounded queue of frames retired by a newer one this call
        // -- drained down to at most 2 entries every call, so a sample/
        // texture is always kept alive at least one full call past the
        // one it was current for. See the anonymous namespace's
        // RetiredGlFrame comment above.
        QVector<RetiredGlFrame> retiredFrames;
    };

    // Stage 7: only ever needs to happen once -- VideoEngineManager
    // itself no-ops a second call (see setSharedGlContext()'s own guard).
    // Deliberately re-attempted on every call until it succeeds rather
    // than giving up after one failure: the window's rhi()/nativeHandles()
    // could plausibly be transiently unready on the very first
    // updatePaintNode() call after this item is created.
    void ensureGlContextShared(VideoEngineManager* pManager, QQuickWindow* pWindow) {
        if (pManager->isGlPreviewAvailable()) {
            return;
        }
#if defined(Q_OS_MAC)
        QRhi* pRhi = pWindow->rhi();
        if (!pRhi || pRhi->backend() != QRhi::OpenGLES2) {
            return;
        }
        const auto* pNativeHandles =
                static_cast<const QRhiGles2NativeHandles*>(pRhi->nativeHandles());
        if (!pNativeHandles || !pNativeHandles->context) {
            return;
        }
        quintptr nativeCglContext = mixxxGetNativeCglContext(pNativeHandles->context);
        if (nativeCglContext != 0) {
            // Tell the manager which GL flavour this context actually is,
            // rather than letting it assume. Mixxx never sets a
            // QSurfaceFormat version/profile (see
            // WaveformWidgetFactory::getSurfaceFormat), so on macOS Qt
            // gets a legacy OpenGL 2.1 compatibility context -- declaring
            // it to GStreamer as OPENGL3 made gst_gl_context_fill_info()
            // fail outright ("an opengl3 context created but the required
            // ES2 compatibility was not found"), which silently disabled
            // this whole path.
            const QSurfaceFormat format = pNativeHandles->context->format();
            const bool isEs = format.renderableType() == QSurfaceFormat::OpenGLES;
            const bool isCoreGl3Plus = !isEs &&
                    (format.majorVersion() > 3 ||
                            (format.majorVersion() == 3 && format.minorVersion() >= 2)) &&
                    format.profile() == QSurfaceFormat::CoreProfile;
            pManager->setSharedGlContext(nativeCglContext,
                    isEs ? VideoEngineManager::GlApi::Gles2
                         : (isCoreGl3Plus ? VideoEngineManager::GlApi::OpenGl3
                                          : VideoEngineManager::GlApi::OpenGlLegacy));
        }
#else
        Q_UNUSED(pWindow);
#endif
    }

    void updateDeckNode(QRhi* pRhi,
            QQuickWindow* pWindow,
            QSGSimpleTextureNode* pNode,
            DeckGlState& deck,
            const VideoEngineManager::GlFrame& frame,
            VideoEngineManager* pManager) {
        // Drain anything retired by a previous call, keeping the most
        // recent 2 alive a little longer as a safety margin (see the
        // class doc comment on why an instant release is unsafe here).
        while (deck.retiredFrames.size() > 2) {
            RetiredGlFrame oldest = deck.retiredFrames.takeFirst();
            destroyRetiredFrame(oldest, pManager);
        }

        if (frame.textureId == 0) {
            // No new frame this tick -- leave whatever this node is
            // already displaying alone (same "don't overwrite the last
            // good frame" behavior QmlVideoPreviewItem's CPU poll uses).
            if (frame.pOwningSample) {
                // Shouldn't happen (grabPreviewGlFrames() only ever sets
                // pOwningSample alongside a non-zero textureId), but never
                // leak a sample this class doesn't otherwise track.
                pManager->releaseGlFrameSample(frame.pOwningSample);
            }
            return;
        }

        QRhiTexture* pTex = pRhi->newTexture(
                QRhiTexture::RGBA8, QSize(frame.width, frame.height));
        if (!pTex || !pTex->createFrom({static_cast<quint64>(frame.textureId), 0})) {
            delete pTex;
            pManager->releaseGlFrameSample(frame.pOwningSample);
            return;
        }

        QSGTexture* pSgTex = pWindow->createTextureFromRhiTexture(pTex);
        pNode->setTexture(pSgTex);
        pNode->setOwnsTexture(false); // this class manages pSgTex's lifetime itself, see below
        pNode->markDirty(QSGNode::DirtyMaterial);

        deck.retiredFrames.append(RetiredGlFrame{pTex, pSgTex, frame.pOwningSample});
    }

    QmlVideoPreviewGlItem* m_pOwner;
    DeckGlState m_deckA;
    DeckGlState m_deckB;
};

QmlVideoPreviewGlItem::QmlVideoPreviewGlItem(QQuickItem* pParent)
        : QQuickItem(pParent),
          m_pImpl(std::make_unique<Impl>(this)) {
    setFlag(QQuickItem::ItemHasContents, true);
    // ~30fps, matching QmlVideoPreviewItem's own CPU-path poll cadence --
    // there's no live-frame-arrived signal to hook into here (the GL
    // texture is pulled fresh from the appsink every call, same as
    // grabPreviewFrame()'s pull-not-push model), so this is just what
    // drives update() to be called regularly.
    auto* pTimer = new QTimer(this);
    pTimer->setInterval(33);
    connect(pTimer, &QTimer::timeout, this, &QQuickItem::update);
    pTimer->start();
}

QmlVideoPreviewGlItem::~QmlVideoPreviewGlItem() = default;

QSGNode* QmlVideoPreviewGlItem::updatePaintNode(QSGNode* pOldNode, UpdatePaintNodeData*) {
    return m_pImpl->updatePaintNode(pOldNode);
}

} // namespace qml
} // namespace mixxx
