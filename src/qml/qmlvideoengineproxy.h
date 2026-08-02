#pragma once

#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <memory>

#include "library/videoengine/videoenginemanager.h"

class ControlProxy;

namespace mixxx {
namespace qml {

/// M12 Stage 3b/3d: thin QML-facing wrapper around the "[VideoEngine]"/
/// "enabled" ControlObject that mixxx::VideoEngineManager (src/library/
/// videoengine/) registers when built with -DVIDEO_ENGINE=ON. The
/// enabled/available properties are deliberately lightweight (no
/// GStreamer dependency of their own) -- just read/write one CO, the same
/// way QML settings pages already read/write other master-bus toggles.
/// Always compiled and registered regardless of VIDEO_ENGINE (matching
/// QmlStemSeparationProxy's convention): when the app wasn't built with
/// VIDEO_ENGINE, the underlying CO simply doesn't exist and available()
/// reports false.
///
/// Stage 3d's loadVideo() is different in kind (a file path per deck isn't
/// something a numeric ControlObject can carry), so it forwards to the
/// real VideoEngineManager via a static shared_ptr seeded from
/// qmlapplication.cpp, mirroring QmlStemSeparationProxy's pattern exactly.
/// VideoEngineManager's header has no VIDEO_ENGINE-conditional content, so
/// this class compiles fine either way; the seeding call itself is the
/// only place that's #ifdef __VIDEO_ENGINE__-gated (qmlapplication.cpp).
/// When unseeded, loadVideo() simply returns false.
class QmlVideoEngineProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ isAvailable CONSTANT)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    /// M12 Stage 6: true only when this build was actually compiled with
    /// -DVIDEO_ENGINE_NDI_OUTPUT=ON -- a compile-time constant, not
    /// runtime state, so QML can decide whether to show any NDI-related
    /// UI at all (matching `available`'s role for the base video engine).
    Q_PROPERTY(bool ndiOutputAvailable READ isNdiOutputAvailable CONSTANT)
    QML_NAMED_ELEMENT(VideoEngine)
    QML_SINGLETON
  public:
    explicit QmlVideoEngineProxy(QObject* parent = nullptr);
    ~QmlVideoEngineProxy() override;

    static QmlVideoEngineProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);

    bool isAvailable() const;
    bool isNdiOutputAvailable() const {
#ifdef __VIDEO_ENGINE_NDI_OUTPUT__
        return true;
#else
        return false;
#endif
    }
    bool isEnabled() const;
    void setEnabled(bool enabled);

    /// Loads a video clip for the given deck group ("[Channel1]" or
    /// "[Channel2]"). Returns false if the app wasn't built with
    /// VIDEO_ENGINE, the deck group isn't recognized, or the underlying
    /// pipeline failed to (re)build.
    Q_INVOKABLE bool loadVideo(const QString& deckGroup, const QString& filePath);

    /// M12 Stage 3y: switches a deck's compositor branch to a live macOS
    /// camera feed instead of its loaded clip (or back, when enabled is
    /// false). See VideoEngineManager::setCameraSource() for what this
    /// does and doesn't change about the crossfade/compositor model.
    Q_INVOKABLE bool setCameraSource(const QString& deckGroup, bool enabled);

    /// M12 Stage 6: starts or stops advertising the composited preview as
    /// an NDI network source named `sourceName` (not per-deck -- this is
    /// the same composite the in-app preview window shows). Always
    /// declared/callable from QML regardless of build flags, matching
    /// loadVideo()'s convention: returns false (rather than not existing)
    /// when this build wasn't compiled with -DVIDEO_ENGINE_NDI_OUTPUT=ON,
    /// or if the underlying NDI SDK/runtime isn't available on this
    /// machine. See VideoEngineManager::enableNdiOutput()/NdiOutputSender.
    Q_INVOKABLE bool enableNdiOutput(bool enabled, const QString& sourceName);

    static inline std::shared_ptr<mixxx::VideoEngineManager> s_pVideoEngineManager;

  signals:
    void enabledChanged();

  private:
    ControlProxy* m_pEnabledControl;
};

} // namespace qml
} // namespace mixxx
