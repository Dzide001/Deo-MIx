#include "library/karaoke/lrcparser.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <utility>

namespace mixxx {

namespace {

const QRegularExpression kTimestampTag(QStringLiteral(R"(^\[(\d+):(\d+)(?:\.(\d+))?\])"));
const QRegularExpression kWordTag(QStringLiteral(R"(<(\d+):(\d+)(?:\.(\d+))?>)"));

// Returns a negative value if minutes/seconds couldn't be parsed as numbers.
double parseTimestampSeconds(const QString& minutes, const QString& seconds, const QString& fraction) {
    bool minutesOk = false;
    bool secondsOk = false;
    const int minutesValue = minutes.toInt(&minutesOk);
    const double secondsValue = seconds.toDouble(&secondsOk);
    if (!minutesOk || !secondsOk) {
        return -1;
    }
    double fractionValue = 0.0;
    if (!fraction.isEmpty()) {
        // ".67" -> 0.67s, ".675" -> 0.675s -- LRC files in the wild use
        // either centisecond or millisecond precision, so this is derived
        // from however many digits were actually present rather than
        // assumed.
        fractionValue = fraction.toDouble() / std::pow(10.0, fraction.length());
    }
    return minutesValue * 60.0 + secondsValue + fractionValue;
}

// M14 Stage 2: `rawText` is the portion of an LRC line after its own
// leading [mm:ss.xx] line timestamp(s) have already been stripped. If it
// contains enhanced-LRC inline <mm:ss.xx> word tags (absolute timestamps,
// same clock as the line-level ones), splits it into plain text + one
// timestamp per word. Otherwise `text` is just the trimmed input and
// `wordTimestamps` stays empty -- a plain line-level-only file never
// reaches the word-tag branch at all.
void parseWordTags(const QString& rawText, QString* pText, QList<double>* pWordTimestamps) {
    QList<QRegularExpressionMatch> matches;
    QRegularExpressionMatchIterator it = kWordTag.globalMatch(rawText);
    while (it.hasNext()) {
        matches.append(it.next());
    }
    if (matches.isEmpty()) {
        *pText = rawText.trimmed();
        return;
    }

    QStringList words;
    QList<double> timestamps;
    for (int i = 0; i < matches.size(); i++) {
        const QRegularExpressionMatch& match = matches[i];
        const double timestamp = parseTimestampSeconds(
                match.captured(1), match.captured(2), match.captured(3));
        if (timestamp < 0) {
            continue;
        }
        const int chunkStart = match.capturedEnd(0);
        const int chunkEnd = (i + 1 < matches.size())
                ? matches[i + 1].capturedStart(0)
                : rawText.length();
        const QString chunk = rawText.mid(chunkStart, chunkEnd - chunkStart).trimmed();
        if (chunk.isEmpty()) {
            // A word tag with no text after it (or immediately followed by
            // the next tag) -- nothing to highlight, skip rather than
            // inserting an empty word that would desync word indices from
            // the rendered (space-joined) text.
            continue;
        }
        words.append(chunk);
        timestamps.append(timestamp);
    }
    *pText = words.join(QChar(' '));
    *pWordTimestamps = timestamps;
}

} // namespace

QList<LrcLine> LrcParser::parse(const QString& fileContents) {
    QList<LrcLine> lines;

    const QStringList rawLines = fileContents.split(QRegularExpression(QStringLiteral("\r\n|\n|\r")));
    for (QString rawLine : rawLines) {
        QList<double> timestamps;
        while (true) {
            // Tolerates stray whitespace between/around consecutive tags
            // (e.g. "[00:12.00] [00:45.00]text") without affecting
            // mid-string whitespace in the actual lyric text, which is
            // never touched here.
            rawLine = rawLine.trimmed();
            const QRegularExpressionMatch match = kTimestampTag.match(rawLine);
            if (!match.hasMatch()) {
                break;
            }
            const double timestamp = parseTimestampSeconds(
                    match.captured(1), match.captured(2), match.captured(3));
            if (timestamp >= 0) {
                timestamps.append(timestamp);
            }
            rawLine.remove(0, match.captured(0).length());
        }
        if (timestamps.isEmpty()) {
            // No recognized timestamp tag at all -- either metadata
            // ([ar:...], [ti:...], [offset:...], etc.) or a malformed
            // line. Skip it rather than erroring.
            continue;
        }

        QString text;
        QList<double> wordTimestamps;
        parseWordTags(rawLine, &text, &wordTimestamps);

        for (double timestamp : timestamps) {
            lines.append(LrcLine{timestamp, text, wordTimestamps});
        }
    }

    std::sort(lines.begin(), lines.end(), [](const LrcLine& a, const LrcLine& b) {
        return a.timestampSeconds < b.timestampSeconds;
    });
    return lines;
}

LrcLyricsSource::LrcLyricsSource(QList<LrcLine> lines) : m_lines(std::move(lines)) {
}

bool LrcLyricsSource::isValid() const {
    return !m_lines.isEmpty();
}

int LrcLyricsSource::findCurrentLineIndex(double positionSeconds) const {
    // m_lines is sorted ascending -- the line to show is the last one
    // whose timestamp has already passed.
    //
    // M15b: binary search rather than the original scan from index 0.
    // This is the hottest call in the karaoke path: the QML poll timers
    // (LyricDisplay.qml and KaraokeDisplayWindow.qml) call currentLine()
    // AND currentWordIndex() every tick, and each of those calls this --
    // so it ran up to 4x per tick, each time walking the whole lyric
    // sheet from the beginning. Cost grew with lyric length and with how
    // far into the track playback was, exactly backwards from what you
    // want. The data was already sorted, so the scan was pure waste.
    const auto it = std::upper_bound(m_lines.cbegin(),
            m_lines.cend(),
            positionSeconds,
            [](double position, const LrcLine& line) {
                return position < line.timestampSeconds;
            });
    // upper_bound lands on the first line AFTER the current one, so the
    // line to show is the one before it -- and cbegin() means nothing has
    // started yet (-1, "no line").
    return static_cast<int>(std::distance(m_lines.cbegin(), it)) - 1;
}

QString LrcLyricsSource::currentLine(double positionSeconds) const {
    const int index = findCurrentLineIndex(positionSeconds);
    return index < 0 ? QString() : m_lines[index].text;
}

int LrcLyricsSource::currentWordIndex(double positionSeconds) const {
    const int lineIndex = findCurrentLineIndex(positionSeconds);
    if (lineIndex < 0) {
        return -1;
    }
    const QList<double>& wordTimestamps = m_lines[lineIndex].wordTimestamps;
    int current = -1;
    for (int i = 0; i < wordTimestamps.size(); i++) {
        if (wordTimestamps[i] > positionSeconds) {
            break;
        }
        current = i;
    }
    return current;
}

} // namespace mixxx
