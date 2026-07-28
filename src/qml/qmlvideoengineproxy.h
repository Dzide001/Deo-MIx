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
    QML_NAMED_ELEMENT(VideoEngine)
    QML_SINGLETON
  public:
    explicit QmlVideoEngineProxy(QObject* parent = nullptr);
    ~QmlVideoEngineProxy() override;

    static QmlVideoEngineProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);

    bool isAvailable() const;
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

    static inline std::shared_ptr<mixxx::VideoEngineManager> s_pVideoEngineManager;

  signals:
    void enabledChanged();

  private:
    ControlProxy* m_pEnabledControl;
};

} // namespace qml
} // namespace mixxx
