#pragma once
#include <QAbstractListModel>
#include <memory>

#include "track/steminfo.h"

namespace mixxx {
namespace qml {

class QmlStemsModel : public QAbstractListModel {
    Q_OBJECT
    // QAbstractListModel's rowCount()/get() are plain methods, not
    // NOTIFY-bound properties, so a QML binding that only calls them (e.g.
    // StemPads.qml's findStemIndex()) never gets re-evaluated when
    // setStems() repopulates the model later -- it silently keeps whatever
    // stale result it saw the first time. This property exists purely so
    // such bindings have something with a real NOTIFY signal to depend on.
    Q_PROPERTY(int stemCount READ stemCount NOTIFY stemsChanged)
  public:
    enum Roles {
        LabelRole,
        ColorRole,
    };
    Q_ENUM(Roles)
    explicit QmlStemsModel(QObject* pParent = nullptr);

    void setStems(QList<StemInfo> stems);

    QVariant data(const QModelIndex& index, int role) const override;
    int rowCount(const QModelIndex& parent) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QVariant get(int row) const;

    int stemCount() const {
        return m_stems.size();
    }

  signals:
    void stemsChanged();

  private:
    QList<StemInfo> m_stems;
};

} // namespace qml
} // namespace mixxx
