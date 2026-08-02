#pragma once

#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QUrl>

#include <memory>

#include "qml/qmltrackproxy.h"
#include "track/track_decl.h"

namespace mixxx {

class WhisperTranscriptionManager;

namespace qml {

/// M16: QML-facing entry point for AI lyric transcription
/// (src/library/lyrictranscription/), modeled directly on
/// QmlStemSeparationProxy -- a QML_SINGLETON wrapping a long-lived C++
/// manager, seeded from outside QML (see qmlapplication.cpp).
class QmlLyricTranscriptionProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(float progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY progressChanged)
    /// Whisper GGML model path, shared with the legacy
    /// DlgPrefAiLyricTranscription page (same config key).
    Q_PROPERTY(QString modelPath READ modelPath WRITE setModelPath NOTIFY modelPathChanged)
    /// Human-readable state of modelPath: "Not set" / "Found" /
    /// "File missing!", mirroring the legacy page's own status label.
    Q_PROPERTY(QString modelStatus READ modelStatus NOTIFY modelPathChanged)
    /// False when the app was built without AI_LYRIC_TRANSCRIPTION (no
    /// manager was ever seeded), so the settings page can say so rather
    /// than silently offering controls that do nothing.
    Q_PROPERTY(bool available READ available CONSTANT)
    QML_NAMED_ELEMENT(LyricTranscription)
    QML_SINGLETON
  public:
    explicit QmlLyricTranscriptionProxy(
            std::shared_ptr<mixxx::WhisperTranscriptionManager> pManager,
            QObject* parent = nullptr);
    ~QmlLyricTranscriptionProxy() override = default;

    static QmlLyricTranscriptionProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);

    bool isRunning() const {
        return m_isRunning;
    }

    float progress() const {
        return m_progress;
    }

    QString statusMessage() const {
        return m_statusMessage;
    }

    bool available() const {
        return static_cast<bool>(m_pManager);
    }

    QString modelPath() const;
    void setModelPath(const QString& path);
    QString modelStatus() const;

    /// Convenience for QML FileDialog, whose selectedFile is a file://
    /// QUrl rather than a plain filesystem path.
    Q_INVOKABLE void setModelPathFromUrl(const QUrl& url);

    /// Starts background AI lyric transcription for `track`, reloading
    /// `deckGroup`'s lyrics display once the resulting .lrc sidecar file
    /// is written. Returns false (nothing started) if `track` is invalid
    /// or WhisperTranscriptionManager declines (model path unset/missing,
    /// a job is already running, etc.) -- `failed` fires with an
    /// explanatory message in that case too.
    Q_INVOKABLE bool transcribeLyrics(
            const mixxx::qml::QmlTrackProxy* track, const QString& deckGroup);

    static inline std::shared_ptr<mixxx::WhisperTranscriptionManager> s_pWhisperTranscriptionManager;

  signals:
    void runningChanged();
    void progressChanged();
    void modelPathChanged();
    void failed(const QString& message);

  private slots:
    void slotProgressChanged(float fraction, const QString& message);
    void slotFinished(const QString& deckGroup, const QString& lrcPath);
    void slotFailed(const QString& message);

  private:
    std::shared_ptr<mixxx::WhisperTranscriptionManager> m_pManager;
    bool m_isRunning = false;
    float m_progress = 0.0f;
    QString m_statusMessage;
};

} // namespace qml
} // namespace mixxx
