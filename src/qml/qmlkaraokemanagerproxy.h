#pragma once

#include <QImage>
#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <memory>

#include "library/karaoke/karaokemanager.h"
#include "qml/qmlsingerqueuemodel.h"

namespace mixxx {
namespace qml {

/// M14: thin QML-facing wrapper around the real mixxx::KaraokeManager,
/// mirroring QmlVideoEngineProxy's pattern exactly -- a static shared_ptr
/// seeded from qmlapplication.cpp (CoreServices owns the real manager;
/// this singleton is just how QML reaches it), since a QML_SINGLETON's own
/// factory-created lifecycle isn't the right place to own something that
/// needs to be constructed at a specific point in CoreServices'
/// initialization order and torn down before PlayerManager/
/// TrackCollectionManager are.
class QmlKaraokeManagerProxy : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(KaraokeManager)
    QML_SINGLETON

  public:
    explicit QmlKaraokeManagerProxy(QObject* parent = nullptr);

    static QmlKaraokeManagerProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);

    /// True if `deckGroup` (e.g. "[Channel1]") currently has a usable
    /// lyrics source loaded (a sidecar file was found and parsed
    /// successfully).
    Q_INVOKABLE bool hasLyrics(const QString& deckGroup) const;

    /// The lyric line that should be showing on this deck right now,
    /// given its current playback position in seconds. Returns an empty
    /// string if there's no lyrics source or no line is active yet.
    Q_INVOKABLE QString currentLine(const QString& deckGroup, double positionSeconds) const;

    /// M14 Stage 2: the index of the word (within currentLine()'s text,
    /// split on single spaces) that should currently be highlighted, or
    /// -1 if this deck's source has no word-level (enhanced LRC) timing
    /// for the active line.
    Q_INVOKABLE int currentWordIndex(const QString& deckGroup, double positionSeconds) const;

    /// M14 Stage 3: true if `deckGroup`'s current lyrics source is a
    /// decoded .cdg file (as opposed to an .lrc one, or none).
    Q_INVOKABLE bool hasCdgSource(const QString& deckGroup) const;

    /// M14 Stage 4: whether karaoke mode is currently on -- gates
    /// auto-mute-on-load in KaraokeManager and, in QML, whether the
    /// second-screen KaraokeDisplayWindow/SingerQueuePanel are shown.
    Q_PROPERTY(bool karaokeModeEnabled READ isKaraokeModeEnabled WRITE
                    setKaraokeModeEnabled NOTIFY karaokeModeEnabledChanged)
    bool isKaraokeModeEnabled() const;
    void setKaraokeModeEnabled(bool enabled);

    /// M14 Stage 5: the singer queue -- owned by this singleton (not
    /// KaraokeManager itself, since it's pure QML-facing session state
    /// with no engine/track involvement at all, unlike everything else
    /// this proxy forwards to the real C++ manager).
    Q_PROPERTY(QObject* singerQueue READ singerQueue CONSTANT)
    QObject* singerQueue() const {
        return m_pSingerQueue;
    }

    /// M14 Stage 6: which physical screen (index into Qt.application.
    /// screens in QML) the second-screen KaraokeDisplayWindow should show
    /// on. Pure UI preference -- lives here rather than on the real
    /// mixxx::KaraokeManager since it has no engine/track involvement at
    /// all, unlike everything else this proxy forwards. Persisted to
    /// mixxx.cfg (it used to be a plain in-memory member, so the choice
    /// was silently lost on every restart) via QmlConfigProxy's static
    /// UserSettings accessor, the same route QmlCustomPadSettingsProxy/
    /// QmlLibraryColumnSettingsProxy already use for their own QML-only
    /// persisted state.
    Q_PROPERTY(int karaokeDisplayScreenIndex READ karaokeDisplayScreenIndex WRITE
                    setKaraokeDisplayScreenIndex NOTIFY karaokeDisplayScreenIndexChanged)
    int karaokeDisplayScreenIndex() const {
        return m_karaokeDisplayScreenIndex;
    }
    void setKaraokeDisplayScreenIndex(int index);

    static inline std::shared_ptr<mixxx::KaraokeManager> s_pKaraokeManager;

  signals:
    void karaokeModeEnabledChanged();
    void karaokeDisplayScreenIndexChanged();

  private:
    QmlSingerQueueModel* m_pSingerQueue;
    int m_karaokeDisplayScreenIndex = 0;
};

} // namespace qml
} // namespace mixxx
