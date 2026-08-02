#include "library/lyrictranscription/lrclibgetlyricstask.h"

#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrlQuery>

#include "moc_lrclibgetlyricstask.cpp"
#include "network/httpstatuscode.h"
#include "util/assert.h"
#include "util/logger.h"
#include "util/versionstore.h"

namespace mixxx {

namespace {

const Logger kLogger("LrcLibGetLyricsTask");

const QUrl kBaseUrl = QStringLiteral("https://lrclib.net/");
const QString kRequestPath = QStringLiteral("/api/get");

const QByteArray kUserAgentRawHeaderKey = "User-Agent";

QString userAgentRawHeaderValue() {
    return VersionStore::applicationName() + QStringLiteral("/") + VersionStore::version();
}

network::JsonWebRequest lookupRequest(
        const QString& trackName,
        const QString& artistName,
        const QString& albumName,
        double durationSeconds) {
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("track_name"), trackName);
    query.addQueryItem(QStringLiteral("artist_name"), artistName);
    if (!albumName.isEmpty()) {
        query.addQueryItem(QStringLiteral("album_name"), albumName);
    }
    query.addQueryItem(QStringLiteral("duration"), QString::number(qRound(durationSeconds)));
    return network::JsonWebRequest{
            network::HttpRequestMethod::Get,
            kRequestPath,
            query,
            QJsonDocument(),
    };
}

} // anonymous namespace

LrcLibGetLyricsTask::LrcLibGetLyricsTask(
        QNetworkAccessManager* pNetworkAccessManager,
        const QString& trackName,
        const QString& artistName,
        const QString& albumName,
        double durationSeconds,
        QObject* pParent)
        : network::JsonWebTask(
                  pNetworkAccessManager,
                  kBaseUrl,
                  lookupRequest(trackName, artistName, albumName, durationSeconds),
                  pParent) {
}

QNetworkReply* LrcLibGetLyricsTask::sendNetworkRequest(
        QNetworkAccessManager* pNetworkAccessManager,
        network::HttpRequestMethod method,
        const QUrl& url,
        const QJsonDocument& content) {
    Q_UNUSED(content);
    DEBUG_ASSERT(pNetworkAccessManager);
    DEBUG_ASSERT(method == network::HttpRequestMethod::Get);

    QNetworkRequest networkRequest(url);
    // Polite/conventional for a free public API -- same reasoning as
    // MusicBrainzRecordingsTask's own User-Agent header.
    networkRequest.setRawHeader(kUserAgentRawHeaderKey, userAgentRawHeaderValue().toLatin1());

    if (kLogger.traceEnabled()) {
        kLogger.trace() << "GET" << url;
    }
    return pNetworkAccessManager->get(networkRequest);
}

void LrcLibGetLyricsTask::onFinished(const network::JsonWebResponse& response) {
    if (!response.isStatusCodeSuccess()) {
        // 404 ("TrackNotFound") is the expected/common case here, not a
        // real error -- the caller (WhisperTranscriptionManager) treats
        // this failed signal as "no free lyrics available, fall back to
        // AI transcription", not something to surface to the user.
        emitFailed(response);
        return;
    }
    VERIFY_OR_DEBUG_ASSERT(response.content().isObject()) {
        kLogger.warning() << "Invalid JSON content" << response.content();
        emitFailed(response);
        return;
    }
    const auto jsonObject = response.content().object();
    const bool instrumental = jsonObject.value(QLatin1String("instrumental")).toBool(false);
    const QString syncedLyrics = jsonObject.value(QLatin1String("syncedLyrics")).toString();
    if (instrumental || syncedLyrics.isEmpty()) {
        // lrclib has an entry for this track but no synced lyrics for it
        // (correctly marked instrumental, or only plain/unsynced lyrics
        // were ever submitted) -- same fallback path as a 404.
        emitFailed(response);
        return;
    }
    emitSucceeded(syncedLyrics);
}

void LrcLibGetLyricsTask::emitSucceeded(const QString& syncedLyrics) {
    VERIFY_OR_DEBUG_ASSERT(isSignalFuncConnected(&LrcLibGetLyricsTask::succeeded)) {
        kLogger.warning() << "Unhandled succeeded signal";
        deleteLater();
        return;
    }
    emit succeeded(syncedLyrics);
}

} // namespace mixxx
