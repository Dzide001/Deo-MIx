#pragma once

#include <QQuickPaintedItem>
#include <QString>
#include <memory>

#include "library/karaoke/karaokemanager.h"

namespace mixxx {
namespace qml {

/// M14 Stage 3: renders the current CD+G ("CDG") canvas frame for a given
/// deck, pulled from the real mixxx::KaraokeManager (loaded/decoded via
/// its normal sidecar-file detection) and polled on a QTimer -- the exact
/// same shape as M12's QmlVideoPreviewItem, just reading a per-deck
/// decoded-image source instead of a live compositor pipeline. `group`
/// and `positionSeconds` are plain QML-settable properties rather than
/// this item owning its own ControlProxy objects: the caller (
/// LyricDisplay.qml) already reads `duration`/`playposition` for the
/// text-based (LRC) lyric path, so this reuses that same computed value
/// rather than duplicating the ControlProxy wiring in C++.
class QmlCdgPreviewItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(CdgPreview)
    Q_PROPERTY(QString group MEMBER m_group NOTIFY groupChanged)
    Q_PROPERTY(double positionSeconds MEMBER m_positionSeconds NOTIFY positionSecondsChanged)

  public:
    explicit QmlCdgPreviewItem(QQuickItem* pParent = nullptr);
    ~QmlCdgPreviewItem() override;

    void paint(QPainter* pPainter) override;

    static inline std::shared_ptr<mixxx::KaraokeManager> s_pKaraokeManager;

  signals:
    void groupChanged();
    void positionSecondsChanged();

  private:
    class Impl;
    std::unique_ptr<Impl> m_pImpl;
    QString m_group;
    double m_positionSeconds = 0.0;
};

} // namespace qml
} // namespace mixxx
