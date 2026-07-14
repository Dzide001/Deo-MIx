#pragma once

#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QString>

#include <memory>

#include "qml/qmltrackproxy.h"
#include "track/track_decl.h"

namespace mixxx {

class StemSeparationManager;

namespace qml {

/// QML-facing entry point for AI stem separation (src/library/stemseparation/).
/// Modeled directly on QmlRecordingProxy: a QML_SINGLETON wrapping a
/// long-lived C++ manager, seeded from outside QML (see qmlapplication.cpp).
class QmlStemSeparationProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(float progress READ progress NOTIFY progressChanged)
    QML_NAMED_ELEMENT(StemSeparation)
    QML_SINGLETON
  public:
    explicit QmlStemSeparationProxy(
            std::shared_ptr<mixxx::StemSeparationManager> pManager,
            QObject* parent = nullptr);
    ~QmlStemSeparationProxy() override = default;

    static QmlStemSeparationProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);

    bool isRunning() const {
        return m_isRunning;
    }

    float progress() const {
        return m_progress;
    }

    /// Starts background AI stem separation for `track`, reloading
    /// `deckGroup` to the result on success. Returns false (nothing
    /// started) if `track` is invalid or StemSeparationManager declines
    /// (model path unset/missing, a job is already running, etc.) --
    /// `failed` fires with an explanatory message in that case too.
    Q_INVOKABLE bool prepareStems(
            const mixxx::qml::QmlTrackProxy* track, const QString& deckGroup);

    static inline std::shared_ptr<mixxx::StemSeparationManager> s_pStemSeparationManager;

  signals:
    void runningChanged();
    void progressChanged();
    void failed(const QString& message);

  private slots:
    void slotProgressChanged(float fraction, const QString& message);
    void slotFinished(TrackPointer pNewStemTrack);
    void slotFailed(const QString& message);

  private:
    std::shared_ptr<mixxx::StemSeparationManager> m_pManager;
    bool m_isRunning = false;
    float m_progress = 0.0f;
};

} // namespace qml
} // namespace mixxx
