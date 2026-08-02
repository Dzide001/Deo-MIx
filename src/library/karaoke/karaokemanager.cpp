#include "library/karaoke/karaokemanager.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include "control/controlproxy.h"
#include "library/karaoke/cdgdecoder.h"
#include "library/karaoke/lrcparser.h"
#include "mixer/playerinfo.h"
#include "moc_karaokemanager.cpp"
#include "track/track.h"
#include "util/logger.h"

namespace mixxx {

namespace {
const mixxx::Logger kLogger("KaraokeManager");

// Same base filename as the track, same folder -- the subtitle-file
// convention this format already follows elsewhere (e.g. video players
// pairing a .srt with a .mp4 of the same name).
QString sidecarPathFor(const QString& trackFilePath, const QString& extension) {
    const QFileInfo trackFileInfo(trackFilePath);
    return trackFileInfo.absolutePath() + QChar('/') +
            trackFileInfo.completeBaseName() + QChar('.') + extension;
}
} // namespace

KaraokeManager::KaraokeManager(QObject* pParent)
        : QObject(pParent) {
    connect(&PlayerInfo::instance(),
            &PlayerInfo::trackChanged,
            this,
            &KaraokeManager::slotTrackChanged);
}

KaraokeManager::~KaraokeManager() = default;

bool KaraokeManager::hasLyrics(const QString& deckGroup) const {
    const auto it = m_sourceByDeck.find(deckGroup);
    return it != m_sourceByDeck.end() && it.value()->isValid();
}

QString KaraokeManager::currentLine(const QString& deckGroup, double positionSeconds) const {
    const auto it = m_sourceByDeck.find(deckGroup);
    if (it == m_sourceByDeck.end() || !it.value()->isValid()) {
        return QString();
    }
    return it.value()->currentLine(positionSeconds);
}

int KaraokeManager::currentWordIndex(const QString& deckGroup, double positionSeconds) const {
    const auto it = m_sourceByDeck.find(deckGroup);
    if (it == m_sourceByDeck.end() || !it.value()->isValid()) {
        return -1;
    }
    auto* pLrcSource = dynamic_cast<LrcLyricsSource*>(it.value().get());
    if (!pLrcSource) {
        return -1;
    }
    return pLrcSource->currentWordIndex(positionSeconds);
}

bool KaraokeManager::hasCdgSource(const QString& deckGroup) const {
    const auto it = m_sourceByDeck.find(deckGroup);
    if (it == m_sourceByDeck.end() || !it.value()->isValid()) {
        return false;
    }
    return dynamic_cast<CdgLyricsSource*>(it.value().get()) != nullptr;
}

QImage KaraokeManager::currentCdgFrame(const QString& deckGroup, double positionSeconds) const {
    const auto it = m_sourceByDeck.find(deckGroup);
    if (it == m_sourceByDeck.end() || !it.value()->isValid()) {
        return QImage();
    }
    auto* pCdgSource = dynamic_cast<CdgLyricsSource*>(it.value().get());
    if (!pCdgSource) {
        return QImage();
    }
    return pCdgSource->currentFrame(positionSeconds);
}

void KaraokeManager::setKaraokeModeEnabled(bool enabled) {
    if (enabled == m_karaokeModeEnabled) {
        return;
    }
    m_karaokeModeEnabled = enabled;
    emit karaokeModeEnabledChanged();
}

void KaraokeManager::maybeAutoMuteVocals(const QString& deckGroup) {
    if (!m_karaokeModeEnabled) {
        return;
    }
    ControlProxy stemInstrumental(
            deckGroup, QStringLiteral("stem_instrumental"), nullptr, ControlFlag::AllowMissingOrInvalid);
    if (!stemInstrumental.valid()) {
        // No stems on this track (or this build has no stem support at
        // all) -- nothing to mute, not an error.
        return;
    }
    stemInstrumental.set(1.0);
}

void KaraokeManager::reloadLyricsForDeck(const QString& deckGroup) {
    const TrackPointer pTrack = PlayerInfo::instance().getTrackInfo(deckGroup);
    if (!pTrack) {
        return;
    }
    loadLyricsFor(deckGroup, pTrack->getLocation());
}

void KaraokeManager::slotTrackChanged(
        const QString& group, TrackPointer pNewTrack, TrackPointer pOldTrack) {
    Q_UNUSED(pOldTrack);
    if (!pNewTrack) {
        m_sourceByDeck.remove(group);
        return;
    }
    loadLyricsFor(group, pNewTrack->getLocation());
}

void KaraokeManager::loadLyricsFor(const QString& deckGroup, const QString& trackFilePath) {
    m_sourceByDeck.remove(deckGroup);

    if (trackFilePath.isEmpty()) {
        return;
    }

    const QString lrcPath = sidecarPathFor(trackFilePath, QStringLiteral("lrc"));
    QFile lrcFile(lrcPath);
    if (lrcFile.exists() && lrcFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&lrcFile);
        const QString contents = stream.readAll();
        lrcFile.close();

        auto pSource = std::make_shared<LrcLyricsSource>(LrcParser::parse(contents));
        if (pSource->isValid()) {
            kLogger.debug() << "Loaded LRC lyrics for" << deckGroup << "from" << lrcPath;
            m_sourceByDeck[deckGroup] = std::move(pSource);
            maybeAutoMuteVocals(deckGroup);
            return;
        }
        kLogger.debug() << "LRC file" << lrcPath << "had no usable timed lines";
    }

    // No usable .lrc -- fall back to the legacy CD+G/MP3+G disc-rip
    // format (a .cdg pair next to the track, same sidecar convention).
    const QString cdgPath = sidecarPathFor(trackFilePath, QStringLiteral("cdg"));
    QFile cdgFile(cdgPath);
    if (cdgFile.exists() && cdgFile.open(QIODevice::ReadOnly)) {
        const QByteArray contents = cdgFile.readAll();
        cdgFile.close();

        auto pSource = std::make_shared<CdgLyricsSource>(CdgDecoder::decode(contents));
        if (pSource->isValid()) {
            kLogger.debug() << "Loaded CD+G lyrics for" << deckGroup << "from" << cdgPath;
            m_sourceByDeck[deckGroup] = std::move(pSource);
            maybeAutoMuteVocals(deckGroup);
            return;
        }
        kLogger.debug() << "CD+G file" << cdgPath << "produced no usable frames";
    }
}

} // namespace mixxx
