#include "qml/qmlsingerqueuemodel.h"

#include "moc_qmlsingerqueuemodel.cpp"

namespace mixxx {
namespace qml {

namespace {
const QHash<int, QByteArray> kRoleNames = {
        {QmlSingerQueueModel::SingerNameRole, "singerName"},
        {QmlSingerQueueModel::SongRequestRole, "songRequest"},
        {QmlSingerQueueModel::StatusRole, "status"},
};
} // namespace

QmlSingerQueueModel::QmlSingerQueueModel(QObject* pParent)
        : QAbstractListModel(pParent) {
}

QVariant QmlSingerQueueModel::data(const QModelIndex& index, int role) const {
    if (index.row() < 0 || index.row() >= m_entries.size()) {
        return QVariant();
    }
    const Entry& entry = m_entries.at(index.row());
    switch (role) {
    case SingerNameRole:
        return entry.singerName;
    case SongRequestRole:
        return entry.songRequest;
    case StatusRole:
        return static_cast<int>(entry.status);
    default:
        return QVariant();
    }
}

int QmlSingerQueueModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_entries.size();
}

QHash<int, QByteArray> QmlSingerQueueModel::roleNames() const {
    return kRoleNames;
}

QVariant QmlSingerQueueModel::get(int row) const {
    const QModelIndex idx = index(row, 0);
    QVariantMap dataMap;
    for (auto it = kRoleNames.constBegin(); it != kRoleNames.constEnd(); it++) {
        dataMap.insert(it.value(), data(idx, it.key()));
    }
    return dataMap;
}

void QmlSingerQueueModel::addEntry(const QString& singerName, const QString& songRequest) {
    const int row = m_entries.size();
    beginInsertRows(QModelIndex(), row, row);
    m_entries.append(Entry{singerName, songRequest, Status::Waiting});
    endInsertRows();
}

void QmlSingerQueueModel::removeEntry(int row) {
    if (row < 0 || row >= m_entries.size()) {
        return;
    }
    beginRemoveRows(QModelIndex(), row, row);
    m_entries.removeAt(row);
    endRemoveRows();
}

void QmlSingerQueueModel::moveEntry(int from, int to) {
    if (from < 0 || from >= m_entries.size() || to < 0 || to >= m_entries.size() ||
            from == to) {
        return;
    }
    // Qt's beginMoveRows() destination convention: when moving a row
    // further down the list, the destination index must be one past the
    // target slot in the ORIGINAL (pre-move) indexing.
    const int destinationRow = (to > from) ? to + 1 : to;
    if (!beginMoveRows(QModelIndex(), from, from, QModelIndex(), destinationRow)) {
        return;
    }
    m_entries.move(from, to);
    endMoveRows();
}

void QmlSingerQueueModel::markPerforming(int row) {
    if (row < 0 || row >= m_entries.size()) {
        return;
    }
    // Only one singer performs at a time -- any other entry currently
    // marked Performing reverts to Waiting first.
    for (int i = 0; i < m_entries.size(); i++) {
        if (i != row && m_entries[i].status == Status::Performing) {
            m_entries[i].status = Status::Waiting;
            const QModelIndex changedIdx = index(i, 0);
            emit dataChanged(changedIdx, changedIdx, {StatusRole});
        }
    }
    m_entries[row].status = Status::Performing;
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {StatusRole});
}

void QmlSingerQueueModel::markDone(int row) {
    if (row < 0 || row >= m_entries.size()) {
        return;
    }
    m_entries[row].status = Status::Done;
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {StatusRole});
}

void QmlSingerQueueModel::markSkipped(int row) {
    if (row < 0 || row >= m_entries.size()) {
        return;
    }
    m_entries[row].status = Status::Skipped;
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {StatusRole});
}

int QmlSingerQueueModel::nextWaitingRow() const {
    for (int i = 0; i < m_entries.size(); i++) {
        if (m_entries[i].status == Status::Waiting) {
            return i;
        }
    }
    return -1;
}

} // namespace qml
} // namespace mixxx
