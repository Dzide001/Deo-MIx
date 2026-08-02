#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QQmlEngine>
#include <QString>

namespace mixxx {
namespace qml {

/// M14 Stage 5: the karaoke singer queue -- an ordered list of {singer
/// name, song request, status}, modeled directly on QmlCuesModel's shape
/// (enum Roles, a plain QList<T> backing store, get(row) for convenient
/// QML property access). Pure in-memory, session-scoped: the spec only
/// asks for "persists through the session," not across restarts, and
/// ConfigObject (the pattern used elsewhere in this project for small
/// persisted settings) has no native array/struct support that would fit
/// an ordered list of records without hand-rolled serialization -- not
/// worth it for something explicitly scoped to session lifetime.
class QmlSingerQueueModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(SingerQueueModel)
    QML_UNCREATABLE("Only accessible via Mixxx.KaraokeManager.singerQueue")

  public:
    enum Roles {
        SingerNameRole = Qt::UserRole + 1,
        SongRequestRole,
        StatusRole,
    };
    Q_ENUM(Roles)

    /// "Skipped" is distinct from "Done" (spec's "mark as sung/skip" are
    /// two separate actions) so a future history view can tell an actual
    /// performance apart from an entry the DJ just cleared out.
    enum class Status {
        Waiting = 0,
        Performing = 1,
        Done = 2,
        Skipped = 3,
    };
    Q_ENUM(Status)

    explicit QmlSingerQueueModel(QObject* pParent = nullptr);

    QVariant data(const QModelIndex& index, int role) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QVariant get(int row) const;

    Q_INVOKABLE void addEntry(const QString& singerName, const QString& songRequest);
    Q_INVOKABLE void removeEntry(int row);
    Q_INVOKABLE void moveEntry(int from, int to);

    /// Marks `row` as the one currently performing -- any other entry
    /// previously marked Performing reverts to Waiting first, since only
    /// one singer performs at a time.
    Q_INVOKABLE void markPerforming(int row);
    Q_INVOKABLE void markDone(int row);
    Q_INVOKABLE void markSkipped(int row);

    /// The row of the first still-Waiting entry in queue order, or -1 if
    /// none -- "up next" is this computed position, not a stored flag per
    /// entry.
    Q_INVOKABLE int nextWaitingRow() const;

  private:
    struct Entry {
        QString singerName;
        QString songRequest;
        Status status = Status::Waiting;
    };

    QList<Entry> m_entries;
};

} // namespace qml
} // namespace mixxx
