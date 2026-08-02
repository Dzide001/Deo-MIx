#include "library/lyrictranscription/whisperlrcbuilder.h"

#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

namespace mixxx {

QString WhisperLrcBuilder::formatLrcTimestamp(double seconds) {
    if (seconds < 0) {
        seconds = 0;
    }
    const int minutes = static_cast<int>(seconds) / 60;
    const double remainderSeconds = seconds - minutes * 60;
    return QStringLiteral("%1:%2")
            .arg(minutes, 2, 10, QChar('0'))
            .arg(remainderSeconds, 5, 'f', 2, QChar('0'));
}

QString WhisperLrcBuilder::buildEnhancedLrc(const std::vector<WhisperWordTiming>& words) {
    QString lrc;
    QTextStream stream(&lrc);

    size_t i = 0;
    while (i < words.size()) {
        size_t lineEnd = i + 1;
        while (lineEnd < words.size() && static_cast<int>(lineEnd - i) < kMaxWordsPerLine) {
            const double gap = words[lineEnd].startSeconds - words[lineEnd - 1].endSeconds;
            if (gap >= kLineBreakGapSeconds) {
                break;
            }
            lineEnd++;
        }

        stream << "[" << formatLrcTimestamp(words[i].startSeconds) << "]";
        for (size_t w = i; w < lineEnd; w++) {
            stream << "<" << formatLrcTimestamp(words[w].startSeconds) << ">"
                   << words[w].text.trimmed();
            if (w + 1 < lineEnd) {
                stream << " ";
            }
        }
        stream << "\n";

        i = lineEnd;
    }

    return lrc;
}

std::vector<WhisperWordTiming> WhisperLrcBuilder::splitSegmentIntoWords(
        double startSeconds, double endSeconds, const QString& segmentText) {
    std::vector<WhisperWordTiming> result;
    const QStringList words = segmentText.split(
            QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    result.reserve(static_cast<size_t>(words.size()));
    for (const QString& word : words) {
        result.push_back(WhisperWordTiming{startSeconds, endSeconds, word});
    }
    return result;
}

} // namespace mixxx
