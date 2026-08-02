#pragma once

#include <QString>

#include "network/jsonwebtask.h"

namespace mixxx {

/// M16: looks up a track's synced lyrics from lrclib.net's free, public,
/// no-auth-required API (https://lrclib.net/api/get) -- a community-
/// submitted database of exact, human-transcribed .lrc lyrics. Tried
/// before falling back to AI transcription (WhisperTranscriptionJob):
/// when a match exists it's both more accurate and free of any of
/// Whisper's inherent ASR limitations (overlapping vocals, word-timing
/// drift, etc.).
class LrcLibGetLyricsTask : public network::JsonWebTask {
    Q_OBJECT
  public:
    LrcLibGetLyricsTask(
            QNetworkAccessManager* pNetworkAccessManager,
            const QString& trackName,
            const QString& artistName,
            const QString& albumName,
            double durationSeconds,
            QObject* pParent = nullptr);
    ~LrcLibGetLyricsTask() override = default;

  signals:
    /// `syncedLyrics` is plain line-level LRC text (lrclib's own format),
    /// ready to write directly to a sidecar .lrc file as-is -- already
    /// exactly what LrcParser (src/library/karaoke/lrcparser.h) expects.
    void succeeded(const QString& syncedLyrics);

  private:
    QNetworkReply* sendNetworkRequest(
            QNetworkAccessManager* pNetworkAccessManager,
            network::HttpRequestMethod method,
            const QUrl& url,
            const QJsonDocument& content) override;

    void onFinished(const network::JsonWebResponse& response) override;

    void emitSucceeded(const QString& syncedLyrics);
};

} // namespace mixxx
